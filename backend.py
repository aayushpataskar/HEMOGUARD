"""
HemoGuard Hybrid Backend  (PyTorch)
====================================
Serves predictions using LSTM (PyTorch) + Random Forest.
Falls back to RF-only if LSTM files not found.
"""

from fastapi import FastAPI
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse
from pydantic import BaseModel
import joblib
import numpy as np
import pandas as pd
import csv, os, json
from datetime import datetime
from collections import deque
from fastapi.middleware.cors import CORSMiddleware

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

# ── Data Models ──────────────────────────────────────────────────
class PatientData(BaseModel):
    HR: float;      HR_slope: float
    PTT: float;     PTT_slope: float
    PPG: float;     PPG_slope: float
    Imp: float;     Imp_slope: float
    Temp: float;    Temp_slope: float

class RecordData(PatientData):
    label: int
    session_id: str

# ── App ──────────────────────────────────────────────────────────
app = FastAPI()
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_credentials=True,
                   allow_methods=["*"], allow_headers=["*"])

# ── Serve Frontend ───────────────────────────────────────────────
@app.get("/")
def serve_index():
    return FileResponse(os.path.join(BASE_DIR, "index2.html"))

app.mount("/static", StaticFiles(directory=BASE_DIR), name="static")

# Serve CSS/JS directly at root level (index2.html references them without /static/)
@app.get("/style2.css")
def serve_css():
    return FileResponse(os.path.join(BASE_DIR, "style2.css"), media_type="text/css")

@app.get("/script2.js")
def serve_js():
    return FileResponse(os.path.join(BASE_DIR, "script2.js"), media_type="application/javascript")

FEATURE_COLS = ['HR','HR_slope','PTT','PTT_slope','PPG','PPG_slope',
                'Imp','Imp_slope','Temp','Temp_slope']
CLASS_LABELS = ['STABLE', 'WARNING', 'CRITICAL']

# ── Model Loading ────────────────────────────────────────────────
HYBRID_MODE = False
lstm_model = None
rf_model = None
scaler = None
model_meta = None
rf_weights = None
seq_len = 10

lstm_path   = os.path.join(BASE_DIR, 'hemoguard_lstm.pt')
rf_path     = os.path.join(BASE_DIR, 'hemoguard_rf_weights.pkl')
scaler_path = os.path.join(BASE_DIR, 'hemoguard_scaler.pkl')
meta_path   = os.path.join(BASE_DIR, 'hemoguard_model_meta.json')

if all(os.path.exists(p) for p in [lstm_path, rf_path, scaler_path, meta_path]):
    try:
        import torch
        import torch.nn as nn

        # Load metadata first to get architecture params
        with open(meta_path) as f:
            model_meta = json.load(f)
        seq_len    = model_meta['seq_len']
        n_features = model_meta['n_features']
        rf_weights = np.array(model_meta['rf_feature_weights'])

        # Define model class (must match training)
        class HemoLSTM(nn.Module):
            def __init__(self, n_feat, n_cls=3):
                super().__init__()
                self.lstm1 = nn.LSTM(n_feat, 64, batch_first=True)
                self.drop1 = nn.Dropout(0.3)
                self.lstm2 = nn.LSTM(64, 32, batch_first=True)
                self.drop2 = nn.Dropout(0.2)
                self.fc1   = nn.Linear(32, 16)
                self.relu  = nn.ReLU()
                self.fc2   = nn.Linear(16, n_cls)
            def forward(self, x):
                out, _ = self.lstm1(x)
                out = self.drop1(out)
                out, _ = self.lstm2(out)
                out = self.drop2(out[:, -1, :])
                return self.fc2(self.relu(self.fc1(out)))

        lstm_model = HemoLSTM(n_features)
        lstm_model.load_state_dict(torch.load(lstm_path, weights_only=True, map_location='cpu'))
        lstm_model.eval()

        rf_model = joblib.load(rf_path)
        scaler   = joblib.load(scaler_path)
        HYBRID_MODE = True
        print("[BACKEND] ✅ Hybrid mode: PyTorch LSTM + Random Forest")
    except Exception as e:
        print(f"[BACKEND] ⚠ Hybrid load failed: {e}")
        HYBRID_MODE = False

if not HYBRID_MODE:
    rf_model = joblib.load(os.path.join(BASE_DIR, 'hemoguard_model.pkl'))
    print("[BACKEND] 🔧 Fallback mode: Random Forest only")

# ── Sliding window buffer ────────────────────────────────────────
feature_buffer = deque(maxlen=100)

