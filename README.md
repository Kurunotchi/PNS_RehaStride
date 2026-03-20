# Robotics_PNS Project Documentation

## Project Overview

Advanced Arduino Nano-based Physical Rehabilitation System for knee/ankle therapy.

**Key Features:**
- 5-mode menu-driven interface (Home → Manual Leg / Manual Ankle / Set Cycles → Auto Rehab)
- Precision stepper control for knee extension (0-250mm travel)
- Continuous rotation servos for ankle push/relax (1° encoder precision)
- Real-time knee angle display (90° seated → 180° extended)
- Finite/Infinite auto cycles with dynamic speed control
- 1-button control: short-press mode toggle, 1s hold emergency stop/reset
- Optimized LCD updates, I2C 400kHz for smooth stepper motion

**Libraries:** AccelStepper, LiquidCrystal_I2C, Servo
**Board:** Arduino Nano ATMega328P

## Hardware Pinout

| Function          | Pin | Notes                  |
|-------------------|-----|------------------------|
| Stepper STEP      | 3   | AccelStepper::DRIVER  |
| Stepper DIR       | 4   |                        |
| Stepper ENABLE    | 5   | LOW=enabled           |
| Encoder CLK       | 9   | INPUT_PULLUP          |
| Encoder DT        | 8   | INPUT_PULLUP          |
| Encoder Button SW | 10  | INPUT_PULLUP, long-press E-stop |
| Servo1 (Ankle)    | 7   | Continuous rotation   |
| Servo2 (Ankle)    | 6   | Continuous rotation   |

**Notes:** Servos are continuous rotation type (speed/direction control, not position).

## Constants & Configuration

| Constant          | Value    | Description                          |
|-------------------|----------|--------------------------------------|
| `STEPS_PER_MM`    | 400L     | Microsteps per mm (calibrate)        |
| `MAX_TRAVEL_MM`   | 250L     | Max knee extension                   |
| `EXTEND_POSITION` | 250L     | Full extension mm                    |
| `MIN_SPEED`       | 200      | Min stepper steps/sec                |
| `MAX_SPEED`       | 1500     | Max stepper steps/sec                |
| `DEFAULT_SPEED`   | 500      | Initial speed                        |
| `HOLD_TIME`       | 2000     | Hold duration (ms)                   |
| `SERVO_STOP`      | 90       | Neutral/stop PWM                     |
| `SERVO_FWD`       | 108      | Forward rotation (calibrate)         |
| `SERVO_REV`       | 72       | Reverse rotation (calibrate)         |
| `MS_PER_DEGREE`   | 40       | Time for 1° at FWD/REV speed (calib) |

## Modes (0-4)

**Mode 0: Home Menu**
- LCD: `>Leg  Ank  Auto` (encoder selects, button enters)
- Encoder: Navigate menu (1:Manual Leg, 2:Manual Ankle, 3:Set Cycles)

**Mode 1: Manual Leg (Knee Extension)**
- LCD: `Ctrl: Leg (Knee)` / `Set: XXXmm`
- Encoder CW: +1mm (`+1*STEPS_PER_MM`), CCW: -5mm
- Stepper moves to `manualTarget` (0-250mm)

**Mode 2: Manual Ankle**
- LCD: `Ctrl: Ankle` / `Set: XXX°`
- Encoder: ±1° (`targetServoAngle` 0-180°), servos FWD/REV until stop (50ms idle)
- Continuous rotation control

**Mode 4: Set Cycles**
- LCD: `Set Auto Cycles:` / `> XX Cycles` or `> Infinite`
- Encoder: Adjust `targetCycles` (0=∞, 1-99), button → Mode 3

**Mode 3: Automatic Rehab**
- Cycles: `targetCycles` times or infinite
- Dynamic speed via encoder (±50 steps/sec)
- LCD: `Extend [1/10]` / `Spd:500 A:135°` + state
- State machine:

**Auto State Machine (`RehabState`):**
1. `MOVE_EXTEND`: Stepper to 250mm
2. `ANKLE_PUSH`: Servos FWD 500ms
3. `HOLD_EXTEND`: Wait 2s
4. `MOVE_HOME`: Stepper to 0mm
5. `ANKLE_RELAX`: Servos REV 500ms
6. `HOLD_HOME`: Wait 2s
7. Next cycle or stop if finite

**Knee Angle:** `map(currentPos, 0, 250*400, 90, 180)` °

## Controls & UI

**Encoder Behaviors (mode-specific):**
| Mode | CW/RIGHT | CCW/LEFT |
|------|----------|----------|
| 0 Home | Next menu | Prev menu |
| 1 Leg | +1mm | -5mm |
| 2 Ankle | +1° FWD | -1° REV |
| 3 Auto | +50 speed | -50 speed |
| 4 Cycles | +1 cycle | -1 cycle |

**Button:**
- Short press: Enter selected mode / toggle home / start cycles
- Hold 1s: Toggle `emergencyStop` (stops all, LCD "EMERGENCY STOP! Hold to Reset")

## Main Loop Flow

```
loop():
  readEncoder()        # Update targets/speed based on mode
  checkButton()        # Mode toggle / E-stop
  if !emergencyStop:
    if mode in [1,2]: Manual()      # Speed = motorSpeed
    elif mode == 3:    Automatic()  # State machine + dynamic speed
    updateLCD()         # Optimized, <100ms rate
  updateServo()         # Mode-specific logic
  stepper.run()         # Non-blocking motion
```

**Key Optimizations:**
- `stepper.run()` called every loop for smooth motion
- LCD conditional redraws (no flicker, low CPU)
- 1ms encoder debounce for responsive UI

## Build & Upload

```bash
# Install PlatformIO if needed, then:
pio run -t upload    # Build + flash to Nano
pio device monitor   # Serial monitor @115200
```

**Dependencies:** Auto-installed via `platformio.ini`

## Calibration Guide

1. **STEPS_PER_MM**: Full travel / 250mm steps (e.g., 400 for 1/8 microstep)
2. **SERVO_FWD/REV**: PWM for gentle rotation; time `MS_PER_DEGREE` @108/72 for 1°
3. **MS_PER_DEGREE**: Measure physical 1° turn time → encoder ° accuracy
4. **Angle Map**: Adjust if knee geometry ≠ linear 90-180°

## Extensions & Safety

- **Hardware:** Limit switches on stepper ends
- **Software:** EEPROM save/restore speed/cycles/manualTarget
- **Safety:** Current-limiting driver, soft-starts, E-stop tested
- **UI:** Add buzzer feedback, OLED upgrade for graphs

