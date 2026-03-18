#include "Gantry.ino"

// Constants based on board specs
#define TILE_SIZE 1.75
#define PIECE_OFFSET 0.875 // Center of first tile
#define JAIL_X_BLACK -0.875
#define JAIL_X_WHITE 14.875
#define BOARD_LIMIT_X 16.0
#define BOARD_LIMIT_Y 16.0
#define MICROSTEPPING 16 // Assumed 1/16 microstepping driver setting

// Constructor
Gantry::Gantry(int stepX, int dirX, int stepY, int dirY, int magnet, int hall, int limX, int limY, int enable, int stallX, int stallY) {
    pinStepX = stepX; pinDirX = dirX;
    pinStepY = stepY; pinDirY = dirY;
    pinMagnet = magnet;
    pinHall = hall;
    pinLimitX = limX; pinLimitY = limY;
    pinEnable = enable;
    pinStallX = stallX; pinStallY = stallY; // Set stall pins

    // Create AccelStepper instances (Interface 1 = Driver)
    stepperX = new AccelStepper(AccelStepper::DRIVER, pinStepX, pinDirX);
    stepperY = new AccelStepper(AccelStepper::DRIVER, pinStepY, pinDirY);

    stepsPerInch = 2038.0; 
    
    currentX_in = 0.0;
    currentY_in = 0.0;
    isHomed = false;
    magnetState = false;
    isRampingMagnet = false;
    magnetCurrentPWM = 0;
    isBusy = false;
}

void Gantry::begin() {
    // Pin Setup
    pinMode(pinMagnet, OUTPUT);
    pinMode(pinHall, INPUT);
    pinMode(pinLimitX, INPUT_PULLUP);
    pinMode(pinLimitY, INPUT_PULLUP);
    pinMode(pinEnable, OUTPUT);
    if (pinStallX != -1) pinMode(pinStallX, INPUT_PULLUP);
    if (pinStallY != -1) pinMode(pinStallY, INPUT_PULLUP);
    
    digitalWrite(pinEnable, LOW); // Enable motors 
    digitalWrite(pinMagnet, LOW);

    // Stepper Defaults
    stepperX->setMaxSpeed(2000);
    stepperX->setAcceleration(1000);
    stepperY->setMaxSpeed(2000);
    stepperY->setAcceleration(1000);
}

// ==========================================
// 1. Stepper Motor Control
// ==========================================

long Gantry::inchesToSteps(float inches) {
    return (long)(inches * stepsPerInch);
}

float Gantry::stepsToInches(long steps) {
    return (float)steps / stepsPerInch;
}

void Gantry::setX(float inches) {
    if (!checkBoundaries(inches, currentY_in)) return;
    long steps = inchesToSteps(inches);
    stepperX->moveTo(steps);
    currentX_in = inches;
}

void Gantry::setY(float inches) {
    if (!checkBoundaries(currentX_in, inches)) return;
    long steps = inchesToSteps(inches);
    stepperY->moveTo(steps);
    currentY_in = inches;
}

float Gantry::getX() {
    return stepsToInches(stepperX->currentPosition());
}

float Gantry::getY() {
    return stepsToInches(stepperY->currentPosition());
}

void Gantry::setSpeedProfile(float max_speed_steps, float accel_steps) {
    stepperX->setMaxSpeed(max_speed_steps);
    stepperX->setAcceleration(accel_steps);
    stepperY->setMaxSpeed(max_speed_steps);
    stepperY->setAcceleration(accel_steps);
}

