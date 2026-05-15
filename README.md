# Robotics_PNS Project Documentation

Advanced Arduino Nano-based Physical Rehabilitation System for knee/ankle therapy.

## Key Features (from current `src/main.cpp`)
- Menu-driven UI with internal modes: 0..5 (see **Modes** below)
- Stepper-driven knee extension via AccelStepper
- Continuous-rotation servos for ankle push/relax
- Automatic rehab implemented as a finite state machine
- 1-button UI: short-press enters/starts, long-press (1s) toggles emergency stop & reset
- LCD updates are rate-limited and only redraw when values change
- EEPROM calibration offsets for stepper + both servos

## Libraries / Board
- Libraries: `AccelStepper`, `LiquidCrystal_I2C`, `Servo`, `Wire`, `EEPROM`
- Board/MCU: Arduino Nano ATmega328P (`platformio.ini`: `nanoatmega328`)

## Hardware Pinout (from `src/main.cpp`)
| Function | Pin | Notes |
|---|---:|---|
| Stepper STEP | 3 | AccelStepper::DRIVER |
| Stepper DIR | 4 | |
| Stepper ENABLE | 5 | LOW = enabled (active low) |
| Encoder CLK | 9 | INPUT_PULLUP |
| Encoder DT | 8 | INPUT_PULLUP |
| Encoder Button SW | 10 | INPUT_PULLUP; long-press (~1s) toggles emergency stop |
| Servo1 (ankle) | 7 | continuous rotation; uses FWD/STOP/REV PWM values |
| Servo2 (ankle) | 6 | continuous rotation; uses FWD/STOP/REV PWM values |

## Constants & Configuration (from `src/main.cpp`)
| Constant | Value | Description |
|---|---:|---|
| `STEPS_PER_MM` | 400 | steps per mm (knee travel scaling) |
| `MAX_TRAVEL_MM` | 105 | maximum knee extension mm (coded limits) |
| `EXTEND_POSITION` | 105 | “full extension mm” (present constant; auto uses `TARGET_ANGLE` mapping) |
| `MIN_SPEED` | 200 | minimum stepper max speed |
| `MAX_SPEED` | 1500 | maximum stepper max speed |
| `DEFAULT_SPEED` | 500 | initial stepper speed |
| `HOLD_TIME` | 5000 | hold duration in auto states (ms) |
| `SERVO_STOP` | 90 | neutral/stop PWM |
| `SERVO_FWD` | 108 | forward rotation PWM |
| `SERVO_REV` | 72 | reverse rotation PWM |
| `SERVO_MOVE_TIME` | 2000 | servo actuation time per auto servo state (ms) |
| `TARGET_ANGLE` | 100 | target knee angle used by auto mode (degrees) |

## Knee Angle Computation / Display
- Auto mode converts `TARGET_ANGLE` to stepper position via:
  - `angleToPositionMM(angle)`:
    - constrains angle to `[90, 105]`
    - linearly maps into `[0 .. MAX_TRAVEL_MM]`
- LCD angle is displayed as:
  - `currentAngle = map(calibratedPos, 0, MAX_TRAVEL_MM, 90, 105)`

## Modes (as implemented)
> Note: Code uses internal `mode` values: 0=Home, 1=Manual Leg, 2=Manual Ankle, 3=Auto Rehab, 4=Set Cycles, 5=Calibration.

### Mode 0: Home Menu (mode==0)
- LCD shows menu selection (Leg / Ank / Auto / Cal).
- Encoder rotates through menu options `menuSelection` in range 1..4.
- Short press enters:
  - selection 1 → Mode 1 (Manual Leg)
  - selection 2 → Mode 2 (Manual Ankle)
  - selection 3 → Mode 4 (Set Cycles)
  - selection 4 → Mode 5 (Calibration)

### Mode 1: Manual Leg (mode==1)
- LCD:
  - Top: `Ctrl: Leg (Knee)`
  - Bottom: `Set: XXXmm`
- Encoder:
  - CW: `manualTarget += STEPS_PER_MM`
  - CCW: `manualTarget -= STEPS_PER_MM`
- Stepper continuously moves toward target (non-blocking) using `stepper.run()`.

### Mode 2: Manual Ankle (mode==2)
- LCD:
  - Top: `Ctrl: Ankle`
  - Bottom: `Set: XXX°` (prints `targetServoAngle`)
- Encoder:
  - CW: `targetServoAngle += 1` and commands both servos to `SERVO_FWD`
  - CCW: `targetServoAngle -= 1` and commands both servos to `SERVO_REV`
- `updateServo()` applies time-limited motion; after ~50ms idle it returns servos to `SERVO_STOP`.

