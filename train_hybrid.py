"""
HemoGuard Hybrid ML Training Pipeline  (PyTorch)
==================================================
Two-model architecture:
  1. Random Forest  → feature importance weights
  2. LSTM (PyTorch) → time-series classification using RF-weighted features

Outputs:
  - hemoguard_lstm.pt          (PyTorch LSTM state dict)
  - hemoguard_rf_weights.pkl   (Random Forest for feature importances)
  - hemoguard_scaler.pkl       (MinMaxScaler for normalisation)
  - hemoguard_model_meta.json  (metadata: feature names, importances, etc.)
  - hybrid_predictions.png     (predictions vs actuals plot)
  - hybrid_feature_importance.png
"""

import os, json, warnings
import numpy as np
import pandas as pd
import joblib
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import MinMaxScaler
from sklearn.metrics import classification_report, confusion_matrix
from sklearn.utils import resample

import torch
import torch.nn as nn
from torch.utils.data import DataLoader, TensorDataset

warnings.filterwarnings('ignore')

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

# ══════════════════════════════════════════════════════════════════
#  CONFIGURATION
# ══════════════════════════════════════════════════════════════════
FEATURE_COLS = ['HR', 'HR_slope', 'PTT', 'PTT_slope',
                'PPG', 'PPG_slope', 'Imp', 'Imp_slope',
                'Temp', 'Temp_slope']
CLASS_LABELS = ['STABLE', 'WARNING', 'CRITICAL']

SEQ_LEN        = 10
N_SYNTH        = 600
RF_ESTIMATORS  = 500
LSTM_EPOCHS    = 100
LSTM_BATCH     = 32
LSTM_LR        = 0.002
RANDOM_SEED    = 42

np.random.seed(RANDOM_SEED)
torch.manual_seed(RANDOM_SEED)

DEVICE = torch.device('cuda' if torch.cuda.is_available() else 'cpu')

print("=" * 65)
print("  HemoGuard Hybrid ML Pipeline  (LSTM + Random Forest)")
print(f"  PyTorch {torch.__version__}  |  Device: {DEVICE}")
print("=" * 65)


# ══════════════════════════════════════════════════════════════════
#  STEP 1 — DATA LOADING
# ══════════════════════════════════════════════════════════════════
def load_datasets():
    frames = []
    hemo_path = os.path.join(BASE_DIR, 'hemoguard_data.csv')
    if os.path.exists(hemo_path):
        df = pd.read_csv(hemo_path)
        target = 'label' if 'label' in df.columns else 'Label'
        df = df.rename(columns={target: 'label'})
        df[FEATURE_COLS] = df[FEATURE_COLS].apply(pd.to_numeric, errors='coerce')
        df = df.dropna(subset=FEATURE_COLS)
        frames.append(df[FEATURE_COLS + ['label']])
        print(f"[OK] hemoguard_data.csv → {len(df)} rows  |  {df['label'].value_counts().to_dict()}")

    real_path = os.path.join(BASE_DIR, 'real_esp32_data.csv')
    if os.path.exists(real_path):
        df = pd.read_csv(real_path)
        for col in ['timestamp', 'session_id']:
            if col in df.columns:
                df = df.drop(columns=[col])
        target = 'label' if 'label' in df.columns else 'Label'
        df = df.rename(columns={target: 'label'})
        df[FEATURE_COLS] = df[FEATURE_COLS].apply(pd.to_numeric, errors='coerce')
        df = df.dropna(subset=FEATURE_COLS)
        frames.append(df[FEATURE_COLS + ['label']])
        print(f"[OK] real_esp32_data.csv → {len(df)} rows  |  {df['label'].value_counts().to_dict()}")

    if not frames:
        raise FileNotFoundError("No dataset CSV files found!")
    return pd.concat(frames, ignore_index=True)


