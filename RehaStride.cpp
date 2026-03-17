#include <Arduino.h>
#include <AccelStepper.h>
#include <LiquidCrystal_I2C.h>

#define STEP_PIN 3
#define DIR_PIN 2
#define ENABLE_PIN 4

#define CLK 9
#define DT 8
#define SW 10

#define STEPS_PER_MM 100
#define MAX_TRAVEL_MM 250

#define HOME_POSITION 0
#define EXTEND_POSITION 250

#define MIN_SPEED 200
#define MAX_SPEED 1500
#define DEFAULT_SPEED 500

#define HOLD_TIME 2000
#define PAUSE_TIME 1000

#define LCD_REFRESH 1000

AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

int lastCLK;
bool mode = 0;
bool emergencyStop = false;
long manualTarget = 0;
int motorSpeed = DEFAULT_SPEED;
unsigned long lcdTimer = 0;
unsigned long encoderTimer = 0;

enum RehabState
{
  MOVE_EXTEND,
  HOLD_EXTEND,
  MOVE_HOME,
  HOLD_HOME,
  PAUSE
};

RehabState rehabState = MOVE_EXTEND;

long targetPos = 0;
unsigned long stateTimer = 0;
int reps = 0;
int maxReps = 10;

void readEncoder();
void checkButton();
void Manual();
void Automatic();

void setup()
{
  Serial.begin(9600);
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW);
  pinMode(CLK, INPUT_PULLUP);
  pinMode(DT, INPUT_PULLUP);
  pinMode(SW, INPUT_PULLUP);
  lastCLK = digitalRead(CLK);
  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(600);
  stepper.setCurrentPosition(0);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("PNS Robotics");
  lcd.setCursor(0,1);
  lcd.print("Rehab System");
  delay(2000);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Manual Mode");
  lcd.setCursor(0,1);
  lcd.print("Rotate = Move");
}

void loop()
{
  readEncoder();
  checkButton();
  if (!emergencyStop)
  {
    if (mode == 0)
      Manual();
    else
      Automatic();
  }
  stepper.run();
}

void readEncoder()
{
  int clk = digitalRead(CLK);
  if (clk != lastCLK && millis() - encoderTimer > 3)
  {
    encoderTimer = millis();
    if (digitalRead(DT) != clk)
    {
      if (mode == 0)
        manualTarget += 5 * STEPS_PER_MM;
      else
        motorSpeed += 50;
    }
    else
    {
      if (mode == 0)
        manualTarget -= 5 * STEPS_PER_MM;
      else
        motorSpeed -= 50;
    }
    motorSpeed = constrain(motorSpeed, MIN_SPEED, MAX_SPEED);
    manualTarget = constrain(manualTarget, 0, MAX_TRAVEL_MM * STEPS_PER_MM);
    if (mode == 0)
      stepper.moveTo(manualTarget);
  }
  lastCLK = clk;
}

void checkButton()
{
  static bool lastState = HIGH;
  static unsigned long pressStart = 0;
  bool state = digitalRead(SW);
  if (state == LOW && lastState == HIGH)
    pressStart = millis();
  if (state == HIGH && lastState == LOW)
  {
    unsigned long pressTime = millis() - pressStart;
    if (pressTime > 3000)
    {
      emergencyStop = !emergencyStop;
      lcd.clear();
      lcd.setCursor(0,0);
      if (emergencyStop)
      {
        lcd.print("EMERGENCY STOP");
        lcd.setCursor(0,1);
        lcd.print("Hold to Reset");
      }
      else
      {
        lcd.print("System Reset");
        delay(1000);
        lcd.clear();
      }
    }
    else
    {
      mode = !mode;
      lcd.clear();
      if (mode == 0)
      {
        lcd.setCursor(0,0);
        lcd.print("Manual Mode");
        lcd.setCursor(0,1);
        lcd.print("Rotate = Move");
        manualTarget = stepper.currentPosition();
      }
      else
      {
        lcd.setCursor(0,0);
        lcd.print("Auto Rehab");
        lcd.setCursor(0,1);
        lcd.print("Adjust Speed");
        rehabState = MOVE_HOME;
        reps = 0;
      }
    }
  }
  lastState = state;
}

void Manual()
{
  stepper.moveTo(manualTarget);
  if (millis() - lcdTimer > LCD_REFRESH)
  {
    lcdTimer = millis();
    lcd.setCursor(0,0);
    lcd.print("Manual Mode   ");
    lcd.setCursor(0,1);
    lcd.print("Rotate = Move ");
  }
}

void Automatic()
{
  switch (rehabState)
  {
    case MOVE_EXTEND:
      targetPos = EXTEND_POSITION * STEPS_PER_MM;
      stepper.setMaxSpeed(motorSpeed);
      stepper.moveTo(targetPos);
      if (stepper.distanceToGo() == 0)
      {
        rehabState = HOLD_EXTEND;
        stateTimer = millis();
      }
      break;
    case HOLD_EXTEND:
      if (millis() - stateTimer > HOLD_TIME)
        rehabState = MOVE_HOME;
      break;
    case MOVE_HOME:
      targetPos = HOME_POSITION;
      stepper.moveTo(targetPos);
      if (stepper.distanceToGo() == 0)
      {
        rehabState = HOLD_HOME;
        stateTimer = millis();
        reps++;
      }
      break;
    case HOLD_HOME:
      if (millis() - stateTimer > HOLD_TIME)
      {
        if (reps >= maxReps)
        {
          rehabState = PAUSE;
          stateTimer = millis();
        }
        else
          rehabState = MOVE_EXTEND;
      }
      break;
    case PAUSE:
      if (millis() - stateTimer > PAUSE_TIME)
      {
        reps = 0;
        rehabState = MOVE_EXTEND;
      }
      break;
  }
  if (millis() - lcdTimer > LCD_REFRESH)
  {
    lcdTimer = millis();
    lcd.setCursor(0,0);
    lcd.print("Auto Rehab    ");
    lcd.setCursor(0,1);
    lcd.print("Spd:");
    lcd.print(motorSpeed);
    lcd.print(" R:");
    lcd.print(reps);
    lcd.print("/");
    lcd.print(maxReps);
    lcd.print("   ");
  }
}
