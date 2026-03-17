Features

Dual Operation Modes

Manual Mode – User-controlled positioning via rotary encoder

Automatic Rehab Mode – Pre-programmed extension and return cycles

Precision Motion Control

Adjustable travel distance using stepper motor (mm-based movement)

Smooth acceleration and speed control

Rehabilitation Cycle Logic

Extend → Hold → Return → Hold sequence

Configurable repetitions and pause intervals

User Interface

16x2 I2C LCD for real-time feedback

Rotary encoder for navigation and adjustments

Safety System

Emergency stop via long-press button

Immediate motion halt and system reset capability

Components

Component and	Description

Arduino Board	Any Arduino-compatible microcontroller to run the firmware and control the stepper motor.

Stepper Motor	Provides precise linear motion; typically paired with a driver like A4988 or DRV8825.

Stepper Motor Driver	Controls current, direction, and speed of the stepper motor. 

Examples: A4988, DRV8825.

Rotary Encoder with Push Button	Used for manual positioning and mode selection; supports both rotation and button press detection.

16x2 I2C LCD	Displays system status, current mode, speed, and repetition count.

Linear Actuator / Lead Screw	Converts rotational motion of the stepper motor into linear motion for rehabilitation exercises.

Power Supply	Provides sufficient voltage and current for the stepper motor and Arduino board.

Wires & Connectors	For proper connections between motor, driver, sensors, and Arduino.

Frame / Mounting Hardware	Mechanical structure to hold the motor, actuator, and sensors securely.
