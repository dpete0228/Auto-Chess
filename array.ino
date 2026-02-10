#include <AccelStepper.h>

// Stepper pins
#define motorStep 14
#define motorDir 12
#define motorEnable 4

// Create stepper driver (1 = driver mode)
AccelStepper stepper(AccelStepper::DRIVER, motorStep, motorDir);

String inputString = "";
bool commandReady = false;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Stepper Serial Controller Ready");
  Serial.println("Type a number from -500 to 500 and press ENTER");

  // Enable pin
  pinMode(motorEnable, OUTPUT);
  digitalWrite(motorEnable, LOW);   // LOW usually enables A4988/DRV8825

  // Stepper settings
  stepper.setMaxSpeed(1000);
  stepper.setAcceleration(600);
}

void loop() {

  // ---- Read Serial Input ----
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      commandReady = true;
    } else {
      inputString += c;
    }
  }

  // ---- When Enter is pressed ----
  if (commandReady) {
    int steps = inputString.toInt();

    if (steps >= -500 && steps <= 500) {
      Serial.print("Moving: ");
      Serial.print(steps);
      Serial.println(" steps");

      // RELATIVE move
      stepper.move(steps);
    } else {
      Serial.println("ERROR: Enter value between -500 and 500");
    }

    inputString = "";
    commandReady = false;
  }

  // ---- This MUST run constantly ----
  stepper.run();
}