# ══════════════════════════════════════════════════════════════════
#  STEP 2 — SYNTHETIC TIME-SERIES DATA  (LBNP-based)
#  Generate CONTINUOUS time-series segments per class, not random.
# ══════════════════════════════════════════════════════════════════
def generate_synthetic_timeseries(n_segments=30, seg_len=50):
    """
    Generate continuous time-series segments that simulate
    clinical progressions. Each segment evolves smoothly over time.
    """
    rng = np.random.default_rng(RANDOM_SEED)
    rows = []

    # Clinical parameter ranges per class
    params = {
        0: {'HR': (65, 80, 3), 'PTT': (220, 280, 10), 'PPG': (0.7, 1.2, 0.08),
            'Imp': (40, 55, 4), 'Temp': (36.2, 37.0, 0.2)},
        1: {'HR': (78, 95, 5), 'PTT': (180, 230, 15), 'PPG': (0.4, 0.7, 0.10),
            'Imp': (55, 75, 6), 'Temp': (35.2, 36.0, 0.4)},
        2: {'HR': (95, 130, 8), 'PTT': (130, 180, 18), 'PPG': (0.2, 0.5, 0.12),
            'Imp': (75, 110, 10), 'Temp': (33.5, 35.5, 0.6)},
    }

    for label in [0, 1, 2]:
        p = params[label]
        for _ in range(n_segments):
            # Start from a random point within the class range
            hr = rng.uniform(*p['HR'][:2])
            ptt = rng.uniform(*p['PTT'][:2])
            ppg = rng.uniform(*p['PPG'][:2])
            imp = rng.uniform(*p['Imp'][:2])
            temp = rng.uniform(*p['Temp'][:2])

            for t in range(seg_len):
                # Smooth random walk within class boundaries
                hr  += rng.normal(0, p['HR'][2] * 0.3)
                ptt += rng.normal(0, p['PTT'][2] * 0.3)
                ppg += rng.normal(0, p['PPG'][2] * 0.3)
                imp += rng.normal(0, p['Imp'][2] * 0.3)
                temp += rng.normal(0, p['Temp'][2] * 0.1)

                # Clip to class boundaries with some margin
                hr  = float(np.clip(hr, p['HR'][0]-5, p['HR'][1]+5))
                ptt = float(np.clip(ptt, p['PTT'][0]-10, p['PTT'][1]+10))
                ppg = float(np.clip(ppg, p['PPG'][0]-0.05, p['PPG'][1]+0.05))
                imp = float(np.clip(imp, p['Imp'][0]-5, p['Imp'][1]+5))
                temp = float(np.clip(temp, p['Temp'][0]-0.3, p['Temp'][1]+0.3))

                rows.append({
                    'HR': hr, 'HR_slope': 0.0,
                    'PTT': ptt, 'PTT_slope': 0.0,
                    'PPG': ppg, 'PPG_slope': 0.0,
                    'Imp': imp, 'Imp_slope': 0.0,
                    'Temp': temp, 'Temp_slope': 0.0,
                    'label': label
                })

    df = pd.DataFrame(rows)

    # Compute slopes from the continuous time-series
    for col in ['HR', 'PTT', 'PPG', 'Imp', 'Temp']:
        df[f'{col}_slope'] = df[col].diff().fillna(0) * 0.1

    print(f"[OK] Synthetic time-series → {len(df)} rows  ({n_segments} segments × {seg_len} steps × 3 classes)")
    print(f"     Class distribution: {df['label'].value_counts().to_dict()}")
    return df


# ══════════════════════════════════════════════════════════════════
#  STEP 3 — FEATURE ENGINEERING
# ══════════════════════════════════════════════════════════════════
def engineer_features(df):
    key_vitals = ['HR', 'PTT', 'PPG', 'Imp']
    new_cols = []
    for col in key_vitals:
        rc = f'{col}_roll5'
        df[rc] = df[col].rolling(5, min_periods=1).mean()
        new_cols.append(rc)
        for lag in [1, 2]:
            lc = f'{col}_lag{lag}'
            df[lc] = df[col].shift(lag).fillna(df[col].iloc[0])
            new_cols.append(lc)
    all_feat = FEATURE_COLS + new_cols
    print(f"[OK] Features: {len(FEATURE_COLS)} base → {len(all_feat)} total")
    return df, all_feat


