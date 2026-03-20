#include <Arduino.h>
#include <AccelStepper.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <Wire.h>

#define STEP_PIN 3
#define DIR_PIN 4
#define ENABLE_PIN 5
#define CLK 9
#define DT 8
#define SW 10
#define SERVO1_PIN 7
#define SERVO2_PIN 6

#define STEPS_PER_MM 400L  
#define MAX_TRAVEL_MM 250L
#define EXTEND_POSITION 250L
#define MIN_SPEED 200
#define MAX_SPEED 1500
#define DEFAULT_SPEED 500
#define HOLD_TIME 2000
#define SERVO_STOP 90
#define SERVO_FWD 108
#define SERVO_REV 72

#define MS_PER_DEGREE 40

AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo servo1, servo2;

int mode = 0;
int menuSelection = 1;
bool emergencyStop = false;
int lastCLK;
long manualTarget = 0;
int motorSpeed = DEFAULT_SPEED;
unsigned long encoderTimer = 0;
int targetCycles = 0;
int currentCycle = 0;

int lastMotorSpeed = -1;
long lastManualTarget = -1;
int lastAngle = -1;
int lastMode = -1;
int lastState = -1;
int lastTargetCycles = -1;
int lastCurrentCycle = -1;
bool forceLcdUpdate = true;
unsigned long lastLcdUpdateTimer = 0;

int currentServoAngle = 90;
int targetServoAngle = 90; 
unsigned long lastServoMove = 0;

enum RehabState
{
  MOVE_EXTEND,
  ANKLE_PUSH,
  HOLD_EXTEND,
  MOVE_HOME,
  ANKLE_RELAX,
  HOLD_HOME
};

RehabState state = MOVE_EXTEND;
unsigned long stateTimer = 0;

bool lastBtnState = HIGH;
unsigned long pressStart = 0;
bool longPressHandled = false;

void forceDisplayUpdate();
void updateLCD();

void startServo(int angle)
{
  targetServoAngle = constrain(angle, 0, 180); 
}

void updateServo()
{
  if (emergencyStop) {
    servo1.write(SERVO_STOP);
    servo2.write(SERVO_STOP);
    return;
  }

  if (mode == 2) {
    if (millis() - lastServoMove > 50) {
      servo1.write(SERVO_STOP);
      servo2.write(SERVO_STOP);
    }
    currentServoAngle = targetServoAngle;
  } else if (mode == 0 || mode == 1 || mode == 4) {
    servo1.write(SERVO_STOP);
    servo2.write(SERVO_STOP);
  }
}

void readEncoder()
{
  int clk = digitalRead(CLK);

  if (clk != lastCLK && millis() - encoderTimer > 1)
  {
    encoderTimer = millis();
    
    if (clk == LOW) {
        if (digitalRead(DT) != clk)
        {
          if (mode == 1) manualTarget += 1 * STEPS_PER_MM;
          else if (mode == 2) {
            targetServoAngle += 1;
            servo1.write(SERVO_FWD);
            servo2.write(SERVO_FWD);
            lastServoMove = millis();
          }
          else if (mode == 3) motorSpeed += 50;
          else if (mode == 4) targetCycles++;
          else if (mode == 0) {
            menuSelection++;
            if (menuSelection > 3) menuSelection = 3;
          }
        }
        else
        {
          if (mode == 1) manualTarget -= 5 * STEPS_PER_MM;
          else if (mode == 2) {
            targetServoAngle -= 1;
            servo1.write(SERVO_REV);
            servo2.write(SERVO_REV);
            lastServoMove = millis();
          }
          else if (mode == 3) motorSpeed -= 50;
          else if (mode == 4) {
            if (targetCycles > 0) targetCycles--;
          }
          else if (mode == 0) {
            menuSelection--;
            if (menuSelection < 1) menuSelection = 1;
          }
        }

        manualTarget = constrain(manualTarget, 0, MAX_TRAVEL_MM * STEPS_PER_MM);
        targetServoAngle = constrain(targetServoAngle, 0, 180);
        motorSpeed = constrain(motorSpeed, MIN_SPEED, MAX_SPEED);
        targetCycles = constrain(targetCycles, 0, 99);
        
        if (mode == 1) stepper.moveTo(manualTarget);
        if (mode == 2) startServo(targetServoAngle);
    }
  }
  lastCLK = clk;
}

