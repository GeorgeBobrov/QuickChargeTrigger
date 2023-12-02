#include <Arduino.h>
#include <GyverEncoder.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <avr/pgmspace.h>
#include <QuickCharge.h>

constexpr byte pinEnc1 = 2;
constexpr byte pinEnc2 = 3;
constexpr byte pinEncButton = 4;
constexpr byte pinDmLowRes = 9;
constexpr byte pinDmHighRes = 10;
constexpr byte pinDpLowRes = 11;
constexpr byte pinDpHighRes = 12;

#define ARRLENGTH(x)  (sizeof(x) / sizeof((x)[0]))

Encoder encoder(pinEnc1, pinEnc2, pinEncButton); 
U8G2_SSD1306_128X64_NONAME_1_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

enum class DisplayMode: int8_t {
	main,
	setCurrent,
	// measureRin,
	viewLog,
	off
};

enum class SelectedActionOnMain: int8_t {
	startDischarge,
	setCurrent,
	// measureRin,
	viewLog,
	off
};

DisplayMode displayMode;
SelectedActionOnMain selectedActionOnMain;
QuickCharge QC (pinDpHighRes, pinDpLowRes, pinDmHighRes, pinDmLowRes, QC_CLASS_B);
int QCtype;

void isrCLK() {
	encoder.tick();
}
							
void isrDT() {
	encoder.tick();
}

void setup(void)
{
  Serial.begin(9600);
	encoder.setType(TYPE2);

	display.begin();

	attachInterrupt(0, isrCLK, CHANGE);
	attachInterrupt(1, isrDT, CHANGE);

}

byte QC_Volt_mode[] = { 
  QC_5V, 
  QC_9V, 
  QC_12V,
  QC_20V,
  QC_VAR
};

const char level1[] PROGMEM = "HI Z";
const char level2[] PROGMEM = "0 mV";
const char level3[] PROGMEM = "600 mV";
const char level4[] PROGMEM = "3300 mV";

const char* levelsStr[] = {level1, level2, level3, level4};


byte UsbPinLevel = 0;
bool changed = false;


void loop(void)
{

  encoder.tick();

  if (encoder.isClick()){
    QCtype = QC.begin();              
    changed = true;
  }

  if (encoder.isRight()) {
    if (UsbPinLevel < ARRLENGTH(QC_Volt_mode) - 1) 
      UsbPinLevel += 1;

    changed = true;
  }

  if (encoder.isLeft()) {
    if (UsbPinLevel > 0)
      UsbPinLevel -= 1;

    changed = true;
  }  

  if (changed) {
    changed = false;

    display.firstPage();
    do {
      display.setFont(u8g2_font_6x10_tf);
      constexpr byte charWidth = 6;
      constexpr byte charHeight = 10;

      display.setCursor(0, 10);

      switch (QCtype) {                    
        case QC_NA:   display.print(F("QC is not available")); break;
        case QC_GEN1: display.print(F("QC1.0 - (5V 2A)")); break;
        case QC_GEN2: display.print(F("QC2.0 or QC3.0")); break;
      }

      // char curLevelStr[strlen_P(levelsStr[UsbPinLevel]) + 1];
      // strcpy_P(curLevelStr, levelsStr[UsbPinLevel]);
      // display.setCursor(4*charWidth, 10);
      // display.print(curLevelStr);

      // if (play) {
      //   display.setCursor(0, 30);
      //   display.print(F("playing"));
      // }  



    } while ( display.nextPage() );
  }


}
