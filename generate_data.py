import numpy as np
import pandas as pd

data = []

def generate_sample(label):
    HR_base = np.random.normal(70, 10)
    PTT_base = np.random.normal(180, 20)
    PPG_base = np.random.normal(1.0, 0.2)
    Imp_base = np.random.normal(50, 10)
    Temp_base = np.random.normal(36.5, 0.5)

    noise = lambda scale: np.random.normal(0, scale)

    if label == 0:  # Stable
        HR_slope = np.random.normal(0, 0.2)
        PTT_slope = np.random.normal(0, 0.4)
        PPG_slope = np.random.normal(0, 0.02)
        Imp_slope = np.random.normal(0, 0.2)
        Temp_slope = np.random.normal(0, 0.02)
    elif label == 1:  # Mild
        HR_slope = np.random.normal(0.2, 0.3)
        PTT_slope = np.random.normal(0.6, 0.5)
        PPG_slope = np.random.normal(-0.02, 0.02)
        Imp_slope = np.random.normal(0.2, 0.3)
        Temp_slope = np.random.normal(-0.02, 0.02)
    else:  # Critical
        HR_slope = np.random.normal(0.6, 0.4)
        PTT_slope = np.random.normal(1.2, 0.6)
        PPG_slope = np.random.normal(-0.05, 0.03)
        Imp_slope = np.random.normal(0.5, 0.4)
        Temp_slope = np.random.normal(-0.05, 0.03)

    HR = HR_base + noise(2)
    PTT = PTT_base + noise(5)
    PPG = PPG_base + noise(0.05)
    Imp = Imp_base + noise(2)
    Temp = Temp_base + noise(0.1)

    return [
        HR, HR_slope, PTT, PTT_slope,
        PPG, PPG_slope, Imp, Imp_slope,
        Temp, Temp_slope, label
    ]

for _ in range(400):
    data.append(generate_sample(0))
for _ in range(300):
    data.append(generate_sample(1))
for _ in range(300):
    data.append(generate_sample(2))

columns = [
    "HR", "HR_slope", "PTT", "PTT_slope",
    "PPG", "PPG_slope", "Imp", "Imp_slope",
    "Temp", "Temp_slope", "Label"
]

df = pd.DataFrame(data, columns=columns)
df = df.sample(frac=1).reset_index(drop=True)
df.to_csv("hemoguard_data.csv", index=False)
print("Dataset generated: hemoguard_data.csv")