# ══════════════════════════════════════════════════════════════════
#  STEP 4 — RANDOM FOREST  (Feature Importance)
# ══════════════════════════════════════════════════════════════════
def train_random_forest(X, y, feature_names):
    print(f"\n{'─'*55}")
    print(f"  STAGE 1: Random Forest  ({RF_ESTIMATORS} trees)")
    print(f"{'─'*55}")

    X_tr, X_te, y_tr, y_te = train_test_split(X, y, test_size=0.2,
                                                random_state=RANDOM_SEED, stratify=y)
    rf = RandomForestClassifier(n_estimators=RF_ESTIMATORS, max_depth=20,
                                 min_samples_leaf=2, class_weight='balanced',
                                 random_state=RANDOM_SEED, n_jobs=-1)
    rf.fit(X_tr, y_tr)

    acc = (rf.predict(X_te) == y_te).mean()
    print(f"  RF Test Accuracy: {acc:.4f}")

    fi = pd.Series(rf.feature_importances_, index=feature_names).sort_values(ascending=False)
    print(f"\n  Top 10 Feature Importances:")
    for feat, imp in fi.head(10).items():
        print(f"    {feat:18s}  {'█'*int(imp*50):<25s}  {imp:.4f}")
    return rf, fi


# ══════════════════════════════════════════════════════════════════
#  STEP 5 — APPLY RF WEIGHTS
# ══════════════════════════════════════════════════════════════════
def apply_rf_weights(X, importances, feature_names):
    weights = np.array([importances.get(f, 0.01) for f in feature_names])
    weights = weights / weights.sum() * len(weights)
    return X * weights, weights


# ══════════════════════════════════════════════════════════════════
#  STEP 6 — SEQUENCE WINDOWING  (class-aware)
# ══════════════════════════════════════════════════════════════════
def create_sequences(X, y, seq_len=SEQ_LEN):
    """
    Create sequences only from CONSECUTIVE samples of the same class.
    This ensures temporal coherence within each window.
    """
    Xs, ys = [], []
    i = 0
    while i <= len(X) - seq_len:
        window_labels = y[i:i+seq_len]
        # Only create sequence if all labels in window are the same
        if np.all(window_labels == window_labels[0]):
            Xs.append(X[i:i+seq_len])
            ys.append(window_labels[-1])
            i += 1
        else:
            # Skip to next class boundary
            i += 1

    Xs, ys = np.array(Xs), np.array(ys)
    print(f"[OK] Sequences: {Xs.shape}  (class-coherent windows)")
    uniq, counts = np.unique(ys, return_counts=True)
    for u, c in zip(uniq, counts):
        print(f"     Class {CLASS_LABELS[u]}: {c} sequences")
    return Xs, ys


# ══════════════════════════════════════════════════════════════════
#  STEP 7 — PyTorch LSTM MODEL
# ══════════════════════════════════════════════════════════════════
class HemoLSTM(nn.Module):
    """Multi-layer LSTM with dropout for 3-class hemorrhage classification."""
    def __init__(self, n_features, n_classes=3):
        super().__init__()
        self.lstm1 = nn.LSTM(n_features, 64, batch_first=True)
        self.drop1 = nn.Dropout(0.3)
        self.lstm2 = nn.LSTM(64, 32, batch_first=True)
        self.drop2 = nn.Dropout(0.2)
        self.fc1   = nn.Linear(32, 16)
        self.relu  = nn.ReLU()
        self.fc2   = nn.Linear(16, n_classes)

    def forward(self, x):
        out, _ = self.lstm1(x)
        out = self.drop1(out)
        out, _ = self.lstm2(out)
        out = self.drop2(out[:, -1, :])
        out = self.relu(self.fc1(out))
        return self.fc2(out)


