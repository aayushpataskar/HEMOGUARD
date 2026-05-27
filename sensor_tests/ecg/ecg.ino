#define ECG_PIN 34
#define LO_PLUS 32
#define LO_MINUS 33

int threshold = 2100;

int ecgValue = 0;
int prevValue = 0;

unsigned long rPeakTime = 0;        // current R peak time
unsigned long prevRPeakTime = 0;    // previous R peak time
unsigned long rrInterval = 0;       // time between peaks

void setup()
{
  Serial.begin(115200);

  pinMode(ECG_PIN, INPUT);
  pinMode(LO_PLUS, INPUT);
  pinMode(LO_MINUS, INPUT);

  Serial.println("ECG System Ready");
}

void loop()
{
  // electrode check
  if(digitalRead(LO_PLUS) || digitalRead(LO_MINUS))
  {
    Serial.println("Leads Off - Check Electrodes");
    delay(200);
    return;
  }

  ecgValue = analogRead(ECG_PIN);

  Serial.print("ECG: ");
  Serial.println(ecgValue);

  // detect R peak
  if(ecgValue > threshold && prevValue <= threshold)
  {
    rPeakTime = millis();

    if(prevRPeakTime != 0)
    {
      rrInterval = rPeakTime - prevRPeakTime;
    }

    Serial.println("----- R PEAK DETECTED -----");

    Serial.print("R Peak Value: ");
    Serial.println(ecgValue);

    Serial.print("R Peak Time (ms): ");
    Serial.println(rPeakTime);

    Serial.print("Previous R Peak Time (ms): ");
    Serial.println(prevRPeakTime);

    Serial.print("RR Interval (ms): ");
    Serial.println(rrInterval);

    Serial.println("---------------------------");

    prevRPeakTime = rPeakTime;
  }

  prevValue = ecgValue;

  delay(5);
}
