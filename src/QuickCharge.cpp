#include <Arduino.h>
#include <GyverEncoder.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <avr/pgmspace.h>
#include "QuickCharge2.h"

constexpr byte pinEnc1 = 2;
constexpr byte pinEnc2 = 3;
constexpr byte pinEncButton = 4;
constexpr byte pinDmLowRes = 9;
constexpr byte pinDmHighRes = 10;
constexpr byte pinDpLowRes = 11;
constexpr byte pinDpHighRes = 12;

constexpr byte pinVoltageMeasure = A7;

constexpr float internalReferenceVoltage = 1.024;
// Resistor divider for measure accum voltage
constexpr float Rup = 101300.0;
constexpr float Rdown = 4720.0;
constexpr float resistorDividerCoef = (Rup + Rdown) / Rdown;
//The LGT8F328P allows you to increase the resolution to 11 or 12 bits.
//Use analogReadResolution(ADC_RESOLUTION) in setup;
//Need to load ref pin on LGT8F328P to work normal
constexpr byte ADC_RESOLUTION = 11;
constexpr int16_t MAX_ADC_VALUE = bit(ADC_RESOLUTION) - 1;

#define ARRLENGTH(x)  (sizeof(x) / sizeof((x)[0]))

Encoder encoder(pinEnc1, pinEnc2, pinEncButton); 
U8G2_SSD1306_128X64_NONAME_1_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
QuickCharge QC (pinDpHighRes, pinDpLowRes, pinDmHighRes, pinDmLowRes, QC_CLASS_B);

enum class ClickAction: int8_t {
	testQC,
	enterSelectedQC_Mode,
	exitVarMode
};
ClickAction clickAction;

enum class SelectAction: int8_t {
	selectQC_Mode,
	selectVarVoltage
};
SelectAction selectAction;
byte QC_ModeSelecting = 0;


int QC_AdapterType = 0;
void setQC_AdapterType(byte aQC_AdapterType) {
	QC_AdapterType = aQC_AdapterType;

	if (QC_AdapterType == QC_GEN2) {
		selectAction = SelectAction::selectQC_Mode;
		clickAction = ClickAction::enterSelectedQC_Mode;
	} else {
		clickAction = ClickAction::testQC;
		QC_ModeSelecting = 0;
	}
}

enum class TestQC_Mode: int8_t {
	wasntTested,
	pending,
	complited
} testQC_Mode;

unsigned long lastTimeDrawMeasuredVoltage_us = 0;
bool redraw;


void isrCLK() {
	encoder.tick();
}
							
void isrDT() {
	encoder.tick();
}

void setup(void)
{
	// pinMode(A0, INPUT_PULLUP);
	pinMode(A1, INPUT_PULLUP);
	pinMode(A2, INPUT_PULLUP);
	pinMode(A3, INPUT_PULLUP);
	pinMode(A6, INPUT_PULLUP);
	pinMode(pinVoltageMeasure, INPUT);

	pinMode(5, INPUT_PULLUP);
	pinMode(6, INPUT_PULLUP);
	pinMode(7, INPUT_PULLUP);
	pinMode(8, INPUT_PULLUP);


	Serial.begin(9600);
	encoder.setType(TYPE2);

	display.begin();

	attachInterrupt(0, isrCLK, CHANGE);
	attachInterrupt(1, isrDT, CHANGE);

	analogReference(INTERNAL1V024);
	analogReadResolution(ADC_RESOLUTION);
}



