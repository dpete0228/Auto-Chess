#pragma once

#include <stdint.h>
#include <string>

#if defined(ARDUINO)
  #include <Arduino.h>
#endif

struct Coords {
  double x;
  double y;
};

extern const double STEPS_PER_INCH_X;
extern const double STEPS_PER_INCH_Y;

extern volatile long currStepsX;
extern volatile long currStepsY;

bool magnetOn();
bool magnetOff();
void sweepTile();
void setHome();

#if defined(ARDUINO)
Coords getCoords(const String& pos);
int gantryTo(const String& pos);
#endif

Coords getCoords(const char* pos);
Coords getCoords(const std::string& pos);

int gantryTo(double xPct, double yPct);
int gantryTo(const char* pos);
int gantryTo(const std::string& pos);

int setX(double percent);
int setY(double percent);
double getX();
double getY();

int castle(bool kingside);
int castle(const char* move);
int castle(const std::string& move);
#if defined(ARDUINO)
int castle(const String& move);
#endif
