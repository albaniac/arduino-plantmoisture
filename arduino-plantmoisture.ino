#define round(x) ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))
#define N_ELEMENTS(array) (sizeof(array)/sizeof((array)[0]))

#define CHILD_ID_MOISTURE 0
#define CHILD_ID_BATTERY 1
#define SLEEP_TIME 1800000 // Sleep time between reads (in milliseconds)
#define THRESHOLD 1.1 // Only make a new reading with reverse polarity if the change is larger than 10%.
#define STABILIZATION_TIME 1000 // Let the sensor stabilize before reading
#define BATTERY_FULL 3143 // 2xAA usually give 3.143V when full
#define BATTERY_ZERO 2340 // 2.34V limit for 328p at 8MHz. 1.9V, limit for nrf24l01 without step-up. 2.8V limit for Atmega328 with default BOD settings.
#define MY_RADIO_NRF24
const int SENSOR_ANALOG_PINS[] = {A0, A1}; // Sensor is connected to these two pins. Avoid A3 if using ATSHA204. A6 and A7 cannot be used because they don't have pullups.

#define MY_SPLASH_SCREEN_DISABLED
#define MY_TRANSPORT_WAIT_READY_MS (30000) // Don't stay awake for more than 30s if communication is broken
#define MY_DEBUG
#include <MySensors.h>

MyMessage msg(CHILD_ID_MOISTURE, V_HUM);
MyMessage voltage_msg(CHILD_ID_BATTERY, V_VOLTAGE);
long oldvoltage = 0;
byte direction = 0;
int oldMoistureLevel = -1;

void presentation() {
  sendSketchInfo("Plant moisture w bat", "1.6");
  present(CHILD_ID_MOISTURE, S_HUM);
  delay(250);
  present(CHILD_ID_BATTERY, S_CUSTOM);

}

void setup()
{
  for (unsigned int i = 0; i < N_ELEMENTS(SENSOR_ANALOG_PINS); i++) {
    pinMode(SENSOR_ANALOG_PINS[i], OUTPUT);
    digitalWrite(SENSOR_ANALOG_PINS[i], LOW);
  }
}

void loop()
{
  int moistureLevel = readMoisture();

  // Send rolling average of 2 samples to get rid of the "ripple" produced by different resistance in the internal pull-up resistors
  // See http://forum.mysensors.org/topic/2147/office-plant-monitoring/55 for more information
  if (oldMoistureLevel == -1) { // First reading, save current value as old
    oldMoistureLevel = moistureLevel;
  }
  if (moistureLevel > (oldMoistureLevel * THRESHOLD) || moistureLevel < (oldMoistureLevel / THRESHOLD)) {
    // The change was large, so it was probably not caused by the difference in internal pull-ups.
    // Measure again, this time with reversed polarity.
    moistureLevel = readMoisture();
  }
  send(msg.set((moistureLevel + oldMoistureLevel) / 2.0 / 10.23, 1));
  oldMoistureLevel = moistureLevel;
  long voltage = readVcc();
  if (oldvoltage != voltage) { // Only send battery information if voltage has changed, to conserve battery.
    send(voltage_msg.set(voltage / 1000.0, 3)); // redVcc returns millivolts. Set wants volts and how many decimals (3 in our case)
    sendBatteryLevel(round((voltage - BATTERY_ZERO) * 100.0 / (BATTERY_FULL - BATTERY_ZERO)));
    oldvoltage = voltage;
  }
  sleep(SLEEP_TIME);
}

int readMoisture() {
  pinMode(SENSOR_ANALOG_PINS[direction], INPUT_PULLUP); // Power on the sensor
  analogRead(SENSOR_ANALOG_PINS[direction]);// Read once to let the ADC capacitor start charging
  sleep(STABILIZATION_TIME);
  int moistureLevel = (1023 - analogRead(SENSOR_ANALOG_PINS[direction]));

  // Turn off the sensor to conserve battery and minimize corrosion
  pinMode(SENSOR_ANALOG_PINS[direction], OUTPUT);
  digitalWrite(SENSOR_ANALOG_PINS[direction], LOW);

  direction = (direction + 1) % 2; // Make direction alternate between 0 and 1 to reverse polarity which reduces corrosion
  return moistureLevel;
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