void loop(void)
{
	if (testQC_Mode == TestQC_Mode::pending) {
		setQC_AdapterType(QC.begin());      
		testQC_Mode = TestQC_Mode::complited;
	}

	encoder.tick();
	if (encoder.isHolded()) {
		if (clickAction == ClickAction::testQC) { // for debug, pretending Adapter is QC_GEN2
			pinMode(QC._dp_h, OUTPUT);    
			pinMode(QC._dp_l, OUTPUT);    
			pinMode(QC._dm_h, OUTPUT);    
			pinMode(QC._dm_l, OUTPUT);  
			QC._5vOnly = false;  
			QC._set_dp(SET_600MV);        

			setQC_AdapterType(QC_GEN2);
		} else {
			QC_ModeSelecting = QC_5V;
			QC.setMode(QC_ModeSelecting);
			testQC_Mode = TestQC_Mode::pending;
		}

		redraw = true;
	}

	if (encoder.isClick()) {
		switch (clickAction) {
			case ClickAction::testQC: 
				testQC_Mode = TestQC_Mode::pending;
			break;

			case ClickAction::enterSelectedQC_Mode: 
				QC.setMode(QC_ModeSelecting);

				if (QC.modeApplied == QC_VAR) {
					selectAction = SelectAction::selectVarVoltage;
					clickAction = ClickAction::exitVarMode;
				} else {
					selectAction = SelectAction::selectQC_Mode;
				}
			break;

			case ClickAction::exitVarMode: 
				selectAction = SelectAction::selectQC_Mode;
				clickAction = ClickAction::enterSelectedQC_Mode;
				QC_ModeSelecting = QC_5V;
				QC.setMode(QC_ModeSelecting);
			break;
		}

		redraw = true;
	}

	if (QC_AdapterType == QC_GEN2) {
		switch (selectAction) {
			case SelectAction::selectQC_Mode: 
				if (encoder.isRight()) 
					if (QC_ModeSelecting < QC_VAR) 
						QC_ModeSelecting += 1;

				if (encoder.isLeft()) 
					if (QC_ModeSelecting > 0)
						QC_ModeSelecting -= 1;

			break;

			case SelectAction::selectVarVoltage: 
				if (encoder.isRight()) 
					QC.inc();

				if (encoder.isLeft()) 
					QC.dec();

			break;
		}
	} else { // Clear flags
		encoder.isRight();
		encoder.isLeft();
	}

	if (encoder.isTurn()) // Clear turn flag before displaying
		redraw = true;
	 

	unsigned long curTime_us = micros();
	unsigned long periodFromLastMeasure_us = curTime_us - lastTimeDrawMeasuredVoltage_us;
	if ((periodFromLastMeasure_us >= 200000) || redraw) {
		lastTimeDrawMeasuredVoltage_us = curTime_us;
		redraw = false;

		// int voltageADC = analogRead(pinVoltageMeasure);
		// Averaging a number of measurements for more stable readings
		constexpr byte averCount = 5;
		int AdcSum = 0;
		for (byte i = 0; i < averCount; i++) 
			AdcSum += analogRead(pinVoltageMeasure);
		float voltageADC = float(AdcSum) / averCount;		

		display.firstPage();
		do {
			encoder.tick();
			if (encoder.isTurn()) break;

			display.setFont(u8g2_font_6x10_tr);
			// byte fontBaseline = abs(display.getFontDescent());
			display.setFontPosBottom(); // and dont messing with fontBaseline

			constexpr byte charWidth = 6;
			byte charHeight = /*10*/display.getMaxCharHeight();
			constexpr byte displayWidth = 128;

			constexpr byte additionalSpacingForFirstLine = 3;
			byte y = charHeight + additionalSpacingForFirstLine;
			byte x = 0;

			display.setCursor(x, y);
			display.print(F("QuickCharge"));

			x = 13*charWidth;
			display.setCursor(x, y);

			if ((testQC_Mode == TestQC_Mode::wasntTested) || (testQC_Mode == TestQC_Mode::complited))
				switch (clickAction) {
					case ClickAction::testQC:
						display.print(F("Test")); 
						display.drawFrame(x - 3, y - charHeight - 1, 4*charWidth + 5, charHeight + 3);
					break;
				}    


			constexpr byte yellowHeaderHeight = 16;
			y = yellowHeaderHeight + charHeight;
			display.setCursor(0, y);

			if (testQC_Mode == TestQC_Mode::complited) 
				switch (QC_AdapterType) {                    
					case QC_NA:   display.print(F("QC is not available")); break;
					case QC_GEN1: display.print(F("QC1 - (5V 2A)")); break;
					case QC_GEN2: display.print(F("QC2/QC3. Set Mode:")); break;
				}

			display.setFont(u8g2_font_6x13_tr);
			charHeight = display.getMaxCharHeight();


			if ((QC_AdapterType == QC_GEN2) && (selectAction == SelectAction::selectQC_Mode)) {
				constexpr byte itemCount = 5;
				constexpr byte spacingBetweenItems = 4;
				constexpr byte modeCaptionWidth = /*7*charWidth + 1*/0;
				constexpr byte modeItemsWidth = displayWidth - modeCaptionWidth - (spacingBetweenItems * (itemCount - 1));
				constexpr float modeItemWidth = (float(modeItemsWidth) / itemCount);
				y += (charHeight + 4);

				for (byte itemInd = 0; itemInd < itemCount; itemInd++) {
					String str;
					switch (itemInd) {                    
						case QC_5V:  str = F("5V"); break;
						case QC_9V:  str = F("9V"); break;
						case QC_12V: str = F("12V"); break;
						case QC_20V: str = F("20V"); break;
						case QC_VAR: str = F("Var"); break;
					}

					byte strWidth = str.length() * charWidth;
					
					x = round(modeCaptionWidth + (modeItemWidth + spacingBetweenItems)*itemInd);
					constexpr byte lastBlankPixelInChar = 1;
					byte offsetForCentering = (modeItemWidth - (strWidth - lastBlankPixelInChar)) / 2;
					display.setCursor(x + offsetForCentering, y);

					if (itemInd == QC.modeApplied) 
						display.setFont(u8g2_font_6x13B_tr);
					else	
						display.setFont(u8g2_font_6x13_tr);

					display.print(str);

					if (itemInd == QC.modeApplied) {
						display.drawFrame(x, y - charHeight - 1, round(modeItemWidth), charHeight + 2);
					} else 
						if (itemInd == QC_ModeSelecting) {
		// drawDottedLines feature added in \U8g2\src\clib\u8g2_hvline.c and \U8g2\src\clib\u8g2.h
							display.getU8g2()->drawDottedLines = true;
							display.drawFrame(x, y - charHeight - 1, round(modeItemWidth), charHeight + 2);
							display.getU8g2()->drawDottedLines = false;
						} else {
							display.getU8g2()->drawDottedLines = true;
							display.drawHLine(x, y, round(modeItemWidth));
							display.getU8g2()->drawDottedLines = false;
						}	

				}

			}


			if (selectAction == SelectAction::selectVarVoltage) {
				y += (charHeight + 4);
				x = 0;
				display.setCursor(x, y);
				display.print(F("Set V:"));
				
				float voltage = QC.voltage() / 1000.0;
				byte emptyCharOffset = (voltage < 10) ? charWidth : 0;

				x = 7.5*charWidth;	
				display.setCursor(x + emptyCharOffset, y);
				display.print(voltage);
				display.getU8g2()->drawDottedLines = true;
				display.drawFrame(x - 3, y - charHeight - 1, 5*charWidth + 5, charHeight + 3);
				display.getU8g2()->drawDottedLines = false;

				display.setFont(u8g2_font_6x10_tr);
				charHeight = display.getMaxCharHeight();
				x = 14*charWidth;
				y -= 1;
				display.setCursor(x, y);
				display.print(F("ExitVar")); 
				display.drawFrame(x - 2, y - charHeight - 1, 7*charWidth + 3, charHeight + 3);
				y += 1;
			} 


			{
				float measuredVoltage = voltageADC * (resistorDividerCoef * internalReferenceVoltage / MAX_ADC_VALUE);

				display.setFont(u8g2_font_6x13_tr);
				charHeight = display.getMaxCharHeight();

				y += (charHeight + 4);
				x = 0;
				display.setCursor(x, y);
				display.print(F("Meas V:"));

				byte emptyCharOffset = (measuredVoltage < 10) ? charWidth : 0;
				x = 7.5*charWidth;
				display.setCursor(x + emptyCharOffset, y);
				display.print(measuredVoltage);				
			}


			// display.setFont(u8g2_font_6x13B_tn);
			// display.setCursor(charWidth*3, y);
			// display.print(F("12"));

			// open_iconic_arrow_1x1

			encoder.tick();
			if (encoder.isTurn()) break;

		} while ( display.nextPage() );

	}

}
