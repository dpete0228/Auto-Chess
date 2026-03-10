#include <AccelStepper.h>
#include <MultiStepper.h>

// Define motors and pins
AccelStepper motor1(14, 12, 4); // (Type:driver, Step, Dir)
AccelStepper motor2(); // Need pins here

MultiStepper steppers;

void setup() {
  steppers.addStepper(motor1);
  steppers.addStepper(motor2);
}

void loop() {
  long positions[2]; 
  positions[0] = 1000; // Motor 1 target
  positions[1] = 500;  // Motor 2 target (creates a shallow diagonal)
  
  steppers.moveTo(positions);
  steppers.runSpeedToPosition(); // Blocks until both reach the goal
  delay(1000);
}