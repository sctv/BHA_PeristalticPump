/*
Peristaltic Pump:

Commands:
sp <speed> Set continuous pump rotation speed (rpm)
rotate <1/10 revolutions> Rotate fixed number of rotations
configspeed <rpm>

Device chaining:
Devices can be chained by connecting another device to software serial pins (rx=8 and tx=9)
Connect the RX pin (8) to the TX pin of the next device, and the TX pin (9) to the RX of the next device (Typically pin 0/1).
Commands to the next device can be send by prefixing a line with a $ character. 
Data coming from the chained device is also prefixed and sent into the serial port.

*/

/* *******************************************************
/  Libraries
*/
#include <Wire.h> // Needed for I2C connection
#include "LiquidCrystal_I2C.h" // Needed for operating the LCD screen
#include <TimerOne.h>
#include <SoftwareSerial.h>

#define LED_PIN 13      // the number of Arduino's onboard LED pin

#define MOTOR_STEP_PIN    4
#define MOTOR_DIR_PIN     5
#define MOTOR_SLEEP_PIN   6

// These have to be interrupt pins (see http://arduino.cc/en/Reference/attachInterrupt). For the UNO, this means we have to use pin 2 and 3
#define ROTARY_ENCODER_PINA 2
#define ROTARY_ENCODER_PINB 3

#define BUTTON_PIN 7

#define MICROSTEPS 32

#define CHAIN_SERIAL_RX_PIN 8
#define CHAIN_SERIAL_TX_PIN 9
SoftwareSerial chainedDeviceSerial(CHAIN_SERIAL_RX_PIN, CHAIN_SERIAL_TX_PIN);
String chainedDeviceBuffer;

// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27,16,2);

byte ledstate = false; // Blinking indicator LED

String buffer;
uint32_t lastUpdate=0;


//Variable needed to keep track of time
uint32_t lastTick = 0; 

/* Useful Constants */
#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24L)
 
/* Useful Macros for getting elapsed time */
#define numberOfSeconds(_time_) (_time_ % SECS_PER_MIN)  
#define numberOfMinutes(_time_) ((_time_ / SECS_PER_MIN) % SECS_PER_MIN)
#define numberOfHours(_time_) (( _time_% SECS_PER_DAY) / SECS_PER_HOUR)
#define elapsedDays(_time_) ( _time_ / SECS_PER_DAY)  

float motorSpeed = 0.0f;


//Based on bildr article: http://bildr.org/2012/08/rotary-encoder-arduino/
volatile int lastEncoded = 0;
volatile long encoderValue = 0;
long lastencoderValue = 0;
int lastMSB = 0;
int lastLSB = 0;


/* *******************************************************
/  updateEncoder is the function that reacts to the rotary encoder interrupts
*/
void updateEncoder(){
  int MSB = digitalRead(ROTARY_ENCODER_PINA); //MSB = most significant bit
  int LSB = digitalRead(ROTARY_ENCODER_PINB); //LSB = least significant bit

  int encoded = (MSB << 1) |LSB; //converting the 2 pin value to single number
  int sum  = (lastEncoded << 2) | encoded; //adding it to the previous encoded value

  if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue ++;
  if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue --;

  lastEncoded = encoded; //store this value for next time
}


