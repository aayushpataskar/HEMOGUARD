import urllib.request, json

tests = [
    {"HR":72,"HR_slope":0.1,"PTT":200,"PTT_slope":0.3,"PPG":1.0,"PPG_slope":0,"Imp":50,"Imp_slope":0.1,"Temp":36.5,"Temp_slope":0},
    {"HR":95,"HR_slope":0.5,"PTT":180,"PTT_slope":0.8,"PPG":0.8,"PPG_slope":-0.02,"Imp":60,"Imp_slope":0.4,"Temp":36.3,"Temp_slope":-0.03},
    {"HR":120,"HR_slope":1.2,"PTT":160,"PTT_slope":1.5,"PPG":0.6,"PPG_slope":-0.06,"Imp":80,"Imp_slope":0.8,"Temp":35.8,"Temp_slope":-0.08},
]
labels = ["Stable Patient", "Warning Patient", "Critical Patient"]

for i, t in enumerate(tests):
    data = json.dumps(t).encode()
    req = urllib.request.Request("http://127.0.0.1:8000/predict", data=data, headers={"Content-Type":"application/json"})
    try:
        r = urllib.request.urlopen(req)
        result = json.loads(r.read().decode())
        if "error" in result:
            print(f"{labels[i]}: Error = {result['error']}")
        else:
            pred_label = ["STABLE","WARNING","CRITICAL"][result["prediction"]]
            print(f"{labels[i]}: Predicted={pred_label}, Confidence={result['confidence']:.1%}")
    except Exception as e:
        print(f"{labels[i]}: Request failed - {e}")
