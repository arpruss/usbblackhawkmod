 #include <USBComposite.h>
#include "debounce.h"

#define NO_VALUE 0xDEADBEEFul

#define AXISX PA0
#define AXISY PA1
#define AXISTHROTTLE PA2
const unsigned buttons[] = {PA4,PA5,PA6,PA7,PB3,PB4,PB5};
const unsigned joyMap[] = {1,2,3,4,5,6,7};
const unsigned x360Map[] = {XBOX_A,XBOX_B,XBOX_X,XBOX_Y,XBOX_LSHOULDER,XBOX_RSHOULDER,XBOX_START};
boolean x360 = false;
USBHID HID;
HIDJoystick joy(HID);
USBXBox360W<1> XBox360;
#define NUM_BUTTONS (sizeof buttons / sizeof *buttons)
Debounce digital0(buttons[0],0);
Debounce digital1(buttons[1],0);
Debounce digital2(buttons[2],0);
Debounce digital3(buttons[3],0);
Debounce digital4(buttons[4],0);
Debounce digital5(buttons[5],0);
Debounce digital6(buttons[6],0);
Debounce* digital[] = {&digital0,&digital1,&digital2,&digital3,&digital4,&digital5,&digital6};
#define LAST_DIGITAL digital6

#define HYSTERESIS 10 // shifts smaller than this are rejected
#define MAX_HYSTERESIS_REJECTIONS 8 // unless we've reached this many of them, and then we use an average
#define READ_ITERATIONS 50
#define AXIS_CONVERT(x) ((x)>>2)
#define AXIS_CONVERT_X360(x) (((int32)(x)-2048)*32767/2048)

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
int32 cx,cy;

uint16 axisConvertJoystick(int32 x, int32  cx) {
  x += 2048 - cx;
  if (x<0)
    x = 0;
  else if (x>=4096)
    x = 4095;
  return x * 1023 / 4095;
}

int16 axisConvertX360(int32 x, int32 cx) {
  x = (x - cx) * 32767 / 2048;
  if (x<-32768)
    return -32768;
  else if (x>32767)
    return 32767;
  else
    return x;  
}

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

  if (LAST_DIGITAL.getRawState()) {
    x360 = true;
    XBox360.begin();
  }
  else {
    x360 = false;
    HID.registerComponent();
  #ifdef SERIAL_DEBUG
    debug.registerComponent();
  #endif    
    joy.setManualReportMode(true);
  }
  USBComposite.begin();    

  for (uint32 i=0;i<100;i++) {
    cx += axisX.getValue();
    cy += axisY.getValue();
  }
  cx = (cx+50)/100;
  cy = (cy+50)/100;    
  
  while (!USBComposite);

  
}

#ifdef SERIAL_DEBUG
uint32 countStart = 0;
uint32 count = 0;
#endif

void loop() {
  if (x360) {
    XBox360.controllers[0].X(axisConvertX360(axisX.getValue(),cx));
    XBox360.controllers[0].Y(-axisConvertX360(axisY.getValue(),cy));
    XBox360.controllers[0].sliderRight(axisThrottle.getValue()>>4);
    
    for (uint32 i = 0 ; i < NUM_BUTTONS ; i++ ) {
      XBox360.controllers[0].button(x360Map[i], digital[i]->getState());
    }
    XBox360.controllers[0].send();
  }
  else {
    joy.X(axisConvertJoystick(axisX.getValue(),cx));
    joy.Y(axisConvertJoystick(axisY.getValue(),cy));
    joy.sliderRight(1023-axisConvertJoystick(axisThrottle.getValue(),2048));
    
    for (uint32 i = 0 ; i < NUM_BUTTONS ; i++ ) {
      joy.button(joyMap[i], digital[i]->getState());
    }
    joy.send();
  }
}

