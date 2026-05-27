"""
HemoGuard Clinical ML Training Pipeline
=============================================
Trains a 3-class (STABLE=0, WARNING=1, CRITICAL=2) RandomForest model using:
  1. Real ESP32 data (real_esp32_data.csv) — STABLE recordings
  2. S10_ECG.csv   — Raw ECG waveform → windowed HR/HRV → STABLE reference
  3. LBNP published study values → Clinically-accurate WARNING/CRITICAL synthesis

The model is saved as hemoguard_model.pkl
Backend /predict endpoint remains unchanged (receives HR, PTT, PPG, etc.)
Feature engineering now happens inside this script + backend.
"""

import pandas as pd
import numpy as np
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix
from sklearn.utils import resample
import joblib
import os

print("=" * 60)
print("  HemoGuard Clinical ML Pipeline v2.0")
print("=" * 60)

# ─── Constants (must match ESP32 / backend scale) ────────────────
FEATURE_COLS = ['HR', 'HR_slope', 'PTT', 'PTT_slope',
                'PPG', 'PPG_slope', 'Imp', 'Imp_slope',
                'Temp', 'Temp_slope']

# ═══════════════════════════════════════════════════════════════════
# STEP 1: Load existing real_esp32_data.csv  (all labels)
# ═══════════════════════════════════════════════════════════════════
all_frames = []

REAL_CSV = os.path.join(os.path.dirname(__file__), 'real_esp32_data.csv')
if os.path.exists(REAL_CSV):
    df_real = pd.read_csv(REAL_CSV)
    if len(df_real) > 0:
        if 'timestamp' in df_real.columns:
            df_real = df_real.drop(columns=['timestamp'])
        if 'session_id' in df_real.columns:
            df_real = df_real.drop(columns=['session_id'])
        target_col = 'label' if 'label' in df_real.columns else 'Label'
        df_real = df_real.rename(columns={target_col: 'label'})
        df_real[FEATURE_COLS] = df_real[FEATURE_COLS].apply(pd.to_numeric, errors='coerce')
        df_real = df_real.dropna(subset=FEATURE_COLS)
        all_frames.append(df_real[FEATURE_COLS + ['label']])
        print(f"[OK] Loaded real_esp32_data.csv → {len(df_real)} rows | "
              f"Labels: {df_real['label'].value_counts().to_dict()}")
    else:
        print("[SKIP] real_esp32_data.csv is empty")
else:
    print("[SKIP] real_esp32_data.csv not found")

# ═══════════════════════════════════════════════════════════════════
# STEP 2: Process S10_ECG.csv raw ECG → windowed physiology = STABLE
# ═══════════════════════════════════════════════════════════════════
ECG_CSV = r"C:\Users\trushna\OneDrive\Documents\S10_ECG.csv"

if os.path.exists(ECG_CSV):
    print(f"\n[PROCESSING] S10_ECG.csv  (this may take a moment)...")
    ecg_df = pd.read_csv(ECG_CSV)
    ecg_col = [c for c in ecg_df.columns if 'ECG' in c.upper() or 'mv' in c.lower()]
    if ecg_col:
        ecg_signal = ecg_df[ecg_col[0]].values
    else:
        ecg_signal = ecg_df.iloc[:, 1].values

    # --- R-peak detection (simple threshold crossing at 1kHz) --------
    THRESH = 0.5   # mV  (QRS peaks in S10 are ~0.8-1.0 mV)
    REFRACTORY = 250  # samples = 250ms at 1kHz

    r_peaks = []
    last_peak = -REFRACTORY
    for i in range(1, len(ecg_signal)):
        if (ecg_signal[i] > THRESH and
                ecg_signal[i] > ecg_signal[i-1] and
                i - last_peak > REFRACTORY):
            r_peaks.append(i)
            last_peak = i

    print(f"  Detected {len(r_peaks)} R-peaks from {len(ecg_signal)} samples")

    # --- Extract RR intervals and compute 10-beat windows ─────────────
    rr_intervals = np.diff(r_peaks)  # samples between R-peaks
    rr_ms = rr_intervals.astype(float)  # at 1kHz, 1 sample = 1ms

    stable_rows = []
    WINDOW = 10   # 10-beat window for each feature row

    for i in range(WINDOW, len(rr_ms) - WINDOW, WINDOW):
        window = rr_ms[i:i+WINDOW]
        if np.any(window < 300) or np.any(window > 2000):
            continue   # filter artefact beats

        hr = 60000.0 / np.mean(window)        # bpm
        hrv_rmssd = np.sqrt(np.mean(np.diff(window)**2))  # RMSSD ms

        # Map RMSSD → HRV-based slopes (higher RMSSD = more stable)
        hr_slope = np.clip((hr - 72) * 0.01, -1.0, 1.0)  # normal HR≈72
        ptt_val  = np.clip(240 - (hr - 72) * 0.8, 150, 380)  # PTT drops if HR rises
        ptt_slope = np.clip((ptt_val - 240) * 0.01, -2.5, 2.5)

        ppg_amp  = np.clip(1.0 + (72 - hr) * 0.005, 0.5, 1.5)  # PPG reduces if HR rises
        imp_val  = np.clip(50.0 + (hr - 72) * 0.3, 30, 120)    # bio impedance
        temp_val = 36.6  # stable room temperature assumption

        stable_rows.append({
            'HR': hr, 'HR_slope': hr_slope,
            'PTT': ptt_val, 'PTT_slope': ptt_slope,
            'PPG': ppg_amp, 'PPG_slope': 0.0,
            'Imp': imp_val, 'Imp_slope': 0.0,
            'Temp': temp_val, 'Temp_slope': 0.0,
            'label': 0
        })

    df_ecg = pd.DataFrame(stable_rows)
    all_frames.append(df_ecg)
    print(f"[OK] S10_ECG.csv → {len(df_ecg)} windowed STABLE rows extracted")
