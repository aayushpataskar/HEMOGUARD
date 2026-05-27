#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 4

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

float temp1, temp2;
float t1 = 0;

void setup() {

  Serial.begin(115200);
  sensors.begin();

}

void loop() {

  sensors.requestTemperatures();

  temp1 = sensors.getTempCByIndex(0);
  temp2 = sensors.getTempCByIndex(1);

  float diff = abs(temp1 - temp2);

  Serial.print("Temp1: ");
  Serial.print(temp1);
  Serial.print(" C  | Temp2: ");
  Serial.print(temp2);
  Serial.print(" C  | Difference: ");
  Serial.println(diff);

  if(diff > 1.0){

    t1 = diff;

    Serial.println("⚠ Temperature Gradient Alert!");
    Serial.print("Gradient Value: ");
    Serial.println(t1);

  }

  delay(1000);
}
