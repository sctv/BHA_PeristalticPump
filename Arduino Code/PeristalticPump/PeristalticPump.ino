/*
	This file is part of Waag Society's BioHack Academy Code.

	Waag Society's BioHack Academy Code is free software: you can 
	redistribute it and/or modify it under the terms of the GNU 
	General Public License as published by the Free Software 
	Foundation, either version 3 of the License, or (at your option) 
	any later version.

	Waag Society's BioHack Academy Code is distributed in the hope 
	that it will be useful, but WITHOUT ANY WARRANTY; without even 
	the implied warranty of MERCHANTABILITY or FITNESS FOR A 
	PARTICULAR PURPOSE.  See the GNU General Public License for more 
	details.

	You should have received a copy of the GNU General Public License
	along with Waag Society's BioHack Academy Code. If not, see 
	<http://www.gnu.org/licenses/>.
*/

/*
Peristaltic Pump:

Commands:
move <speed> Set continuous pump rotation speed (rpm)

*/


#include <Wire.h> // Needed for I2C connection
#include "LiquidCrystal_I2C.h" // Needed for operating the LCD screen
#include <TimerOne.h>
#include <SoftwareSerial.h>

#define LED_PIN 13      // the number of Arduino's onboard LED pin

class RotaryEncoder
{
  byte state;
  byte pin0, pin1;
  int value;

  byte readState()
  {
    return (digitalRead(pin0) == HIGH ? 1u : 0u)
         | (digitalRead(pin1) == HIGH ? 2u : 0u);
  }
public:
  RotaryEncoder(int p0, int p1) :
    pin0(p0), pin1(p1), value(0), state(0) {}
  
  void init()
  {
    pinMode(pin0, INPUT);
    pinMode(pin1, INPUT);
    digitalWrite(pin0, 1);  // enable internal pullup
    digitalWrite(pin1, 1);  // enable internal pullup
    value = 0;
    state = readState();
  }

  bool poll()
  {
    // State transition table
    static char tbl[16] =
    { 0, +1, -1, 0,
      // position 3 = 00 to 11, can't really do anythin, so 0
      -1, 0, -2, +1,
      // position 2 = 01 to 10, assume a bounce, should be 01 -> 00 -> 10
      +1, +2, 0, -1,
      // position 1 = 10 to 01, assume a bounce, should be 10 -> 00 -> 01
      0, -1, +1, 0
      // position 0 = 11 to 10, can't really do anything
    };
  
    byte t = readState();
    char movement = tbl[(state << 2) | t];
    if (movement != 0)
    {
      value += movement;
      state = t;
      return true;
    }
    return false;
  }
  
  int getValue()
  {
    return value;
  }
};

RotaryEncoder rotaryEncoderA (2,3);
RotaryEncoder rotaryEncoderB (A2,A3);


// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27,16,2);

byte ledstate = false; // Blinking indicator LED

String buffer;
byte readout;

long last_time;
int stepper_timer=0;
int step_time = 0;
long steps=0;
int  update_timer=0;

byte stepper_en = 0;

const byte PIN_STEP=7; 
const byte PIN_ENABLE=4;
const byte PIN_DIR=8;

const byte PIN_MOSFET0 = 5;
const byte PIN_MOSFET1 = 6;


int mosfet_switch_timer=0;
byte mosfet_index=0;

int speed = 0;

void scanI2C()
{
  for (int i=0;i<127;i++) {
    Wire.beginTransmission(i);
    byte err = Wire.endTransmission();
    if(err == 0) {
      Serial.print("Device at: " ); Serial.println(i, HEX);
    }
  }
}

void setupLCD() {
  // Initialize I2C
  Wire.begin();
  // Initialize the LCD and print a message
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(F("BioHack Academy"));
  lcd.setCursor(0,1);
  lcd.print(F("Peristaltic Pump"));
}

void setup() {
  // Open serial connection and print a message
  Serial.begin(9600);
  Serial.println(F("BioHackAcademy Peristaltic Pump"));

  rotaryEncoderA.init();
  rotaryEncoderB.init();
  
  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_ENABLE,OUTPUT);
  pinMode(PIN_DIR,OUTPUT);
  
  pinMode(PIN_MOSFET0, OUTPUT);
  pinMode(PIN_MOSFET1, OUTPUT);
  
  stepper_en=1;
  digitalWrite(PIN_ENABLE,1-stepper_en);
  
  setupLCD();
}

void updateLCD()
{
//  lcd.clear(); 
  lcd.setCursor(0,0);
  lcd.print(F("Speed: "));
  lcd.print( rotaryEncoderA.getValue() );
  lcd.print(F(" %     "));

/*  lcd.setCursor(0,1);
  lcd.print(F("Volume: "));//clear row
  
  const float pi =3.141593f;
  char strbuf[10];
  dtostrf(0.0f, 4, 2, strbuf);
  lcd.print(strbuf);
  lcd.print( " cm3/s");*/
}

void loop() {
  // put your main code here
  long time = millis();
  long dt = time - last_time;
  last_time = time;

  ledstate = 1-ledstate;
  digitalWrite(LED_PIN, ledstate); // alternate between 0 and 1   

  if(stepper_en) {  
    stepper_timer+=dt;
    update_timer+=dt;
  }
    
  if (stepper_timer > step_time && stepper_en) {
    stepper_timer-=step_time;
    digitalWrite(PIN_STEP, 1);
    delay(1);
    digitalWrite(PIN_STEP, 0);
    
    steps++;
  }
  
  mosfet_switch_timer+=dt;
  if (mosfet_switch_timer > 1000) {
    mosfet_switch_timer-=1000;
    
    mosfet_index=1-mosfet_index;
    digitalWrite(PIN_MOSFET0, mosfet_index==0);
    digitalWrite(PIN_MOSFET1, mosfet_index==1);
    updateLCD();
  }
  
  if (update_timer>300 && stepper_en) {
    update_timer-=300;
    Serial.print("steps: ");
    Serial.print(steps);
   
    Serial.print("   A0: ");
    Serial.print(analogRead(A0));

    Serial.print("    A1: ");
    Serial.print(analogRead(A1));
    Serial.println();
  }

  if (rotaryEncoderA.poll()) {
    Serial.print("Encoder A: ");
    Serial.print(rotaryEncoderA.getValue());
    Serial.println();
    if(rotaryEncoderA.getValue() == 0) {
      step_time = 10000000;
    }
    else {
        step_time = 100/(rotaryEncoderA.getValue());
    }
  }
  
  if (rotaryEncoderB.poll()) {
    Serial.print("Encoder B: ");
    Serial.print(rotaryEncoderB.getValue());
    Serial.println();
  }
      
  while (Serial.available()>0) {
    char c = (char)Serial.read();
    if (c == '\n') {
      if(buffer.startsWith("id")) {
        Serial.println("id:peristaltic-pump"); // so the bioreactor can figure out what is connected 
      }
      else if(buffer.startsWith("i2c")) {
        scanI2C();
      }    
      else if(buffer.startsWith("sp")) {
        int sp = buffer.substring(3).toInt();
        
        Serial.print("setting step time to : ");
        Serial.println(sp);
        
        step_time = sp;
      } else if (buffer.startsWith("en")) {
        stepper_en = 1-stepper_en;
        digitalWrite(PIN_ENABLE,1-stepper_en);
        Serial.println((int)stepper_en);
      } else {
        Serial.println("Unknown cmd.");
      }
      buffer="";
    } else buffer+=c;
  }
}




