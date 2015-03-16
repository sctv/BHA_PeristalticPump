
#define TUBE_WIDTH_CM 0.75f
#define MICROSTEPS 32 // when M0/M1/M2 are all on 0V
#define PUMP_RADIUS_CM 3


/* *******************************************************
/  Libraries
*/
#include <Wire.h> // Needed for I2C connection
#include "LiquidCrystal_I2C.h" // Needed for operating the LCD screen
#include <TimerOne.h>


// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27,16,2);

byte ledstate = false; // Blinking indicator LED
#define LED_PIN 13      // the number of Arduino's onboard LED pin

#define MOTOR_STEP_PIN 2
#define MOTOR_DIR_PIN 3
#define MOTOR_SLEEP_PIN 4


String buffer;
uint32_t lastUpdate=0;




/* *******************************************************
/  Variables needed for keeping track of time
*/
uint32_t lastTick = 0; // Global Clock

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

void setMotorSpeed(float rps)
{
  motorSpeed=rps;
  byte motorDir = rps > 0 ? 1 : 0;
  digitalWrite(MOTOR_DIR_PIN, motorDir);

  lcd.clear();  
  lcd.setCursor(0,0);
  lcd.print(F("Speed: "));
  lcd.print( (int)(rps*60.0f) );
  lcd.print(F(" rpm"));

  lcd.setCursor(0,1);
  lcd.print(F("Volume: "));//clear row
  
  const float pi =3.141593f;
  const float tube_area = pi * (TUBE_WIDTH_CM/2)*(TUBE_WIDTH_CM/2);
   // linear velocity = pump radius * angular velocity in radians
  float linvel = PUMP_RADIUS_CM * 2*pi*rps;
  float vol = tube_area * linvel;
  char strbuf[10];
  dtostrf(vol, 4, 2, strbuf);

  lcd.print(strbuf);
  lcd.print( " cm3/s");
    
  // Our motor has 1.8deg per step, so 360/1.8 is 200 steps per revolution
  float stepsPerSecond = 200*rps*MICROSTEPS;

  // set period in microseconds
  if (stepsPerSecond != 0) {
    Timer1.setPeriod(1e6/stepsPerSecond);
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

  // Initialize I2C
  Wire.begin();

  // Open serial connection and print a message
  Serial.begin(9600);
  Serial.println(F("BioHack Academy PeristalticPump"));

  // initialize the LED pin as an output:
  pinMode(LED_PIN, OUTPUT);
  
  // Initialize the LCD and print a message
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(F("BioHack Academy"));
  lcd.setCursor(0,1);
  lcd.print(F("PeristalticPump"));
  
  pinMode(MOTOR_DIR_PIN,OUTPUT);
  pinMode(MOTOR_STEP_PIN,OUTPUT);
  pinMode(MOTOR_SLEEP_PIN,OUTPUT);
  digitalWrite(MOTOR_SLEEP_PIN,0); // sleep by default
  
  delay(600);
  Timer1.initialize(15000);
  Timer1.attachInterrupt(motorMove); // blinkLED to run every 0.15 seconds
  lcd.backlight();
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
  delay(100);
  
  if (time > lastUpdate + 1000) {
    Serial.println("...");
    lastUpdate=time;
  }
    
  while (Serial.available()>0) {
    char c = (char)Serial.read();
  
    if (c == '\n') {
      if (buffer.startsWith("sp")) {
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


