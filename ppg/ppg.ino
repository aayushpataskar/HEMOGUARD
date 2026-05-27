#include <Wire.h>
#include "MAX30100.h"

MAX30100 sensor;

uint16_t ir, red;

uint16_t peakPPG = 0;
uint16_t valleyPPG = 65535;

uint16_t amplitude = 0;

unsigned long peakTime = 0;

bool rising = false;

void setup()
{
  Serial.begin(115200);
  Wire.begin(21,22);

  Serial.println("Initializing MAX30100...");

  if(!sensor.begin())
  {
    Serial.println("MAX30100 not detected!");
    while(1);
  }

  sensor.setMode(MAX30100_MODE_SPO2_HR);
  sensor.setLedsCurrent(MAX30100_LED_CURR_7_6MA,
                        MAX30100_LED_CURR_7_6MA);

  Serial.println("Sensor Ready");
}

void loop()
{
  sensor.update();

  if(sensor.getRawValues(&ir,&red))
  {
    static uint16_t prevIR = 0;

    // Finger detection
    if(ir < 7000)
    {
      Serial.println("No Finger Detected");
      return;
    }

    // detect valley
    if(ir < valleyPPG)
    {
      valleyPPG = ir;
    }

    // rising edge
    if(ir > prevIR)
    {
      rising = true;
    }

    // peak detection
    if(ir < prevIR && rising)
    {
      peakPPG = prevIR;
      peakTime = millis();

      amplitude = peakPPG - valleyPPG;

      Serial.println("----- SYSTOLIC PEAK -----");

      Serial.print("Peak PPG: ");
      Serial.println(peakPPG);

      Serial.print("Valley PPG: ");
      Serial.println(valleyPPG);

      Serial.print("Amplitude (Peak-Valley): ");
      Serial.println(amplitude);

      Serial.print("Peak Time (ms): ");
      Serial.println(peakTime);

      Serial.println("-------------------------");

      // reset valley
      valleyPPG = peakPPG;
      rising = false;
    }

    prevIR = ir;
  }

  delay(10);
}