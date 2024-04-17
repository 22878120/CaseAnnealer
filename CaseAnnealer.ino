#include <AFMotor.h>
#include <Servo.h>
#include <EEPROM.h>

// LCD I2C 1602 LCD Display Module 16X2
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// DC hobby servo
// Servo1 (signal) >> Pin10
// Servo2 (Signal) >> Pin9
// motor range 20-165
Servo servo1;


#include <ezButton.h>
ezButton btnRun(9);       // servo2 not used
ezButton btnInc(3);       // M2 not used
ezButton btnDec(11);      // M1 not used



//Global var for anneal duration
float annealing_duration = 3.00;  //time in sec, default 3 sec
const String sAnnealingReady = "Annealing ready";
const String sAnnealingRun = "Annealing run";
const String sChangeTiming = "Red btn to save";
bool isIdle = true;
bool isChangeTime = false;
bool isRequestCancel = false;



// Relay
int relayPin = 2;  //Pin 2

//Specifics for my servo
int servo_min = 25;
int servo_max = 160;
int servo_mid = 93; // the servoe start pos

// Stepper motor on M3+M4 200 steps per revolution
// M1 -> Pin 11
// M2 -> Pin 3
AF_Stepper stepper(200, 2);


// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2); // set the LCD address to 0x27, if new version please use 0x3F instead.


// --------------------------------------------------------------------------------------------------------------
void showLCD(String msgLn1="", String msgLn2="", int lcd_delay=500 ) { 
  // Fill text with space to clear screen
  for (int i = msgLn1.length(); i <=16; i++) {
    msgLn1 = msgLn1 + " ";
  }

  for (int i = msgLn2.length(); i <=16; i++) {
    msgLn2 = msgLn2 + " ";
  }

  lcd.setCursor(0, 0); // set the cursor to column 0, line 0
  lcd.print(msgLn1);  // Print a message to the LCD
  
  lcd.setCursor(0, 1); // set the cursor to column 0, line 1
  lcd.print(msgLn2);  // Print a message to the LCD.

  // Serial.println(msgLn1);
  // Serial.println(msgLn2);

  delay(lcd_delay);  
}

void DoAnnealing () {
  unsigned long StartTime;
  unsigned long EndTime;
  char str[5];  // Allocate a character array to store the resul
  const String sStartAnn = "Start annealing:";

  showLCD(sStartAnn);

  StartTime = millis();

  digitalWrite(relayPin,HIGH);  	// turn the relay ON
								// [NO] is connected to feed
								// [NC] is not connected to feed

  while ((millis()-StartTime) < (annealing_duration*1000)) {
    CheckRunPressDuringExecution();
    // showLCD("Start annealing:", String((millis()-StartTime)/1000)+" sec", 0);

    dtostrf((millis()-StartTime)/1000, 5, 2, str);
    showLCD(sStartAnn, String(str) + " sec", 0);
  }  				

  EndTime = millis();
	digitalWrite(relayPin,LOW);  	// turn the relay OFF
								// [NO] is not connected to feed
								// [NC] is connected to feed

  showLCD("Annealing finished", "Time: "+String((EndTime - StartTime)/1000)+" sec", 1000);
}

void CaseFeed() {
  showLCD("Feed case");

  stepper.setSpeed(50);

  // stepper.step(400, FORWARD, INTERLEAVE); //too fast
  stepper.step(200, FORWARD, MICROSTEP);
  // stepper.step(200, FORWARD, DOUBLE);

  stepper.release(); // stop rotation and turn off holding torque.
}

void CaseRelease() {
  showLCD("Release case");
  servo1.write(servo_min);
  delay(500);  //1000 ms = 1 sec
  servo1.write(servo_mid);
}

String GetAnnealTime() {
  return "Time: "+String(annealing_duration)+" sec";
}

void AnnealOneCycle() {
  CaseFeed();
  CheckRunPressDuringExecution();

  DoAnnealing();
  CheckRunPressDuringExecution();

  CaseRelease();

  unsigned long StartTime;
  unsigned long TotalWaitTime = 0;

  StartTime = millis();

  while (TotalWaitTime < 2000) {
    CheckRunPressDuringExecution();
    TotalWaitTime = millis() - StartTime;
  }
}

void CheckRunPressDuringExecution() {
  btnRun.loop();
  if (btnRun.isPressed()) {
    isRequestCancel = true;
    showLCD("Cancel requested", "");
    // delay(1000);
  }
}

void setup() {
  //--------------------------------------------------------------------------------------------------------------------
  // LCD
  lcd.init();  //initialize the lcd
  lcd.backlight();  //open the backlight
  showLCD("LCD ready");

  //--------------------------------------------------------------------------------------------------------------------
  // Serial.begin(9600);           // set up Serial library at 9600 bps

  //--------------------------------------------------------------------------------------------------------------------
  // Relay setup
  pinMode(relayPin, OUTPUT);  	// Define the port attribute as output   
  digitalWrite(relayPin, LOW);    // switch off relay:  
  showLCD("Relay ready");

  //--------------------------------------------------------------------------------------------------------------------
  // turn on servo
  servo1.attach(10);

  if (servo1.read() == servo_mid) {
    showLCD("Case door closed.", 
            "Pos: "+String(servo1.read())+", "+servo1.readMicroseconds()+" ms");
  }


  //--------------------------------------------------------------------------------------------------------------------
  // turn on stepper motor #2
  stepper.setSpeed(200);
  showLCD("Feeder motor", "ready...");


  // Get stored anneal time
  EEPROM.get(0, annealing_duration);
  showLCD(sAnnealingReady, GetAnnealTime());
}

void loop() {
  btnRun.loop();
  btnInc.loop();
  btnDec.loop();

  if (btnRun.isPressed() || isRequestCancel) {
    if (isIdle){  //Idle & change -> Save value
      if (isChangeTime) { //Save
        EEPROM.put(0, annealing_duration);
        isChangeTime = false;
        
        showLCD("Timing saved", GetAnnealTime());
        delay(1000);
        showLCD(sAnnealingReady, GetAnnealTime());
      } else {  //Idle & no change -> RUN
        isIdle = false;
        showLCD(sAnnealingRun, ""); //will run after exit this IF
      }
    } else {  // not idel (running) -> Cancel run
      isIdle = true;
      isRequestCancel = false;
      showLCD("Canceling...", GetAnnealTime());
      delay(1000);
      showLCD(sAnnealingReady, GetAnnealTime());
    }
  }

  //If running, keep running
  if (!isIdle) {
    AnnealOneCycle();
  }

  // Inc/Dec buttons can be used only in Idel
  if (isIdle) {
    if (btnInc.isPressed()){
      annealing_duration = annealing_duration + 0.1;
      isChangeTime = true;
      showLCD(sChangeTiming, GetAnnealTime(), 0);
    }

    if (btnDec.isPressed()){
      // Serial.println("btnDec" + String(annealing_duration));
      annealing_duration = annealing_duration -0.1;
      if (annealing_duration < 0) {
        annealing_duration = 0;
      }
      isChangeTime = true;
      showLCD(sChangeTiming, GetAnnealTime(), 0);
    }
  }

  delay(100);
}
