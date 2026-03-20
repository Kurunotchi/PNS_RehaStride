# Robotics_PNS Project Documentation

## 1. Project Overview

This is a robotics rehabilitation control project designed for an Arduino-based system. It integrates:
- stepper motor control (`AccelStepper`)
- LCD display (`LiquidCrystal_I2C`)
- rotary encoder input
- two servos for ankle push/relax movements

The system supports:
- Manual mode (encoder controls position)
- Automatic rehab mode (state machine cycles through extension, hold, push, return, relax, hold)
- Emergency stop toggle (button hold >3s)

## 2. Hardware Pinout

| Function | Pin | Notes |
|----------|-----|-------|
| Stepper STEP | `3` | AccelStepper DRIVER mode |
| Stepper DIR | `4` | |
| Stepper ENABLE | `5` | Active low |
| Rotary encoder CLK | `9` | `INPUT_PULLUP` |
| Rotary encoder DT | `8` | `INPUT_PULLUP` |
| Rotary encoder SW | `10` | pushbutton, `INPUT_PULLUP` |
| Servo 1 signal | `7` | |
| Servo 2 signal | `6` | |

## 3. Constants and Configuration

- `STEPS_PER_MM = 100` (microsteps per mm)
- `MAX_TRAVEL_MM = 250`
- `EXTEND_POSITION = 250`
- `MIN_SPEED = 200`, `MAX_SPEED = 1500`, `DEFAULT_SPEED = 500`
- `HOLD_TIME = 2000` ms
- `LCD_REFRESH = 500` ms
- `SERVO_STOP = 90`, `SERVO_PUSH = 110`, `SERVO_RELAX = 70`, `SERVO_TIME = 1000` ms

## 4. Modes

### Manual Mode
- Active when `mode == 0`
- Rotary encoder adjusts `manualTarget` in steps of `5 mm` (`5 * STEPS_PER_MM`)
- Stepper is moved to `manualTarget`
- LCD shows "Manual Mode" and "Move Knob"

### Automatic Rehab Mode
- Active when `mode == 1`
- State machine in `RehabState`:
  1. `MOVE_EXTEND` -> move to `EXTEND_POSITION`
  2. `ANKLE_PUSH` -> run servos to `SERVO_PUSH`
  3. `HOLD_EXTEND` -> wait `HOLD_TIME`
  4. `MOVE_HOME` -> move to `0`
  5. `ANKLE_RELAX` -> run servos to `SERVO_RELAX`
  6. `HOLD_HOME` -> wait `HOLD_TIME`
  7. loop back to `MOVE_EXTEND`
- LCD shows "Auto Rehab" and current speed

## 5. Controls

- Rotary encoder turned in manual mode adjusts target position.
- Rotary encoder turned in automatic mode adjusts `motorSpeed`.
- Encoder button press toggles between manual and automatic mode.
- Encoder button hold >3 seconds toggles `emergencyStop`.

## 6. Setup Sequence

1. Set `ENABLE_PIN` low (releases stepper driver).
2. Set encoder pins to `INPUT_PULLUP`.
3. Initialize stepper max speed and acceleration.
4. Initialize LCD and backlight.
5. Attach servos and set to stop position.
6. Display initial mode text.

## 7. Main Loop

Every cycle:
1. `readEncoder()` reads the rotary encoder and updates targets/speed.
2. `checkButton()` handles mode toggle and emergency stop.
3. If not emergency stopped:
   - Run `Manual()` or `Automatic()` depending on mode.
4. `updateServo()` returns servos to stop after `SERVO_TIME`.
5. `stepper.run()` executes stepper motion.

## 8. Notes / Extensions

- Adjust `STEPS_PER_MM` and `EXTEND_POSITION` to match mechanical travel.
- Add physical limit switches to prevent over-travel in manual mode.
- Add persistent settings in EEPROM for speed and position.
