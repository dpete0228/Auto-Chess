#ifndef GANTRY_H
#define GANTRY_H

#include <Arduino.h>
#include <AccelStepper.h>
#include <vector>

// Coordinate structure
struct Point {
    float x;
    float y;
};

// Queue Item structure for buffered movements
struct GantryCommand {
    enum Type { MOVE, MAGNET_ON, MAGNET_OFF, DELAY, HOME };
    Type type;
    float x;
    float y;
    float speed;
    unsigned long duration;
};