void checkButton()
{
  bool btnState = digitalRead(SW);

  if (btnState == LOW) {
    if (lastBtnState == HIGH) {
      pressStart = millis();
      longPressHandled = false;
    } else {
      if (!longPressHandled && millis() - pressStart > 1000) {
        longPressHandled = true;
        
        emergencyStop = !emergencyStop;
        
        lcd.clear();
        lcd.setCursor(0, 0);

        if (emergencyStop) {
          stepper.moveTo(stepper.currentPosition()); 
          startServo(SERVO_STOP);
          
          lcd.print("EMERGENCY STOP! ");
          lcd.setCursor(0, 1);
          lcd.print("Hold to Reset   ");
        } else {
          lcd.print("System Reset    ");
          delay(1000);
          
          if (mode == 1) {
            manualTarget = stepper.currentPosition();
            stepper.moveTo(manualTarget);
          } else if (mode == 2) {
            state = MOVE_HOME;
          } else if (mode == 3) {
            state = MOVE_HOME;
          }
          forceDisplayUpdate();
        }
      }
    }
  } else {
    if (lastBtnState == LOW) {
      if (!longPressHandled) {
        if (!emergencyStop) {
          if (mode == 0) {
            if (menuSelection == 1) {
              mode = 1;
              manualTarget = stepper.currentPosition();
              stepper.moveTo(manualTarget);
            } else if (menuSelection == 2) {
              mode = 2;
            } else if (menuSelection == 3) {
              mode = 4;
            }
          } else if (mode == 4) {
            mode = 3;
            currentCycle = 0;
            state = MOVE_EXTEND; 
          } else {
            mode = 0;
            stepper.stop(); 
            startServo(SERVO_STOP);
          }
          forceDisplayUpdate();
        }
      }
    }
  }
  
  lastBtnState = btnState;
}

void forceDisplayUpdate() {
  forceLcdUpdate = true;
  lastMode = -1;
}

void updateLCD() {
  if (emergencyStop) return;

  if (!forceLcdUpdate && millis() - lastLcdUpdateTimer < 100) return;
  
  long currentPos = constrain(stepper.currentPosition(), 0, MAX_TRAVEL_MM * STEPS_PER_MM);
  int currentAngle = map(currentPos, 0, MAX_TRAVEL_MM * STEPS_PER_MM, 90, 180);
  bool didUpdate = false;
  
  if (mode == 0) {
    static int lastMenuSelect = -1;
    if (lastMode != mode || forceLcdUpdate || lastMenuSelect != menuSelection) {
      stepper.run(); lcd.setCursor(0, 0); stepper.run();
      lcd.print("Mode Select:    "); stepper.run();
      lcd.setCursor(0, 1); stepper.run();

      if (menuSelection == 1)      lcd.print(">Leg  Ank  Auto ");
      else if (menuSelection == 2) lcd.print(" Leg >Ank  Auto ");
      else                         lcd.print(" Leg  Ank >Auto ");

      lastMode = mode;
      forceLcdUpdate = false;
      lastMenuSelect = menuSelection;
      didUpdate = true;
    }
  }

  if (didUpdate) {
    lastLcdUpdateTimer = millis();
  }
}

void Manual()
{
  stepper.setMaxSpeed(motorSpeed);
}

void Automatic()
{
  stepper.setMaxSpeed(motorSpeed);
  
  switch (state)
  {
    case MOVE_EXTEND:
      stepper.moveTo(EXTEND_POSITION * STEPS_PER_MM);
      if (stepper.distanceToGo() == 0) {
        servo1.write(SERVO_FWD);
        servo2.write(SERVO_FWD);
        stateTimer = millis();
        state = ANKLE_PUSH;
      }
      break;

    case ANKLE_PUSH:
      if (millis() - stateTimer >= 500) {
        servo1.write(SERVO_STOP);
        servo2.write(SERVO_STOP);
        stateTimer = millis();
        state = HOLD_EXTEND;
      }
      break;

    case HOLD_EXTEND:
      if (millis() - stateTimer > HOLD_TIME)
        state = MOVE_HOME;
      break;

    case MOVE_HOME:
      stepper.moveTo(0);
      if (stepper.distanceToGo() == 0) {
        servo1.write(SERVO_REV);
        servo2.write(SERVO_REV);
        stateTimer = millis();
        state = ANKLE_RELAX;
      }
      break;

    case ANKLE_RELAX:
      if (millis() - stateTimer >= 500) {
        servo1.write(SERVO_STOP);
        servo2.write(SERVO_STOP);
        stateTimer = millis();
        state = HOLD_HOME;
      }
      break;

    case HOLD_HOME:
      if (millis() - stateTimer > HOLD_TIME) {
        currentCycle++;
        
        if (targetCycles > 0 && currentCycle >= targetCycles) {
          mode = 0;
          stepper.stop();
          startServo(SERVO_STOP);
          servo1.write(SERVO_STOP);
          servo2.write(SERVO_STOP);
          forceDisplayUpdate();
        } else {
          state = MOVE_EXTEND;
        }
      }
      break;
  }
}

void setup()
{
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW);

  pinMode(CLK, INPUT_PULLUP);
  pinMode(DT, INPUT_PULLUP);
  pinMode(SW, INPUT_PULLUP);

  lastCLK = digitalRead(CLK);

  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(600);

  lcd.init();
  lcd.backlight();

  Wire.setClock(400000);

  lcd.setCursor(0, 0);
  lcd.print("Rehab Pro System");
  lcd.setCursor(0, 1);
  lcd.print(" Initializing.. ");

  delay(1500);

  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);

  servo1.write(SERVO_STOP);
  servo2.write(SERVO_STOP);

  startServo(SERVO_STOP);

  forceDisplayUpdate();
}

void loop()
{
  readEncoder();
  checkButton();

  if (!emergencyStop)
  {
    if (mode == 1 || mode == 2)
      Manual();
    else if (mode == 3)
      Automatic();
      
    updateLCD();
  }

  updateServo();
  stepper.run();
}
