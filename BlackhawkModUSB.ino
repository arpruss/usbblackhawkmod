#include <USBComposite.h>
#include "debounce.h"

#define NO_VALUE 0xDEADBEEFul

#define AXISX PA0
#define AXISY PA1
#define AXISTHROTTLE PA2
const int buttons[] = {PA4,PA5,PA6,PA7,PB3,PB4,PB5};
#define NUM_BUTTONS (sizeof buttons / sizeof *buttons)
Debounce digital0(buttons[0],0);
Debounce digital1(buttons[1],0);
Debounce digital2(buttons[2],0);
Debounce digital3(buttons[3],0);
Debounce digital4(buttons[4],0);
Debounce digital5(buttons[5],0);
Debounce digital6(buttons[6],0);
Debounce* digital[] = {&digital0,&digital1,&digital2,&digital3,&digital4,&digital5,&digital6};

#define HYSTERESIS 10 // shifts smaller than this are rejected
#define MAX_HYSTERESIS_REJECTIONS 8 // unless we've reached this many of them, and then we use an average
#define READ_ITERATIONS 50
#define AXIS_CONVERT(x) ((x)>>2)

uint16 analogRead2(uint8 pin) {
  return adc_read(pin == AXISX ? &adc1 : &adc2, PIN_MAP[pin].adc_channel);
}

//#define SERIAL_DEBUG

#ifdef SERIAL_DEBUG
USBCompositeSerial debug;
#define DEBUG(...) debug.println(__VA_ARGS__);
#else
#define DEBUG(...)
#endif

class AnalogPort {
  public:
    uint32 port;
    uint32 oldValue = NO_VALUE;
    uint32 rejectedCount = 0;
    uint32 rejectedSum = 0;
    uint32 getValue() {
      uint32 v = 0;

      //nvic_globalirq_disable();
      for (uint32 i = 0 ; i < READ_ITERATIONS ; i++)
        v += analogRead2(port);
      //nvic_globalirq_enable();
      v = (v + READ_ITERATIONS / 2) / READ_ITERATIONS;

      if (oldValue != NO_VALUE && v != oldValue && v < oldValue + HYSTERESIS && oldValue < v + HYSTERESIS) {
        if (rejectedCount > 0) {
          rejectedCount++;
          rejectedSum += v;
          if (rejectedCount >= MAX_HYSTERESIS_REJECTIONS) {
            v = (rejectedSum + MAX_HYSTERESIS_REJECTIONS / 2) / MAX_HYSTERESIS_REJECTIONS;
            rejectedCount = 0;
          }
          else {
            v = oldValue;
          }
        }
        else {
          rejectedCount = 1;
          rejectedSum = v;
          v = oldValue;
        }
      }
      else {
        rejectedCount = 0;
      }

      oldValue = v;
      return 4095 - v;
    };

    AnalogPort(uint32 _port) {
      port = _port;
    };
};

AnalogPort axisX(AXISX);
AnalogPort axisY(AXISY);
AnalogPort axisThrottle(AXISTHROTTLE);

USBHID HID;
HIDJoystick joy(HID);

void setup() {
  // default is 55_5
  adc_set_sample_rate(ADC1, ADC_SMPR_239_5);
  adc_set_sample_rate(ADC2, ADC_SMPR_239_5);
  USBComposite.setProductId(0xb2e3);
  USBComposite.setManufacturerString("Omega Centauri");
  USBComposite.setProductString("BlackhawkModUSB");

#if 0
  for (uint32 i = 0 ; i < BOARD_NR_GPIO_PINS ; i++) {
    if (i != PA11 && i != PA12) {
      pinMode(i, INPUT_PULLDOWN);
    }
  }
#endif

  pinMode(axisX.port, INPUT_ANALOG);
  pinMode(axisY.port, INPUT_ANALOG);
  pinMode(axisThrottle.port, INPUT_ANALOG);

  for (uint32 i = 0 ; i < NUM_BUTTONS ; i++)
    pinMode(buttons[i], INPUT_PULLUP);


//  pinMode(LED, OUTPUT);
//  digitalWrite(LED, 1);
  
  HID.registerComponent();
#ifdef SERIAL_DEBUG
  debug.registerComponent();
#endif    
  joy.setManualReportMode(true);
  USBComposite.begin();
  
  while (!USBComposite);
}

#ifdef SERIAL_DEBUG
uint32 countStart = 0;
uint32 count = 0;
#endif

void loop() {
  joy.X(AXIS_CONVERT(axisX.getValue()));
  joy.Y(AXIS_CONVERT(axisY.getValue()));
  joy.sliderRight(1023-AXIS_CONVERT(axisThrottle.getValue()));
  
  for (uint32 i = 0 ; i < NUM_BUTTONS ; i++ ) {
    joy.button(i+1, digital[i]->getState());
  }
  joy.sendReport();
#ifdef SERIAL_DEBUG
  count++;
  if (count == 1000) {
    uint32 t = millis() - countStart;
    char out[10];
    out[9] = 0;
    char *p = out + 8;
    while (1) {
      *p = t % 10 + '0';
      t /= 10;
      if (t == 0) {
        debug.write(p);
        debug.write("\n");
        break;
      }
      p--;
    }
    count = 0;
    countStart = millis();
  }
#endif

}

