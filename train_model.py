import pandas as pd
from sklearn.ensemble import RandomForestClassifier
import joblib
import argparse
import os

parser = argparse.ArgumentParser()
parser.add_argument('--real', action='store_true', help='Use real_esp32_data.csv instead of synthetic hemoguard_data.csv')
args = parser.parse_args()

file_path = "real_esp32_data.csv" if args.real else "hemoguard_data.csv"

if not os.path.exists(file_path):
    print(f"Error: Dataset {file_path} not found!")
    exit(1)

print(f"Loading dataset: {file_path}")
df = pd.read_csv(file_path)

if len(df) == 0:
    print(f"\n[!] ERROR: '{file_path}' is completely empty!")
    print("[!] Please open your Web Dashboard, enter your ESP32 IP, and click [RECORD] to gather some physical data before trying to train the model.")
    exit(1)

if 'timestamp' in df.columns:
    df = df.drop(columns=['timestamp'])
if 'session_id' in df.columns:
    df = df.drop(columns=['session_id'])

target_col = 'label' if 'label' in df.columns else 'Label'

X = df.drop(columns=[target_col])
y = df[target_col]

model = RandomForestClassifier(n_estimators=100, random_state=42)
model.fit(X, y)

joblib.dump(model, "hemoguard_model.pkl")
print("Model trained and saved: hemoguard_model.pkl")
print(f"Training accuracy: {model.score(X, y):.4f}")