else:
    print(f"[SKIP] S10_ECG.csv not found at {ECG_CSV}")

# ═══════════════════════════════════════════════════════════════════
# STEP 3: Synthesize WARNING + CRITICAL using LBNP published values
# Based on: Convertino et al. 2011 LBNP studies
#   Baseline: HR~60, PTT~260ms, PPG=1.0 (norm), Imp~45
#   WARNING (-30mmHg): HR~75, PTT~220ms, PPG~0.45, Imp~65
#   CRITICAL (symptomatic): HR~105, PTT~160ms, PPG~0.4, Imp~90+
# ═══════════════════════════════════════════════════════════════════
print("\n[GENERATING] Clinically-parameterized WARNING + CRITICAL rows...")

N_PER_CLASS = 600   # balanced with expected stable count
rng = np.random.default_rng(42)

def make_rows(n, label,
              hr_mu, hr_sig,
              ptt_mu, ptt_sig,
              ppg_mu, ppg_sig,
              imp_mu, imp_sig,
              temp_mu, temp_sig,
              hr_slope_mu=0.0, ptt_slope_mu=0.0):
    """Generate n rows of physiological data around given means."""
    rows = []
    prev_hr = hr_mu
    prev_ptt = ptt_mu
    for _ in range(n):
        hr  = float(np.clip(rng.normal(hr_mu,  hr_sig),  40, 200))
        ptt = float(np.clip(rng.normal(ptt_mu, ptt_sig), 80, 380))
        ppg = float(np.clip(rng.normal(ppg_mu, ppg_sig), 0.01, 2.0))
        imp = float(np.clip(rng.normal(imp_mu, imp_sig), 20, 200))
        tem = float(np.clip(rng.normal(temp_mu, temp_sig), 30.0, 42.0))
        hr_sl  = float(np.clip((hr - prev_hr) * 0.1,  -1.0, 1.0))
        ptt_sl = float(np.clip((ptt - prev_ptt) * 0.05, -2.5, 2.5))
        rows.append({
            'HR': hr, 'HR_slope': hr_sl,
            'PTT': ptt, 'PTT_slope': ptt_sl,
            'PPG': ppg, 'PPG_slope': 0.0,
            'Imp': imp, 'Imp_slope': 0.0,
            'Temp': tem, 'Temp_slope': 0.0,
            'label': label
        })
        prev_hr, prev_ptt = hr, ptt
    return rows

# WARNING — LBNP -30 mmHg stage (moderate hypovolemia)
warning_rows = make_rows(
    N_PER_CLASS, label=1,
    hr_mu=78,  hr_sig=6,
    ptt_mu=215, ptt_sig=18,
    ppg_mu=0.55, ppg_sig=0.12,
    imp_mu=65,  imp_sig=8,
    temp_mu=35.5, temp_sig=0.6,
    hr_slope_mu=0.2, ptt_slope_mu=-0.15
)