### Mode 4: Set Cycles (mode==4)
- LCD:
  - Top: `Set Auto Cycles:`
  - Bottom:
    - `> Infinite` if `targetCycles == 0`
    - otherwise `> NN Cycles`
- Encoder:
  - CW increments `targetCycles` (0..99)
  - CCW decrements `targetCycles` (does not go below 0)
- Short press starts Auto (switches to `mode=3`):
  - sets `currentCycle = 0`
  - sets `state = MOVE_EXTEND`

### Mode 3: Automatic Rehab (mode==3)
- LCD top line: state name + cycle progress
  - if infinite (`targetCycles==0`): shows `[INF]`
  - else shows `[current/target]`
- LCD bottom line (dynamic): `Spd:<motorSpeed> A:<currentAngle>°`
- Encoder during auto:
  - CW: `motorSpeed += 50`
  - CCW: `motorSpeed -= 50`

#### Auto State Machine (`RehabState`)
- `MOVE_EXTEND`
  - stepper moves to position derived from `TARGET_ANGLE`
  - once `distanceToGo()==0` ⇒ move to `ANKLE_PUSH`
- `ANKLE_PUSH`
  - both servos: `SERVO_FWD` for `SERVO_MOVE_TIME`
  - then ⇒ `HOLD_EXTEND`
- `HOLD_EXTEND`
  - waits `HOLD_TIME`
  - then ⇒ `MOVE_HOME`
- `MOVE_HOME`
  - stepper moves to `0`
  - once complete ⇒ set servos to `SERVO_REV` and go to `ANKLE_RELAX`
- `ANKLE_RELAX`
  - both servos: `SERVO_REV` for `SERVO_MOVE_TIME`
  - then ⇒ `HOLD_HOME`
- `HOLD_HOME`
  - waits `HOLD_TIME`
  - then increments `currentCycle`
  - if finite and completed ⇒ returns to `mode=0` and stops
  - else ⇒ loops back to `MOVE_EXTEND`

### Mode 5: Calibration (mode==5)
Calibration is a sub-mode controlled by `calibrationSubMode`.
- `CALIB_STEPPER`
  - encoder moves `calibrationTempTarget` by `STEPS_PER_MM`
  - stepper moves to that target while you adjust
- Short press advances calibration:
  - CALIB_STEPPER → CALIB_SERVO1
  - CALIB_SERVO1 → CALIB_SERVO2
  - CALIB_SERVO2 → save offsets and return to Mode 0
- Servo calibration:
  - In CALIB_SERVO1, encoder adjusts `calibrationServoAngle` while Servo1 runs and Servo2 stops
  - In CALIB_SERVO2, encoder adjusts `calibrationServoAngle` while Servo2 runs and Servo1 stops

#### EEPROM fields
- `EEPROM_STEPPER_OFFSET = 0`
- `EEPROM_SERVO1_OFFSET = 4`
- `EEPROM_SERVO2_OFFSET = 8`
- `EEPROM_MAGIC_NUMBER = 12` (`MAGIC_NUM = 0xABCD`)

## Controls & UI Behavior
### Encoder behavior (mode-specific)
- Mode 0: scrolls menu (`menuSelection`)
- Mode 1: knee mm (`manualTarget +=/- STEPS_PER_MM`)
- Mode 2: ankle angle (`targetServoAngle +=/- 1`) and drives both servos FWD/REV
- Mode 3: auto speed (`motorSpeed +=/- 50`)
- Mode 4: cycle count (`targetCycles +=/- 1`)
- Mode 5: calibration sub-target adjustment (stepper or servo angle depending on `calibrationSubMode`)

### Button behavior
- Short press (logic depends on current mode/state; see code `checkButton()`):
  - from Home selects mode
  - from Set Cycles starts Auto
  - from Calibration steps through servo calibration and saves to EEPROM
- Long press (>1000ms): toggles `emergencyStop`
  - when enabled: stepper is commanded to stop at current position and both servos go to `SERVO_STOP`
  - when disabled: performs reset transitions depending on current mode

## Main Loop Flow (from `loop()`)
1. `readEncoder()`
2. `checkButton()`
3. If not `emergencyStop`:
   - `Manual()` if mode==1 or mode==2
   - `Automatic()` if mode==3
   - `updateLCD()`
4. `updateServo()`
5. `stepper.run()`

## Build & Upload (PlatformIO)
```bash
pio run -t upload
pio device monitor
```

## Versioning / Doc Note
This `documentation.md` was regenerated to match the current `src/main.cpp` implementation (travel limits, constants, LCD strings, and the auto state machine).

