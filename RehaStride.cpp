#include <AccelStepper.h>
#include <Arduino.h>
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
#define MAX_TRAVEL_MM 105L
#define EXTEND_POSITION 105L
#define MIN_SPEED 200
#define MAX_SPEED 1500
#define DEFAULT_SPEED 500
#define HOLD_TIME 5000
#define SERVO_STOP 90
#define SERVO_FWD 108
#define SERVO_REV 72
#define SERVO_MOVE_TIME 2000
#define TARGET_ANGLE 100

#define CALIB_STEPPER 1
#define CALIB_SERVO1 2
#define CALIB_SERVO2 3

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

long stepperCalibrationOffset = 0;
int servo1CalibrationOffset = 0;
int servo2CalibrationOffset = 0;
long calibrationTempTarget = 0;
int calibrationServoAngle = 90;
int calibrationSubMode = 0;

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

enum RehabState {
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

#include <EEPROM.h>
#define EEPROM_STEPPER_OFFSET 0
#define EEPROM_SERVO1_OFFSET 4
#define EEPROM_SERVO2_OFFSET 8
#define EEPROM_MAGIC_NUMBER 12
#define MAGIC_NUM 0xABCD

long angleToPositionMM(int angle) {
  angle = constrain(angle, 90, 105);
  return ((long)(angle - 90) * MAX_TRAVEL_MM) / 15;
}

void saveCalibration() {
  EEPROM.put(EEPROM_STEPPER_OFFSET, stepperCalibrationOffset);
  EEPROM.put(EEPROM_SERVO1_OFFSET, servo1CalibrationOffset);
  EEPROM.put(EEPROM_SERVO2_OFFSET, servo2CalibrationOffset);
  EEPROM.put(EEPROM_MAGIC_NUMBER, (uint16_t)MAGIC_NUM);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calibration Saved");
  lcd.setCursor(0, 1);
  lcd.print("Press any key...");
  delay(1500);
}

void loadCalibration() {
  uint16_t magic;
  EEPROM.get(EEPROM_MAGIC_NUMBER, magic);

  if (magic == MAGIC_NUM) {
    EEPROM.get(EEPROM_STEPPER_OFFSET, stepperCalibrationOffset);
    EEPROM.get(EEPROM_SERVO1_OFFSET, servo1CalibrationOffset);
    EEPROM.get(EEPROM_SERVO2_OFFSET, servo2CalibrationOffset);
  }
}

long getCalibratedPosition() {
  return stepper.currentPosition() - stepperCalibrationOffset;
}
void setCalibratedTarget(long calibratedTarget) {
  stepper.moveTo(calibratedTarget + stepperCalibrationOffset);
}

void forceDisplayUpdate() {
  forceLcdUpdate = true;
  lastMode = -1;
}

void startServo(int angle) { targetServoAngle = constrain(angle, 0, 180); }

void updateServo() {
  if (emergencyStop) {
    servo1.write(SERVO_STOP);
    servo2.write(SERVO_STOP);
    return;
  }

  if (calibrationSubMode == CALIB_SERVO1 && mode == 5) {
    servo1.write(calibrationServoAngle);
    servo2.write(SERVO_STOP);
  } else if (calibrationSubMode == CALIB_SERVO2 && mode == 5) {
    servo1.write(SERVO_STOP);
    servo2.write(calibrationServoAngle);
  } else if (mode == 2) {
    if (millis() - lastServoMove > 50) {
      servo1.write(SERVO_STOP);
      servo2.write(SERVO_STOP);
    }
    currentServoAngle = targetServoAngle;
  } else {
    if (mode != 3) {
      servo1.write(SERVO_STOP);
      servo2.write(SERVO_STOP);
    }
  }
}

void readEncoder() {
  int clk = digitalRead(CLK);
  if (clk != lastCLK && millis() - encoderTimer > 5) {
    encoderTimer = millis();

    if (clk == LOW) {
      if (digitalRead(DT) != clk) {
        if (mode == 5) {
          if (calibrationSubMode == CALIB_STEPPER) {
            calibrationTempTarget += STEPS_PER_MM;
            stepper.moveTo(calibrationTempTarget);
          } else if (calibrationSubMode == CALIB_SERVO1) {
            calibrationServoAngle += 1;
            if (calibrationServoAngle > 180)
              calibrationServoAngle = 180;
          } else if (calibrationSubMode == CALIB_SERVO2) {
            calibrationServoAngle += 1;
            if (calibrationServoAngle > 180)
              calibrationServoAngle = 180;
          }
        } else if (mode == 1) {
          manualTarget += STEPS_PER_MM;
          manualTarget =
              constrain(manualTarget, 0, MAX_TRAVEL_MM * STEPS_PER_MM);
          setCalibratedTarget(manualTarget);
        } else if (mode == 2) {
          targetServoAngle += 1;
          servo1.write(SERVO_FWD);
          servo2.write(SERVO_FWD);
          lastServoMove = millis();
        } else if (mode == 3)
          motorSpeed += 50;
        else if (mode == 4)
          targetCycles++;
        else if (mode == 0) {
          menuSelection++;
          if (menuSelection > 4)
            menuSelection = 4;
        }
      } else {
        if (mode == 5) {
          if (calibrationSubMode == CALIB_STEPPER) {
            calibrationTempTarget -= STEPS_PER_MM;
            stepper.moveTo(calibrationTempTarget);
          } else if (calibrationSubMode == CALIB_SERVO1) {
            calibrationServoAngle -= 1;
            if (calibrationServoAngle < 0)
              calibrationServoAngle = 0;
          } else if (calibrationSubMode == CALIB_SERVO2) {
            calibrationServoAngle -= 1;
            if (calibrationServoAngle < 0)
              calibrationServoAngle = 0;
          }
        } else if (mode == 1) {
          manualTarget -= STEPS_PER_MM;
          manualTarget =
              constrain(manualTarget, 0, MAX_TRAVEL_MM * STEPS_PER_MM);
          setCalibratedTarget(manualTarget);
        } else if (mode == 2) {
          targetServoAngle -= 1;
          servo1.write(SERVO_REV);
          servo2.write(SERVO_REV);
          lastServoMove = millis();
        } else if (mode == 3)
          motorSpeed -= 50;
        else if (mode == 4) {
          if (targetCycles > 0)
            targetCycles--;
        } else if (mode == 0) {
          menuSelection--;
          if (menuSelection < 1)
            menuSelection = 1;
        }
      }
      if (mode != 5) {
        targetServoAngle = constrain(targetServoAngle, 0, 180);
        motorSpeed = constrain(motorSpeed, MIN_SPEED, MAX_SPEED);
        targetCycles = constrain(targetCycles, 0, 99);
      }

      if (mode == 2)
        startServo(targetServoAngle);
      // LCD updates are handled by updateLCD() rate limit
    }
  }
  lastCLK = clk;
}

void checkButton() {
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
            manualTarget = getCalibratedPosition();
            manualTarget =
                constrain(manualTarget, 0, MAX_TRAVEL_MM * STEPS_PER_MM);
            setCalibratedTarget(manualTarget);
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
              manualTarget = getCalibratedPosition();
              manualTarget =
                  constrain(manualTarget, 0, MAX_TRAVEL_MM * STEPS_PER_MM);
              setCalibratedTarget(manualTarget);
              stepper.setMaxSpeed(motorSpeed);
            } else if (menuSelection == 2) {
              mode = 2;
            } else if (menuSelection == 3) {
              mode = 4;
            } else if (menuSelection == 4) {
              mode = 5;
              calibrationSubMode = CALIB_STEPPER;
              calibrationTempTarget = stepper.currentPosition();
              calibrationServoAngle = SERVO_STOP;
              stepper.moveTo(calibrationTempTarget);
              stepper.setMaxSpeed(DEFAULT_SPEED);
              stepper.setAcceleration(600);
            }
          } else if (mode == 4) {
            mode = 3;
            currentCycle = 0;
            state = MOVE_EXTEND;
          } else if (mode == 5) {
            if (calibrationSubMode == CALIB_STEPPER) {
              stepperCalibrationOffset = calibrationTempTarget;
              calibrationSubMode = CALIB_SERVO1;
              calibrationServoAngle = SERVO_STOP;
              manualTarget = 0;
              setCalibratedTarget(manualTarget);
              forceDisplayUpdate();
            } else if (calibrationSubMode == CALIB_SERVO1) {
              servo1CalibrationOffset = calibrationServoAngle - SERVO_STOP;
              calibrationSubMode = CALIB_SERVO2;
              calibrationServoAngle = SERVO_STOP;
              forceDisplayUpdate();
            } else if (calibrationSubMode == CALIB_SERVO2) {
              servo2CalibrationOffset = calibrationServoAngle - SERVO_STOP;
              saveCalibration();
              mode = 0;
              calibrationSubMode = 0;
              int calibratedServo1 =
                  constrain(SERVO_STOP + servo1CalibrationOffset, 0, 180);
              int calibratedServo2 =
                  constrain(SERVO_STOP + servo2CalibrationOffset, 0, 180);
              servo1.write(calibratedServo1);
              servo2.write(calibratedServo2);
              forceDisplayUpdate();
            }
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
void safePrint(const char* text) {
  while (*text) {
    lcd.print(*text++);
    stepper.run();
  }
}

void updateLCD() {
  if (emergencyStop)
    return;
  if (!forceLcdUpdate && millis() - lastLcdUpdateTimer < 100)
    return;

  long calibratedPos;
  if (mode == 5 && calibrationSubMode == CALIB_STEPPER) {
    calibratedPos = calibrationTempTarget / STEPS_PER_MM;
  } else {
    calibratedPos = getCalibratedPosition() / STEPS_PER_MM;
    calibratedPos = constrain(calibratedPos, 0, MAX_TRAVEL_MM);
  }

  int currentAngle = map(calibratedPos, 0, MAX_TRAVEL_MM, 90, 105);
  bool didUpdate = false;

  if (mode == 0) { // Home Page
    static int lastMenuSelect = -1;
    if (lastMode != mode || forceLcdUpdate || lastMenuSelect != menuSelection) {
      stepper.run();
      lcd.setCursor(0, 0);
      stepper.run();
      lcd.print("Mode Select:    ");
      stepper.run();
      lcd.setCursor(0, 1);
      stepper.run();
      if (menuSelection == 1)
        lcd.print(">Leg  Ank  Auto ");
      else if (menuSelection == 2)
        lcd.print(" Leg >Ank  Auto ");
      else if (menuSelection == 3)
        lcd.print(" Leg  Ank >Auto ");
      else
        lcd.print(" Leg  Ank  >Cal");
      stepper.run();

      lastMode = mode;
      forceLcdUpdate = false;
      lastMenuSelect = menuSelection;
      didUpdate = true;
    }
  } else if (mode == 1) { // Manual Leg
    if (lastMode != mode || forceLcdUpdate ||
        lastManualTarget != manualTarget) {
      stepper.run();
      lcd.setCursor(0, 0);
      stepper.run();
      lcd.print("Ctrl: Leg (Knee)");
      stepper.run();
      char lineBuffer[17];
      int targetMM = manualTarget / STEPS_PER_MM;
      sprintf(lineBuffer, "Set: %-3dmm      ", targetMM);
      lcd.setCursor(0, 1);
      stepper.run();
      lcd.print(lineBuffer);
      stepper.run();

      lastMode = mode;
      forceLcdUpdate = false;
      lastManualTarget = manualTarget;
      didUpdate = true;
    }
  } else if (mode == 2) { // Manual Ankle
    static int lastManualServo = -1;
    if (lastMode != mode || forceLcdUpdate ||
        lastManualServo != targetServoAngle) {
      stepper.run();
      lcd.setCursor(0, 0);
      stepper.run();
      lcd.print("Ctrl: Ankle     ");
      stepper.run();
      char lineBuffer[17];
      sprintf(lineBuffer, "Set: %-3d\xDF       ", targetServoAngle);
      lcd.setCursor(0, 1);
      stepper.run();
      lcd.print(lineBuffer);
      stepper.run();

      lastMode = mode;
      forceLcdUpdate = false;
      lastManualServo = targetServoAngle;
      didUpdate = true;
    }
  } else if (mode == 4) { // Set Cycles Menu
    if (lastMode != mode || forceLcdUpdate ||
        lastTargetCycles != targetCycles) {
      stepper.run();
      lcd.setCursor(0, 0);
      stepper.run();
      lcd.print("Set Auto Cycles:");
      stepper.run();
      char lineBuffer[17];
      if (targetCycles == 0) {
        sprintf(lineBuffer, "> Infinite      ");
      } else {
        sprintf(lineBuffer, "> %-2d Cycles     ", targetCycles);
      }
      lcd.setCursor(0, 1);
      stepper.run();
      lcd.print(lineBuffer);
      stepper.run();

      lastMode = mode;
      forceLcdUpdate = false;
      lastTargetCycles = targetCycles;
      didUpdate = true;
    }
  } else if (mode == 5) { // Calibration Mode
    static long lastCalibTempTarget = -999999;
    static int lastCalibServoAngle = -1;
    static int lastCalibSubMode = -1;
    if (lastMode != mode || forceLcdUpdate || 
        lastCalibTempTarget != calibrationTempTarget || 
        lastCalibServoAngle != calibrationServoAngle || 
        lastCalibSubMode != calibrationSubMode) {
      stepper.run();
      lcd.setCursor(0, 0);
      stepper.run();

      if (calibrationSubMode == CALIB_STEPPER) {
        safePrint("Calib: Stepper   ");
        char lineBuffer[17];
        long currentMM = calibrationTempTarget / STEPS_PER_MM;
        sprintf(lineBuffer, "Raw: %+4ldmm     ", currentMM);
        lcd.setCursor(0, 1);
        stepper.run();
        safePrint(lineBuffer);
      } else if (calibrationSubMode == CALIB_SERVO1) {
        safePrint("Calib: Servo 1   ");
        char lineBuffer[17];
        sprintf(lineBuffer, "Ang: %-3d\xDF       ", calibrationServoAngle);
        lcd.setCursor(0, 1);
        stepper.run();
        safePrint(lineBuffer);
      } else if (calibrationSubMode == CALIB_SERVO2) {
        safePrint("Calib: Servo 2   ");
        char lineBuffer[17];
        sprintf(lineBuffer, "Ang: %-3d\xDF       ", calibrationServoAngle);
        lcd.setCursor(0, 1);
        stepper.run();
        safePrint(lineBuffer);
      }

      lastCalibTempTarget = calibrationTempTarget;
      lastCalibServoAngle = calibrationServoAngle;
      lastCalibSubMode = calibrationSubMode;
      lastMode = mode;
      forceLcdUpdate = false;
      didUpdate = true;
    }
  } else if (mode == 3) { // Auto Mode
    bool drawDynamic = forceLcdUpdate;
    if (lastMode != mode || lastState != state || forceLcdUpdate ||
        lastCurrentCycle != currentCycle) {
      stepper.run();
      lcd.setCursor(0, 0);
      stepper.run();

      char topBuffer[17];
      char stateName[9];
      switch (state) {
      case MOVE_EXTEND:
        strcpy(stateName, "Extend  ");
        break;
      case ANKLE_PUSH:
        strcpy(stateName, "Ank Push");
        break;
      case HOLD_EXTEND:
        strcpy(stateName, "Hold Ext");
        break;
      case MOVE_HOME:
        strcpy(stateName, "Retract ");
        break;
      case ANKLE_RELAX:
        strcpy(stateName, "Ank Rlx ");
        break;
      case HOLD_HOME:
        strcpy(stateName, "Hold Rlx");
        break;
      }

      if (targetCycles == 0) {
        sprintf(topBuffer, "%s [INF]  ", stateName);
      } else {
        sprintf(topBuffer, "%s [%d/%d]", stateName, currentCycle + 1,
                targetCycles);
      }

      for (int i = strlen(topBuffer); i < 16; i++) {
        topBuffer[i] = ' ';
      }
      topBuffer[16] = '\0';

      lcd.print(topBuffer);
      stepper.run();
      drawDynamic = true;
      lastMode = mode;
      lastState = state;
      lastCurrentCycle = currentCycle;
      forceLcdUpdate = false;
      didUpdate = true;
    }

    if (drawDynamic || lastMotorSpeed != motorSpeed ||
        lastAngle != currentAngle) {
      char lineBuffer[17];
      sprintf(lineBuffer, "Spd:%-4d A:%-3d\xDF ", motorSpeed, currentAngle);
      stepper.run();
      lcd.setCursor(0, 1);
      stepper.run();
      lcd.print(lineBuffer);
      stepper.run();

      lastMotorSpeed = motorSpeed;
      lastAngle = currentAngle;
      didUpdate = true;
    }
  }
  if (didUpdate) {
    lastLcdUpdateTimer = millis();
  }
}

void Manual() { stepper.setMaxSpeed(motorSpeed); }

void Automatic() {
  stepper.setMaxSpeed(motorSpeed);

  // Calculate target position based on TARGET_ANGLE
  long targetPositionMM = angleToPositionMM(TARGET_ANGLE);

  switch (state) {
  case MOVE_EXTEND:
    // Move stepper to target position first
    setCalibratedTarget(targetPositionMM * STEPS_PER_MM);
    if (stepper.distanceToGo() == 0) {
      // Stepper finished moving, now move servo
      servo1.write(SERVO_FWD);
      servo2.write(SERVO_FWD);
      stateTimer = millis();
      state = ANKLE_PUSH;
    }
    break;

  case ANKLE_PUSH:
    // Servo movement phase
    if (millis() - stateTimer >= SERVO_MOVE_TIME) {
      servo1.write(SERVO_STOP);
      servo2.write(SERVO_STOP);
      stateTimer = millis();
      state = HOLD_EXTEND;
    }
    break;

  case HOLD_EXTEND:
    // Hold position
    if (millis() - stateTimer > HOLD_TIME)
      state = MOVE_HOME;
    break;

  case MOVE_HOME:
    // Move stepper back to home first
    setCalibratedTarget(0);
    if (stepper.distanceToGo() == 0) {
      // Stepper finished returning, now move servo opposite direction
      servo1.write(SERVO_REV);
      servo2.write(SERVO_REV);
      stateTimer = millis();
      state = ANKLE_RELAX;
    }
    break;

  case ANKLE_RELAX:
    // Servo movement phase
    if (millis() - stateTimer >= SERVO_MOVE_TIME) {
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

void setup() {
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

  loadCalibration();

  servo1.write(SERVO_STOP);
  servo2.write(SERVO_STOP);
  startServo(SERVO_STOP);

  manualTarget = getCalibratedPosition();
  manualTarget = constrain(manualTarget, 0, MAX_TRAVEL_MM * STEPS_PER_MM);

  forceDisplayUpdate();
}

void loop() {
  readEncoder();
  checkButton();

  if (!emergencyStop) {
    if (mode == 1 || mode == 2)
      Manual();
    else if (mode == 3)
      Automatic();

    updateLCD();
  }

  updateServo();
  stepper.run();
}