def train_lstm(model, train_loader, val_loader, epochs=LSTM_EPOCHS):
    print(f"\n{'─'*55}")
    print(f"  STAGE 2: LSTM Training  ({epochs} max epochs)")
    print(f"{'─'*55}")

    criterion = nn.CrossEntropyLoss()
    optimizer = torch.optim.Adam(model.parameters(), lr=LSTM_LR, weight_decay=1e-5)
    scheduler = torch.optim.lr_scheduler.ReduceLROnPlateau(optimizer, patience=8, factor=0.5)

    best_val_loss = float('inf')
    best_val_acc = 0
    best_state = None
    patience_counter = 0
    patience_limit = 15

    for epoch in range(epochs):
        model.train()
        train_loss, train_correct, train_total = 0, 0, 0
        for X_b, y_b in train_loader:
            X_b, y_b = X_b.to(DEVICE), y_b.to(DEVICE)
            optimizer.zero_grad()
            out = model(X_b)
            loss = criterion(out, y_b)
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            optimizer.step()
            train_loss += loss.item() * len(y_b)
            train_correct += (out.argmax(1) == y_b).sum().item()
            train_total += len(y_b)

        model.eval()
        val_loss, val_correct, val_total = 0, 0, 0
        with torch.no_grad():
            for X_b, y_b in val_loader:
                X_b, y_b = X_b.to(DEVICE), y_b.to(DEVICE)
                out = model(X_b)
                loss = criterion(out, y_b)
                val_loss += loss.item() * len(y_b)
                val_correct += (out.argmax(1) == y_b).sum().item()
                val_total += len(y_b)

        t_loss = train_loss / train_total
        v_loss = val_loss / val_total
        t_acc = train_correct / train_total
        v_acc = val_correct / val_total
        scheduler.step(v_loss)

        if (epoch + 1) % 5 == 0 or epoch == 0:
            print(f"  Epoch {epoch+1:3d}/{epochs}  |  "
                  f"Train: Loss={t_loss:.4f} Acc={t_acc:.4f}  |  "
                  f"Val: Loss={v_loss:.4f} Acc={v_acc:.4f}")

        if v_acc > best_val_acc or (v_acc == best_val_acc and v_loss < best_val_loss):
            best_val_loss = v_loss
            best_val_acc = v_acc
            best_state = {k: v.clone() for k, v in model.state_dict().items()}
            patience_counter = 0
        else:
            patience_counter += 1
            if patience_counter >= patience_limit:
                print(f"  Early stopping at epoch {epoch+1}")
                break

    if best_state:
        model.load_state_dict(best_state)
    print(f"  Best Val Accuracy: {best_val_acc:.4f}  |  Best Val Loss: {best_val_loss:.4f}")
    return model


# ══════════════════════════════════════════════════════════════════
#  STEP 8 — EVALUATION
# ══════════════════════════════════════════════════════════════════
def evaluate(model, X_test_seq, y_test_seq, rf_model, X_test_flat, y_test_flat):
    print(f"\n{'═'*65}")
    print(f"  EVALUATION RESULTS")
    print(f"{'═'*65}")

    rf_acc = (rf_model.predict(X_test_flat) == y_test_flat).mean()
    print(f"\n  Random Forest (standalone) Accuracy: {rf_acc:.4f}")

    model.eval()
    X_t = torch.FloatTensor(X_test_seq).to(DEVICE)
    with torch.no_grad():
        logits = model(X_t)
        probs = torch.softmax(logits, dim=1).cpu().numpy()
    preds = np.argmax(probs, axis=1)
    lstm_acc = (preds == y_test_seq).mean()
    print(f"  LSTM (RF-weighted)         Accuracy: {lstm_acc:.4f}")

    conf = np.max(probs, axis=1)
    rmse = np.sqrt(np.mean((1.0 - conf) ** 2))
    mae = np.mean(np.abs(1.0 - conf))
    print(f"  LSTM Mean Confidence: {np.mean(conf):.4f}")
    print(f"  LSTM Confidence RMSE: {rmse:.4f}  |  MAE: {mae:.4f}")

    print(f"\n  LSTM Classification Report:")
    print(classification_report(y_test_seq, preds, target_names=CLASS_LABELS, zero_division=0))

    cm = confusion_matrix(y_test_seq, preds)
    print(f"  Confusion Matrix:")
    print(f"               Pred STABLE  Pred WARN  Pred CRIT")
    for i, lbl in enumerate(['True STABLE', 'True WARN  ', 'True CRIT  ']):
        print(f"    {lbl}  {cm[i]}")

    return preds, probs


