#include <SPI.h>
#include <MySensor.h>

#define round(x) ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))

#define CHILD_ID_MOISTURE 0
#define CHILD_ID_BATTERY 1
#define SENSOR_ANALOG_PIN 0
#define SENSOR_POWER_PIN 8
#define SLEEP_TIME 300000 // Sleep time between reads (in milliseconds)
#define STABILIZATION_TIME 500 // Let the sensor stabilize before reading
#define BATTERY_FULL 3700 // 3,700 millivolts
#define BATTERY_ZERO 1700 // 1,700 millivolts

MySensor gw;
MyMessage msg(CHILD_ID_MOISTURE, V_HUM);
MyMessage voltage_msg(CHILD_ID_BATTERY, V_VOLTAGE);

void setup()
{
  pinMode(SENSOR_POWER_PIN, OUTPUT);
  gw.begin();

  gw.sendSketchInfo("Plant moisture w bat", "1.1");

  gw.present(CHILD_ID_MOISTURE, S_HUM);
  delay(250);
  gw.present(CHILD_ID_BATTERY, S_CUSTOM);
}

void loop()
{
  digitalWrite(SENSOR_POWER_PIN, HIGH); // Power on the sensor
  gw.sleep(STABILIZATION_TIME);
  int moistureLevel = (1023 - analogRead(SENSOR_ANALOG_PIN)) / 10.23;
  digitalWrite(SENSOR_POWER_PIN, LOW);
  gw.send(msg.set(moistureLevel));
  long voltage = readVcc();
  gw.send(voltage_msg.set(voltage / 1000.0, 3)); // redVcc returns millivolts and set wants volts and how many decimals (3 in our case)
  gw.sendBatteryLevel(round((voltage - BATTERY_ZERO) * 100.0 / (BATTERY_FULL - BATTERY_ZERO)));
  gw.sleep(SLEEP_TIME);
}

long readVcc() {
  // From http://provideyourown.com/2012/secret-arduino-voltmeter-measure-battery-voltage/
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  ADMUX = _BV(MUX5) | _BV(MUX0);
#elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
  ADMUX = _BV(MUX3) | _BV(MUX2);
#else
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#endif

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  long result = (high << 8) | low;

  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}