# CRITICAL — LBNP symptomatic phase (severe hypovolemia / bleeding)
critical_rows = make_rows(
    N_PER_CLASS, label=2,
    hr_mu=105, hr_sig=10,
    ptt_mu=162, ptt_sig=22,
    ppg_mu=0.35, ppg_sig=0.10,
    imp_mu=92,  imp_sig=12,
    temp_mu=34.5, temp_sig=0.8,
    hr_slope_mu=0.6, ptt_slope_mu=-0.5
)

df_warn  = pd.DataFrame(warning_rows)
df_crit  = pd.DataFrame(critical_rows)
all_frames.extend([df_warn, df_crit])
print(f"[OK] WARNING rows: {len(df_warn)}")
print(f"[OK] CRITICAL rows: {len(df_crit)}")

# ═══════════════════════════════════════════════════════════════════
# STEP 4: Merge + Class Balance
# ═══════════════════════════════════════════════════════════════════
df_all = pd.concat(all_frames, ignore_index=True)
df_all[FEATURE_COLS] = df_all[FEATURE_COLS].apply(pd.to_numeric, errors='coerce')
df_all = df_all.dropna()

print(f"\n[MERGE] Total rows before balancing: {len(df_all)}")
print(f"  Class distribution: {df_all['label'].value_counts().to_dict()}")

# Balance: oversample minority, cap majority
max_class = N_PER_CLASS
balanced = []
for cls in [0, 1, 2]:
    cls_df = df_all[df_all['label'] == cls]
    if len(cls_df) == 0:
        print(f"  [WARNING] Class {cls} has 0 samples!")
        continue
    if len(cls_df) < max_class:
        cls_df = resample(cls_df, replace=True, n_samples=max_class, random_state=42)
    else:
        cls_df = cls_df.sample(n=max_class, random_state=42)
    balanced.append(cls_df)

df_balanced = pd.concat(balanced, ignore_index=True).sample(frac=1, random_state=42)
print(f"  Balanced distribution: {df_balanced['label'].value_counts().to_dict()}")
print(f"  Final training rows: {len(df_balanced)}")

# ═══════════════════════════════════════════════════════════════════
# STEP 5: Train RandomForest
# ═══════════════════════════════════════════════════════════════════
X = df_balanced[FEATURE_COLS]
y = df_balanced['label']

X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.20, random_state=42, stratify=y)

print(f"\n[TRAINING] RandomForestClassifier (500 trees, balanced)...")
model = RandomForestClassifier(
    n_estimators=500,
    max_depth=20,
    min_samples_leaf=2,
    class_weight='balanced',
    random_state=42,
    n_jobs=-1
)
model.fit(X_train, y_train)

# ═══════════════════════════════════════════════════════════════════
# STEP 6: Validate
# ═══════════════════════════════════════════════════════════════════
y_pred = model.predict(X_test)
print(f"\n[VALIDATION]")
print(f"  Test Accuracy: {(y_pred == y_test).mean():.4f}")
print()
print(classification_report(y_test, y_pred, target_names=['STABLE', 'WARNING', 'CRITICAL']))
print("Confusion Matrix:")
cm = confusion_matrix(y_test, y_pred)
print(f"              Pred STABLE  Pred WARN  Pred CRIT")
for i, row_label in enumerate(['True STABLE ', 'True WARN  ', 'True CRIT  ']):
    print(f"  {row_label}  {cm[i]}")

# Feature importance
print("\n[FEATURE IMPORTANCE] (Top ranked features for decision making)")
fi = pd.Series(model.feature_importances_, index=FEATURE_COLS).sort_values(ascending=False)
for feat, imp in fi.items():
    bar = '#' * int(imp * 40)
    print(f"  {feat:15s}: {bar:<40s} {imp:.4f}")

# ═══════════════════════════════════════════════════════════════════
# STEP 7: Save Model
# ═══════════════════════════════════════════════════════════════════
MODEL_PATH = os.path.join(os.path.dirname(__file__), 'hemoguard_model.pkl')
joblib.dump(model, MODEL_PATH)
print(f"\n[SAVED] hemoguard_model.pkl → {MODEL_PATH}")
print("\n[DONE] Model is ready. Restart uvicorn to hot-reload the new model.")
print("=" * 60)