# ══════════════════════════════════════════════════════════════════
#  STEP 9 — VISUALISATION
# ══════════════════════════════════════════════════════════════════
def plot_predictions(y_true, y_pred, probs, path):
    fig, axes = plt.subplots(2, 1, figsize=(14, 8), dpi=100)
    x = np.arange(len(y_true))

    axes[0].scatter(x, y_true, c='#3b82f6', alpha=0.5, s=15, label='Actual', zorder=2)
    axes[0].scatter(x, y_pred, c='#ef4444', alpha=0.5, s=15, marker='x', label='Predicted', zorder=3)
    axes[0].set_yticks([0, 1, 2]); axes[0].set_yticklabels(CLASS_LABELS)
    axes[0].set_xlabel('Sample'); axes[0].set_ylabel('Class')
    axes[0].set_title('LSTM Predictions vs Actual — HemoGuard Hybrid Model')
    axes[0].legend(); axes[0].grid(alpha=0.3)

    conf = np.max(probs, axis=1)
    colors = ['#10b981' if p == t else '#f43f5e' for p, t in zip(y_pred, y_true)]
    axes[1].bar(x, conf, color=colors, alpha=0.7, width=1.0)
    axes[1].axhline(0.5, color='orange', ls='--', alpha=0.5, label='50% threshold')
    axes[1].set_xlabel('Sample'); axes[1].set_ylabel('Confidence')
    axes[1].set_title('Prediction Confidence (green=correct, red=wrong)')
    axes[1].set_ylim(0, 1.05)
    axes[1].legend(); axes[1].grid(alpha=0.3)

    plt.tight_layout(); plt.savefig(path, bbox_inches='tight'); plt.close()
    print(f"[OK] Saved → {path}")


def plot_feature_importance(importances, path):
    fi = importances.sort_values(ascending=True)
    fig, ax = plt.subplots(figsize=(10, 8), dpi=100)
    ax.barh(fi.index, fi.values, color=plt.cm.viridis(np.linspace(0.3, 0.9, len(fi))))
    ax.set_xlabel('Importance')
    ax.set_title('Random Forest Feature Importances — Used as LSTM Weights')
    ax.grid(alpha=0.3, axis='x')
    plt.tight_layout(); plt.savefig(path, bbox_inches='tight'); plt.close()
    print(f"[OK] Saved → {path}")


