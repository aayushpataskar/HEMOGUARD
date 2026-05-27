const int injectPin = 25;     // current injection
const int sensePin = 35;      // instrumentation amp output

int baseline = 0;
int bioValue = 0;

void setup() {

  Serial.begin(115200);

  pinMode(injectPin, OUTPUT);

  // Generate AC signal using PWM (ESP32 Core 3.x API)
  ledcAttach(injectPin, 50000, 8);  // pin, 50 kHz, 8-bit resolution
  ledcWrite(injectPin, 128);        // 50% duty cycle

  delay(2000);

  // Establish baseline
  long sum = 0;

  for(int i=0;i<500;i++){
    sum += analogRead(sensePin);
    delay(2);
  }

  baseline = sum / 500;

  Serial.print("Baseline Impedance: ");
  Serial.println(baseline);
}

void loop() {

  bioValue = analogRead(sensePin);

  int deltaZ = bioValue - baseline;

  Serial.print("Bioimpedance Signal: ");
  Serial.print(bioValue);

  Serial.print("  | Change: ");
  Serial.println(deltaZ);

  delay(50);
}