def _engineer_buffer(buf):
    """Compute rolling avgs & lags from raw buffer rows."""
    arr = np.array(buf)
    n = arr.shape[0]
    key_idxs = [FEATURE_COLS.index(c) for c in ['HR','PTT','PPG','Imp']]
    result = []
    for i in range(n):
        row = list(arr[i])
        for ki in key_idxs:
            start = max(0, i-4)
            row.append(float(np.mean(arr[start:i+1, ki])))          # roll5
            row.append(float(arr[i-1, ki]) if i > 0 else float(arr[i, ki]))  # lag1
            row.append(float(arr[i-2, ki]) if i > 1 else float(arr[i, ki]))  # lag2
        result.append(row)
    return result

# ── Endpoints ────────────────────────────────────────────────────
@app.post("/predict")
def predict(data: PatientData):
    try:
        base_vec = [data.HR, data.HR_slope, data.PTT, data.PTT_slope,
                    data.PPG, data.PPG_slope, data.Imp, data.Imp_slope,
                    data.Temp, data.Temp_slope]

        if HYBRID_MODE and lstm_model is not None:
            import torch
            feature_buffer.append(base_vec)

            if len(feature_buffer) >= seq_len:
                eng = _engineer_buffer(list(feature_buffer))
                seq = np.array(eng[-seq_len:])
                seq = seq * rf_weights
                seq = scaler.transform(seq)
                inp = torch.FloatTensor(seq).unsqueeze(0)

                with torch.no_grad():
                    logits = lstm_model(inp)
                    probs = torch.softmax(logits, dim=1).numpy()[0]

                pred = int(np.argmax(probs))
                return {
                    "prediction": pred,
                    "label": CLASS_LABELS[pred],
                    "probability": probs.tolist(),
                    "class_labels": CLASS_LABELS,
                    "confidence": float(np.max(probs)),
                    "model_type": "hybrid_lstm_rf"
                }
            else:
                # warmup — not enough history for LSTM, use RF with engineered features
                eng = _engineer_buffer(list(feature_buffer))
                eng_vec = np.array(eng[-1:])
                pred = rf_model.predict(eng_vec)
                prob = rf_model.predict_proba(eng_vec)
                return {
                    "prediction": int(pred[0]),
                    "label": CLASS_LABELS[int(pred[0])],
                    "probability": prob[0].tolist(),
                    "class_labels": CLASS_LABELS,
                    "confidence": float(max(prob[0])),
                    "model_type": "rf_warmup"
                }
        else:
            # Fallback RF-only — compute engineered features from buffer
            feature_buffer.append(base_vec)
            eng = _engineer_buffer(list(feature_buffer))
            eng_vec = np.array(eng[-1:])
            pred = rf_model.predict(eng_vec)
            prob = rf_model.predict_proba(eng_vec)
            return {
                "prediction": int(pred[0]),
                "label": CLASS_LABELS[int(pred[0])],
                "probability": prob[0].tolist(),
                "class_labels": CLASS_LABELS,
                "confidence": float(max(prob[0])),
                "model_type": "random_forest"
            }
    except Exception as e:
        return {"error": str(e)}


@app.get("/model-info")
def model_info():
    try:
        if HYBRID_MODE and model_meta:
            base_imp = rf_model.feature_importances_[:len(FEATURE_COLS)].tolist()
            while len(base_imp) < len(FEATURE_COLS):
                base_imp.append(0.0)
            return {
                "model_type": "Hybrid LSTM + RandomForest",
                "lstm_architecture": model_meta.get('lstm_architecture', 'N/A'),
                "n_estimators": model_meta.get('rf_n_estimators', 500),
                "n_features": len(FEATURE_COLS),
                "feature_names": FEATURE_COLS,
                "feature_importances": base_imp,
                "class_labels": CLASS_LABELS,
                "max_depth": 20,
                "seq_len": seq_len,
                "hybrid_mode": True
            }
        else:
            return {
                "model_type": "RandomForest",
                "n_estimators": rf_model.n_estimators,
                "n_features": len(FEATURE_COLS),
                "feature_names": FEATURE_COLS,
                "feature_importances": rf_model.feature_importances_.tolist(),
                "class_labels": CLASS_LABELS,
                "max_depth": rf_model.max_depth,
                "hybrid_mode": False
            }
    except Exception as e:
        return {"error": str(e)}


@app.post("/record")
def record_data(data: RecordData):
    try:
        filename = os.path.join(BASE_DIR, "real_esp32_data.csv")
        file_exists = os.path.isfile(filename)
        with open(filename, mode='a', newline='') as f:
            w = csv.writer(f)
            if not file_exists:
                w.writerow(["timestamp","session_id","HR","HR_slope","PTT","PTT_slope",
                            "PPG","PPG_slope","Imp","Imp_slope","Temp","Temp_slope","label"])
            w.writerow([datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3],
                        data.session_id, data.HR, data.HR_slope, data.PTT, data.PTT_slope,
                        data.PPG, data.PPG_slope, data.Imp, data.Imp_slope,
                        data.Temp, data.Temp_slope, data.label])
        return {"status": "success", "recorded": True}
    except Exception as e:
        return {"error": str(e)}
