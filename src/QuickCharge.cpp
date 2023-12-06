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
	checkQC,
	toggleVarMode,
};
ClickAction clickAction;

enum class SelectAction: int8_t {
	selectQCmode,
	selectVarVoltage
};
SelectAction selectAction;
byte QC_VoltMode = 0;
bool QC_VarMode = false;

int QC_AdapterType = 0;
bool redraw = false;

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
	redraw = true;
}



constexpr byte averCount = 5;
int16_t AdcAveraging[averCount];
byte AdcAveragingInd = 0;

void loop(void)
{
	encoder.tick();
	if (encoder.isHolded()) { // for debug
		pinMode(QC._dp_h, OUTPUT);    
        pinMode(QC._dp_l, OUTPUT);    
		pinMode(QC._dm_h, OUTPUT);    
		pinMode(QC._dm_l, OUTPUT);    
		QC._set_dp(SET_600MV);        

		QC_AdapterType = QC_GEN2;
	}

	switch (clickAction) {
		case ClickAction::checkQC: 
			if (encoder.isClick()) 
				QC_AdapterType = QC.begin();              
		break;

		case ClickAction::toggleVarMode: 
			if (encoder.isClick()) {
				QC_VarMode = !QC_VarMode;
				if (QC_VarMode)
					selectAction = SelectAction::selectVarVoltage;
				else
					selectAction = SelectAction::selectQCmode;
			}
		break;
	}


	if (QC_AdapterType == QC_GEN2) {
		switch (selectAction) {
			case SelectAction::selectQCmode: {
				bool changed = false;

				if (encoder.isRight()) {
					if (QC_VoltMode < QC_VAR) 
						QC_VoltMode += 1;
					
					changed = true;
				}

				if (encoder.isLeft()) {
					if (QC_VoltMode > 0)
						QC_VoltMode -= 1;

					changed = true;
				}  

				if (changed) {
					QC.setMode(QC_VoltMode);

					if (QC_VoltMode == QC_VAR) 
						clickAction = ClickAction::toggleVarMode;
					else
						clickAction = ClickAction::checkQC;
				}
			} break;

			case SelectAction::selectVarVoltage: 
				if (encoder.isRight()) 
					QC.inc();

				if (encoder.isLeft()) 
					QC.dec();

			break;
		}
	}

	encoder.isTurn(); // Clear turn flag before displaying

	// unsigned long curTime_us = micros();
	// unsigned long periodFromLastMeasure_us = curTime_us - lastTimeMeasure_us;
	// if (periodFromLastMeasure_us >= 50000) {
	// 	lastTimeMeasure_us = curTime_us;

	// if (redraw) 
	{
		display.firstPage();
		do {
			encoder.tick();
			if (encoder.isTurn()) break;

			display.setFont(u8g2_font_6x10_tf);
			// display.setFontPosBottom();
			constexpr byte charWidth = 6;
			constexpr byte charHeight = 10;

			byte fontBaseline = abs(display.getFontDescent());
			constexpr byte additionalSpacingForFirstLine = 3;
			constexpr byte yellowHeaderHeight = 16;
			constexpr byte spacingBetweenLines = 2; 


			byte y = charHeight - fontBaseline + additionalSpacingForFirstLine;

			display.setCursor(2, y);
			switch (clickAction) {
				case ClickAction::toggleVarMode:
					if (QC_VarMode) 
						display.print(F("Exit  Var Mode")); 
					else
						display.print(F("Go to Var Mode")); 
				break;
				case ClickAction::checkQC:
					display.print(F("Check QuickCharge")); 
				break;
			}    
			display.drawFrame(0, y-9, 127, 13);

			y = yellowHeaderHeight + charHeight - fontBaseline;
			display.setCursor(0, y);
			switch (QC_AdapterType) {                    
				case QC_NA:   display.print(F("QC is not available")); break;
				case QC_GEN1: display.print(F("QC1.0 - (5V 2A)")); break;
				case QC_GEN2: display.print(F("QC2.0 or QC3.0")); break;
			}

			y += (charHeight + spacingBetweenLines);
			display.setCursor(0, y);
			display.print(F("Mode,V:"));

			byte xSelectedMode = 0;
			constexpr byte modeCaptionWidth = 8*charWidth;
			constexpr byte modeItemsWidth = 128 - modeCaptionWidth;
			constexpr float modeItemWidth = modeItemsWidth / 5.0;
			for (byte itemInd = 0; itemInd < 5; itemInd++) {
				byte x = round(modeCaptionWidth + modeItemWidth*itemInd);
				display.setCursor(x + 1, y);

				switch (itemInd) {                    
					case QC_5V:  display.print(F(" 5")); break;
					case QC_9V:  display.print(F(" 9")); break;
					case QC_12V: display.print(F("12")); break;
					case QC_20V: display.print(F("20")); break;
					case QC_VAR: display.print(F("Var")); break;
				}
				if (itemInd == QC_VoltMode)
					xSelectedMode = x;
				else	
					display.drawHLine(x + 1, y + fontBaseline, round(modeItemWidth) - 2);
			}

// drawDottedLines feature added in \U8g2\src\clib\u8g2_hvline.c and \U8g2\src\clib\u8g2.h
			display.getU8g2()->drawDottedLines = true;
			display.drawFrame(xSelectedMode, y-9, round(modeItemWidth), 12);
			display.getU8g2()->drawDottedLines = false;


			// int voltageADC = analogRead(pinVoltageMeasure);
			// Averaging a number of measurements for more stable readings
			int curADC = analogRead(pinVoltageMeasure);

			AdcAveraging[AdcAveragingInd] = curADC;
			AdcAveragingInd++;
			if (AdcAveragingInd >= averCount) AdcAveragingInd = 0;

			int AdcSum = 0;
			for (byte i = 0; i < averCount; i++) 
				AdcSum += AdcAveraging[i];

			float voltageADC = float(AdcSum) / averCount;

			float voltage_V = voltageADC * (resistorDividerCoef * internalReferenceVoltage / MAX_ADC_VALUE);

			y += (charHeight + spacingBetweenLines);
			display.setCursor(0, y);
			display.print(F("Volt meas"));
			display.setCursor(charWidth*10, y);
			display.print(voltage_V);

			y += (charHeight + spacingBetweenLines);
			if (QC_VarMode) {
				display.setCursor(10*charWidth, y);
				display.print(QC.voltage());
			}
			display.setCursor(0, y);
			display.print(fontBaseline);
			// display.setCursor(charWidth*7, y);
			// display.print(voltageADC);
			// display.setCursor(charWidth*14, y);
			// display.print(AdcSum);

			encoder.tick();
			if (encoder.isTurn()) break;

		} while ( display.nextPage() );

		redraw = false;
	}


}