void setupRotaryEncoder() {
  // Set pinmodes
  pinMode(ROTARY_ENCODER_PINA, INPUT); 
  pinMode(ROTARY_ENCODER_PINA, INPUT);

  digitalWrite(ROTARY_ENCODER_PINA, HIGH); //turn pullup resistor on
  digitalWrite(ROTARY_ENCODER_PINA, HIGH); //turn pullup resistor on

  //call updateEncoder() when any high/low changed seen
  //on interrupt 0 (pin 2), or interrupt 1 (pin 3)  (see http://arduino.cc/en/Reference/attachInterrupt)
  attachInterrupt(0, updateEncoder, CHANGE); 
  attachInterrupt(1, updateEncoder, CHANGE);
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

void updateLCD()
{
//  lcd.clear();  
  lcd.setCursor(0,0);
  lcd.print(F("Speed: "));
  lcd.print( (int)(motorSpeed*60.0f) );
  lcd.print(F(" rpm   "));

/*  lcd.setCursor(0,1);
  lcd.print(F("Volume: "));//clear row
  
  const float pi =3.141593f;
  char strbuf[10];
  dtostrf(0.0f, 4, 2, strbuf);

  lcd.print(strbuf);
  lcd.print( " cm3/s");*/
  
  lcd.setCursor(0,1);
  lcd.print("rot: ");
  lcd.print(encoderValue);
  lcd.print(" btn: ");
  lcd.print(digitalRead(BUTTON_PIN));
  lcd.print("   ");
}


void setMotorSpeed(float rps)
{
  motorSpeed=rps;
  byte motorDir = rps > 0 ? 1 : 0;
  digitalWrite(MOTOR_DIR_PIN, motorDir);

  updateLCD();
    
  // Our motor has 1.8deg per step, so 360/1.8 is 200 steps per revolution
  float stepsPerSecond = 200*rps*MICROSTEPS;

  // set period in microseconds
  if (stepsPerSecond != 0) {
    Timer1.setPeriod(1e6/fabsf(stepsPerSecond));
  }
  
  bool sleepMode = motorSpeed == 0.0f;
  digitalWrite(MOTOR_SLEEP_PIN, sleepMode ? 0 : 1); // sleep pin is inverted

  Serial.print(F("Steps per sec"));
  Serial.println((int)stepsPerSecond);
}

void motorMove()
{
  digitalWrite(MOTOR_STEP_PIN,1);
  digitalWrite(MOTOR_STEP_PIN,0);
}


/* *******************************************************
/  Setup, this code is only executed once
*/
void setup() {
  // Update clock
  lastTick = millis();

  // Open serial connection and print a message
  Serial.begin(9600);
  Serial.println(F("peristaltic-pump"));

  // initialize the LED pin as an output:
  pinMode(LED_PIN, OUTPUT);
  
  setupLCD();
  
  pinMode(MOTOR_DIR_PIN,OUTPUT);
  pinMode(MOTOR_STEP_PIN,OUTPUT);
  pinMode(MOTOR_SLEEP_PIN,OUTPUT);
  digitalWrite(MOTOR_SLEEP_PIN,0); // sleep by default
  
  setupRotaryEncoder();
  
  pinMode(BUTTON_PIN,INPUT);
  digitalWrite(BUTTON_PIN,HIGH); // set internal pull-up. When the button shorts, we see a 0. Otherwise, the internal pull-up resistor will pull the pin high.
  
  delay(600);
  Timer1.initialize(15000);
  Timer1.attachInterrupt(motorMove); // blinkLED to run every 0.15 seconds
//  lcd.backlight();
  
  updateLCD();
  
  chainedDeviceSerial.begin(9600);
}



/* *******************************************************
/  Loop, this code is constantly repeated
*/
void loop() {
  // Update clock
  uint32_t time = millis(); // current time since start of sketch
  uint16_t dt = time-lastTick; // difference between current and previous time
  lastTick = time;
  
  ledstate = 1-ledstate;
  digitalWrite(LED_PIN, ledstate); // alternate between 0 and 1   
  
//  motorMove();
//  delay(100);

  if (time > lastUpdate + 100) {
  //  Serial.println("...");
    lastUpdate=time;
    updateLCD();
  }
  
  while (chainedDeviceSerial.available()>0) {
    char c = (char)chainedDeviceSerial.read();
    if (c == '\n') {
      Serial.print("$"); Serial.println(chainedDeviceBuffer);
      chainedDeviceBuffer = "";
    } else {
      if (chainedDeviceBuffer.length()<100)
        chainedDeviceBuffer += c;
    }
  }
    
  while (Serial.available()>0) {
    char c = (char)Serial.read();
    if (c == '\n') {
      if (buffer.startsWith("$")) {
        chainedDeviceSerial.println(buffer.substring(1));
      } else if(buffer.startsWith("id")) {
        Serial.println("peristaltic-pump"); // so the bioreactor can figure out what is connected 
      } else if (buffer.startsWith("sp")) {
        float sp = buffer.substring(2).toInt()/60.0f;
        Serial.print(F("Setting motor speed to "));
        Serial.print(sp, 2);
        Serial.println(F(" rpm"));
        setMotorSpeed(sp);
      }
      else {
        Serial.println("Unknown cmd.");
      }
      buffer="";
    } else buffer+=c;
  }  
}