# ══════════════════════════════════════════════════════════════════
#  MAIN PIPELINE
# ══════════════════════════════════════════════════════════════════
def main():
    # 1. Load real data
    print("\n[STEP 1] Loading datasets...")
    df_real = load_datasets()

    # 2. Generate synthetic time-series (preserves temporal coherence)
    print("\n[STEP 2] Generating synthetic time-series...")
    df_synth = generate_synthetic_timeseries(n_segments=30, seg_len=50)

    # Combine
    df = pd.concat([df_real, df_synth], ignore_index=True)
    print(f"\n[TOTAL] {len(df)} rows | {df['label'].value_counts().to_dict()}")

    # 3. Feature engineering
    print("\n[STEP 3] Feature engineering...")
    df, all_feat = engineer_features(df)
    df[all_feat] = df[all_feat].ffill().bfill().fillna(0)

    X_all = df[all_feat].values
    y_all = df['label'].values.astype(int)

    # 4. Train Random Forest (on flattened features)
    rf_model, rf_importances = train_random_forest(X_all, y_all, all_feat)

    # 5. Apply RF weights
    print("\n[STEP 5] Applying RF feature weights...")
    X_weighted, weight_arr = apply_rf_weights(X_all, rf_importances, all_feat)
    print(f"[OK] Weighted {X_weighted.shape[1]} features")

    # 6. Normalise
    print("\n[STEP 6] Normalising...")
    scaler = MinMaxScaler()
    X_scaled = scaler.fit_transform(X_weighted)

    # 7. Create class-coherent sequences
    print("\n[STEP 7] Creating class-coherent sequences...")
    X_seq, y_seq = create_sequences(X_scaled, y_all)

    # Shuffle sequences (not individual steps) and split
    indices = np.arange(len(X_seq))
    np.random.shuffle(indices)
    X_seq, y_seq = X_seq[indices], y_seq[indices]

    split = int(len(X_seq) * 0.8)
    X_train, X_test = X_seq[:split], X_seq[split:]
    y_train, y_test = y_seq[:split], y_seq[split:]
    print(f"[OK] Train: {len(X_train)},  Test: {len(X_test)}")

    train_ds = TensorDataset(torch.FloatTensor(X_train), torch.LongTensor(y_train))
    test_ds  = TensorDataset(torch.FloatTensor(X_test),  torch.LongTensor(y_test))
    train_loader = DataLoader(train_ds, batch_size=LSTM_BATCH, shuffle=True)
    test_loader  = DataLoader(test_ds,  batch_size=LSTM_BATCH)

    # 8. Build & train LSTM
    print("\n[STEP 8] Building LSTM...")
    n_features = X_train.shape[2]
    lstm = HemoLSTM(n_features, n_classes=3).to(DEVICE)
    total_params = sum(p.numel() for p in lstm.parameters())
    print(f"  Architecture: LSTM(22→64) → Dropout(0.3) → LSTM(64→32) → Dropout(0.2) → Dense(16) → Dense(3)")
    print(f"  Total Parameters: {total_params:,}")

    lstm = train_lstm(lstm, train_loader, test_loader)

    # 9. Evaluate
    print("\n[STEP 9] Evaluating...")
    X_tr_flat, X_te_flat, y_tr_flat, y_te_flat = train_test_split(
        X_all, y_all, test_size=0.2, random_state=RANDOM_SEED, stratify=y_all)
    preds, probs = evaluate(lstm, X_test, y_test, rf_model, X_te_flat, y_te_flat)

    # 10. Visualise
    print("\n[STEP 10] Visualisations...")
    plot_predictions(y_test, preds, probs, os.path.join(BASE_DIR, 'hybrid_predictions.png'))
    plot_feature_importance(rf_importances, os.path.join(BASE_DIR, 'hybrid_feature_importance.png'))

    # 11. Save models
    print(f"\n{'═'*65}")
    print(f"  SAVING MODELS")
    print(f"{'═'*65}")

    torch.save(lstm.state_dict(), os.path.join(BASE_DIR, 'hemoguard_lstm.pt'))
    print(f"  [SAVED] LSTM          → hemoguard_lstm.pt")

    joblib.dump(rf_model, os.path.join(BASE_DIR, 'hemoguard_rf_weights.pkl'))
    print(f"  [SAVED] Random Forest → hemoguard_rf_weights.pkl")

    joblib.dump(scaler, os.path.join(BASE_DIR, 'hemoguard_scaler.pkl'))
    print(f"  [SAVED] Scaler        → hemoguard_scaler.pkl")

    joblib.dump(rf_model, os.path.join(BASE_DIR, 'hemoguard_model.pkl'))
    print(f"  [SAVED] RF compat     → hemoguard_model.pkl")

    meta = {
        'feature_names': all_feat,
        'base_feature_names': FEATURE_COLS,
        'rf_feature_importances': rf_importances.to_dict(),
        'rf_feature_weights': weight_arr.tolist(),
        'seq_len': SEQ_LEN,
        'n_features': n_features,
        'class_labels': CLASS_LABELS,
        'model_type': 'Hybrid LSTM (PyTorch) + RandomForest',
        'lstm_architecture': 'LSTM(64)-Drop(0.3)-LSTM(32)-Drop(0.2)-Dense(16)-Dense(3)',
        'rf_n_estimators': RF_ESTIMATORS,
    }
    with open(os.path.join(BASE_DIR, 'hemoguard_model_meta.json'), 'w') as f:
        json.dump(meta, f, indent=2)
    print(f"  [SAVED] Metadata      → hemoguard_model_meta.json")

    print(f"\n{'═'*65}")
    print(f"  ✅ PIPELINE COMPLETE")
    print(f"  Restart backend:  python -m uvicorn backend:app --port 8000")
    print(f"{'═'*65}")


if __name__ == '__main__':
    main()
