#include "Gantry.h"

/*
Design Note (Knight/Diagonal Decomposition)
------------------------------------------
The gantry carries the magnet, so any sideways "cutting corners" can drag pieces off the center
of a tile (especially on fast diagonals). This module keeps the magnet close to tile centerlines
by segmenting motion:

1) General moves are broken into short straight segments (default 0.25 in). For each segment,
   both axes are commanded to an intermediate endpoint; the motors are stepped together using
   AccelStepper's non-blocking run() calls until both axes reach the intermediate target.

2) Knight moves (1 tile by 2 tiles) are decomposed into *cardinal-only* motion: we move the
   2-tile leg first along one axis, then the 1-tile leg along the other axis, both in 0.25 in
   increments. This keeps the magnet directly over a file/rank centerline for the entire move,
   rather than cutting a diagonal through adjacent squares.
*/

#include <cmath>
#include <cctype>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#if defined(ARDUINO_ARCH_ESP32)
  #include <FS.h>
  #include <SPIFFS.h>
#else
  #include <fstream>
  #include <iostream>
  #include <thread>
  #include <chrono>
#endif

#include <AccelStepper.h>

#if defined(__GNUC__)
__attribute__((weak))
#endif
const double STEPS_PER_INCH_X = 127.93;

#if defined(__GNUC__)
__attribute__((weak))
#endif
const double STEPS_PER_INCH_Y = 114.48;

#if defined(__GNUC__)
__attribute__((weak))
#endif
volatile long currStepsX = 0;

#if defined(__GNUC__)
__attribute__((weak))
#endif
volatile long currStepsY = 0;

static constexpr double X_MIN_PCT = -10.0;
static constexpr double X_MAX_PCT = 110.0;
static constexpr double Y_MIN_PCT = 0.0;
static constexpr double Y_MAX_PCT = 110.0;

static constexpr long X_RANGE_STEPS = 31500;
static constexpr long Y_RANGE_STEPS = 25000;

static constexpr long SQUARE_STEPS = 3500;
static constexpr long SEGMENT_STEPS_DEFAULT = 500;
static constexpr double SWEEP_RADIUS_CM = 1.0;

static constexpr double PI = 3.14159265358979323846;

static constexpr int X_STEP_PIN = 2;
static constexpr int X_DIR_PIN = 4;
static constexpr int Y_STEP_PIN = 32;
static constexpr int Y_DIR_PIN = 33;
static constexpr int ENABLE_PIN = 14;

static constexpr int MAGNET_GATE_PIN = 13;
static constexpr int HALL_PIN = 34;

static constexpr float MAX_SPEED_STEPS_S = 1800.0f;
static constexpr float ACCEL_STEPS_S2 = 900.0f;

static AccelStepper stepperX(AccelStepper::DRIVER, X_STEP_PIN, X_DIR_PIN);
static AccelStepper stepperY(AccelStepper::DRIVER, Y_STEP_PIN, Y_DIR_PIN);

static bool initialized = false;

static void ensureInitialized() {
  if (initialized) {
    return;
  }

#if defined(ARDUINO)
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW);

  pinMode(MAGNET_GATE_PIN, OUTPUT);
  digitalWrite(MAGNET_GATE_PIN, LOW);

  pinMode(HALL_PIN, INPUT);
#endif

  stepperX.setMaxSpeed(MAX_SPEED_STEPS_S);
  stepperX.setAcceleration(ACCEL_STEPS_S2);
  stepperY.setMaxSpeed(MAX_SPEED_STEPS_S);
  stepperY.setAcceleration(ACCEL_STEPS_S2);

  stepperX.setCurrentPosition(currStepsX);
  stepperY.setCurrentPosition(currStepsY);

  initialized = true;
}

static inline double stepsToPctX(long steps) {
  return (static_cast<double>(steps) * 100.0) / static_cast<double>(X_RANGE_STEPS);
}

static inline double stepsToPctY(long steps) {
  return (static_cast<double>(steps) * 100.0) / static_cast<double>(Y_RANGE_STEPS);
}

static inline long pctToStepsX(double pct) {
  return static_cast<long>(llround((pct / 100.0) * static_cast<double>(X_RANGE_STEPS)));
}

static inline long pctToStepsY(double pct) {
  return static_cast<long>(llround((pct / 100.0) * static_cast<double>(Y_RANGE_STEPS)));
}

static inline bool inLimits(double xIn, double yIn) {
  return (xIn >= X_MIN_PCT && yIn >= Y_MIN_PCT && xIn <= X_MAX_PCT && yIn <= Y_MAX_PCT);
}

static bool hallDetected() {
#if defined(ARDUINO)
  return digitalRead(HALL_PIN) == HIGH;
#else
  return false;
#endif
}

static void runToStepsBlocking(long targetStepsX, long targetStepsY) {
  ensureInitialized();

  stepperX.moveTo(targetStepsX);
  stepperY.moveTo(targetStepsY);

  while (stepperX.distanceToGo() != 0 || stepperY.distanceToGo() != 0) {
    stepperX.run();
    stepperY.run();

#if defined(ARDUINO)
    delay(0);
#endif
  }

  currStepsX = stepperX.currentPosition();
  currStepsY = stepperY.currentPosition();
}

static inline bool inLimitsSteps(long xSteps, long ySteps) {
  const long minX = pctToStepsX(X_MIN_PCT);
  const long maxX = pctToStepsX(X_MAX_PCT);
  const long minY = pctToStepsY(Y_MIN_PCT);
  const long maxY = pctToStepsY(Y_MAX_PCT);
  return (xSteps >= minX && xSteps <= maxX && ySteps >= minY && ySteps <= maxY);
}

static int gantryToSegmentedStepsInternal(long targetStepsX, long targetStepsY, long segmentSteps, bool cardinalOnly) {
  if (!inLimitsSteps(targetStepsX, targetStepsY)) {
    return -1;
  }

  const long startStepsX = currStepsX;
  const long startStepsY = currStepsY;
  const long dx = targetStepsX - startStepsX;
  const long dy = targetStepsY - startStepsY;

  if (dx == 0 && dy == 0) {
    return 0;
  }

  const long step = std::max(1L, segmentSteps);

  if (cardinalOnly) {
    if (dx != 0) {
      const int n = static_cast<int>(std::ceil(std::abs(static_cast<double>(dx)) / static_cast<double>(step)));
      for (int i = 1; i <= n; ++i) {
        const long xi = startStepsX + static_cast<long>(llround(static_cast<double>(dx) * (static_cast<double>(i) / n)));
        runToStepsBlocking(xi, startStepsY);
        if (!inLimitsSteps(currStepsX, currStepsY)) {
          return -1;
        }
      }
    }

    if (dy != 0) {
      const int n = static_cast<int>(std::ceil(std::abs(static_cast<double>(dy)) / static_cast<double>(step)));
      for (int i = 1; i <= n; ++i) {
        const long yi = startStepsY + static_cast<long>(llround(static_cast<double>(dy) * (static_cast<double>(i) / n)));
        runToStepsBlocking(targetStepsX, yi);
        if (!inLimitsSteps(currStepsX, currStepsY)) {
          return -1;
        }
      }
    }

    return 0;
  }

  const double distanceSteps = std::hypot(static_cast<double>(dx), static_cast<double>(dy));
  const int segments = std::max(1, static_cast<int>(std::ceil(distanceSteps / static_cast<double>(step))));

  for (int i = 1; i <= segments; ++i) {
    const double t = static_cast<double>(i) / segments;
    const long xi = startStepsX + static_cast<long>(llround(static_cast<double>(dx) * t));
    const long yi = startStepsY + static_cast<long>(llround(static_cast<double>(dy) * t));
    runToStepsBlocking(xi, yi);
    if (!inLimitsSteps(currStepsX, currStepsY)) {
      return -1;
    }
  }

  return 0;
}

static inline bool approxEqual(double a, double b, double tol = 1e-2) {
  return std::abs(a - b) <= tol;
}

bool magnetOn() {
  ensureInitialized();

#if defined(ARDUINO)
  digitalWrite(MAGNET_GATE_PIN, HIGH);
  delay(50);
  return hallDetected();
#else
  return true;
#endif
}
/*
Purpose:
  Energize the electromagnet and confirm pickup via the Hall/reed input.
Parameters / Return:
  Return: true if the Hall-sensor indicates a piece is present after the 50 ms settle time.
Units:
  Timing uses milliseconds; Hall sensor is treated as a digital present/absent signal.
Hardware assumptions:
  - MAGNET_GATE_PIN drives a MOSFET gate (HIGH = magnet on).
  - HALL_PIN reads a digital "piece detected" signal (HIGH = detected).
Usage example:
  if (magnetOn()) { Serial.println("Piece grabbed"); }
*/

bool magnetOff() {
  ensureInitialized();

#if defined(ARDUINO)
  digitalWrite(MAGNET_GATE_PIN, LOW);
  delay(50);
  return !hallDetected();
#else
  return true;
#endif
}
/*
Purpose:
  De-energize the electromagnet and confirm release via the Hall/reed input.
Parameters / Return:
  Return: true if the Hall-sensor indicates no piece is present after the 50 ms settle time.
Units:
  Timing uses milliseconds; Hall sensor is treated as a digital present/absent signal.
Hardware assumptions:
  - MAGNET_GATE_PIN drives a MOSFET gate (LOW = magnet off).
  - HALL_PIN reads a digital "piece detected" signal (HIGH = detected).
Usage example:
  (void)magnetOff(); // drop piece before retreating
*/

void setHome() {
  ensureInitialized();
  currStepsX = 0;
  currStepsY = 0;
  stepperX.setCurrentPosition(0);
  stepperY.setCurrentPosition(0);
#if defined(ARDUINO)
  Serial.println("Gantry home position manually set to (0,0).");
#endif
}

void sweepTile() {
  ensureInitialized();

  const long centerStepsX = currStepsX;
  const long centerStepsY = currStepsY;

  if (hallDetected()) {
    return;
  }

  const double maxTheta = 10.0 * PI;
  const double stepsPerCm = static_cast<double>(SQUARE_STEPS) / (1.75 * 2.54);
  const double radiusSteps = SWEEP_RADIUS_CM * stepsPerCm;
  const double a = radiusSteps / maxTheta;
  const double thetaStep = 0.35;

  for (double theta = 0.0; theta <= maxTheta; theta += thetaStep) {
    if (hallDetected()) {
      return;
    }

    const double r = std::min(radiusSteps, a * theta);
    const long x = centerStepsX + static_cast<long>(llround(r * std::cos(theta)));
    const long y = centerStepsY + static_cast<long>(llround(r * std::sin(theta)));

    const long minX = pctToStepsX(X_MIN_PCT);
    const long maxX = pctToStepsX(X_MAX_PCT);
    const long minY = pctToStepsY(Y_MIN_PCT);
    const long maxY = pctToStepsY(Y_MAX_PCT);

    const long clampedX = std::min(std::max(minX, x), maxX);
    const long clampedY = std::min(std::max(minY, y), maxY);

    (void)gantryToSegmentedStepsInternal(clampedX, clampedY, 100, false);
  }

  (void)gantryToSegmentedStepsInternal(centerStepsX, centerStepsY, 100, false);
}
/*
Purpose:
  Perform a small-radius spiral search to improve pickup reliability when the magnet is near a piece.
Parameters / Return:
  None. Exits immediately if the Hall-sensor triggers during the search.
Units:
  Radius is 1 cm; motion points are generated in motor steps.
Hardware assumptions:
  - The Hall sensor can detect a piece while moving.
  - The gantry can safely move within +/- 1 cm of the current square center.
Usage example:
  magnetOn();
  sweepTile(); // recenters over the piece if initial contact was off-center
*/

static bool tryParseCsvLine(const std::string& line, std::string& keyOut, double& xOut, double& yOut) {
  std::string s = line;
  if (s.empty()) {
    return false;
  }

  const auto trim = [](std::string& t) {
    while (!t.empty() && (t.back() == '\r' || t.back() == '\n' || t.back() == ' ' || t.back() == '\t')) {
      t.pop_back();
    }
    size_t start = 0;
    while (start < t.size() && (t[start] == ' ' || t[start] == '\t')) {
      ++start;
    }
    if (start > 0) {
      t.erase(0, start);
    }
  };

  trim(s);
  if (s.empty()) {
    return false;
  }

  std::string a, b, c;
  {
    std::stringstream ss(s);
    if (!std::getline(ss, a, ',')) {
      return false;
    }
    if (!std::getline(ss, b, ',')) {
      return false;
    }
    if (!std::getline(ss, c)) {
      return false;
    }
  }

  trim(a);
  trim(b);
  trim(c);

  if (a.empty() || b.empty() || c.empty()) {
    return false;
  }

  if (a == "Position") {
    return false;
  }

  try {
    xOut = std::stod(b);
    yOut = std::stod(c);
  } catch (...) {
    return false;
  }

  keyOut = a;
  return true;
}

[[noreturn]] static void failCoords(const char* message) {
#if defined(__cpp_exceptions)
  throw std::runtime_error(message);
#else
  #if defined(ARDUINO)
    Serial.println(message);
  #endif
  while (true) {
  }
#endif
}

static Coords getCoordsFromCsv(const std::string& pos) {
  if (pos.size() != 2) {
    failCoords("getCoords: expected algebraic square like \"a1\"..\"h8\"");
  }

  const char file = static_cast<char>(std::tolower(static_cast<unsigned char>(pos[0])));
  const char rank = pos[1];
  if (file < 'a' || file > 'h' || rank < '1' || rank > '8') {
    failCoords("getCoords: invalid square; expected \"a1\"..\"h8\"");
  }

#if defined(ARDUINO_ARCH_ESP32)
  ensureInitialized();
  if (!SPIFFS.begin(true)) {
    failCoords("getCoords: SPIFFS mount failed");
  }

  File f = SPIFFS.open("/board-positions.csv", "r");
  if (!f) {
    f = SPIFFS.open("/chess_pos.csv", "r");
  }
  if (!f) {
    failCoords("getCoords: could not open CSV (SPIFFS)");
  }

  while (f.available()) {
    const String line = f.readStringUntil('\n');
    std::string key;
    double x = 0.0;
    double y = 0.0;
    if (!tryParseCsvLine(line.c_str(), key, x, y)) {
      continue;
    }
    if (key == pos) {
      return Coords{x, y};
    }
  }

  failCoords("getCoords: square not found in CSV");
#else
  const char* candidates[] = {"board-positions.csv", "chess_pos.csv"};
  for (const char* path : candidates) {
    std::ifstream in(path);
    if (!in.is_open()) {
      continue;
    }

    std::string line;
    while (std::getline(in, line)) {
      std::string key;
      double x = 0.0;
      double y = 0.0;
      if (!tryParseCsvLine(line, key, x, y)) {
        continue;
      }
      if (key == pos) {
        return Coords{x, y};
      }
    }
  }

  failCoords("getCoords: square not found in CSV");
#endif
}

#if defined(ARDUINO)
Coords getCoords(const String& pos) {
  return getCoordsFromCsv(std::string(pos.c_str()));
}
/*
Purpose:
  Convert a chess square label into gantry coordinates using the CSV lookup.
Parameters / Return:
  pos: Arduino String containing "a1".."h8".
  Return: Coords{x,y} in percent; fails on invalid input or missing CSV entry.
Units:
  Percent for both x and y (0–100% covers the nominal board travel range).
Hardware assumptions:
  - The board-position CSV exists on the device filesystem (SPIFFS on ESP32) as "/board-positions.csv".
Usage example:
  Coords c = getCoords(String("e4"));
*/
#endif

Coords getCoords(const char* pos) {
  if (pos == nullptr) {
    failCoords("getCoords: null string");
  }
  return getCoordsFromCsv(std::string(pos));
}
/*
Purpose:
  Convert a chess square label into gantry coordinates using the CSV lookup.
Parameters / Return:
  pos: C-string containing "a1".."h8".
  Return: Coords{x,y} in percent; fails on invalid input or missing CSV entry.
Units:
  Percent for both x and y (0–100% covers the nominal board travel range).
Hardware assumptions:
  - A CSV mapping file exists as "board-positions.csv" (preferred) or "chess_pos.csv".
Usage example:
  Coords c = getCoords("b7");
*/

Coords getCoords(const std::string& pos) {
  return getCoordsFromCsv(pos);
}
/*
Purpose:
  Convert a chess square label into gantry coordinates using the CSV lookup.
Parameters / Return:
  pos: std::string containing "a1".."h8".
  Return: Coords{x,y} in percent; throws std::runtime_error on failures when exceptions are enabled.
Units:
  Percent for both x and y (0–100% covers the nominal board travel range).
Hardware assumptions:
  - The board-position CSV is available to the runtime environment.
Usage example:
  Coords c = getCoords(std::string("a1"));
*/

int gantryTo(double xPct, double yPct) {
  ensureInitialized();

  if (!inLimits(xPct, yPct)) {
    return -1;
  }

  const long targetStepsX = pctToStepsX(xPct);
  const long targetStepsY = pctToStepsY(yPct);
  const long dx = targetStepsX - currStepsX;
  const long dy = targetStepsY - currStepsY;

  const long adx = std::abs(dx);
  const long ady = std::abs(dy);

  const bool isKnight =
      (adx == SQUARE_STEPS && ady == 2 * SQUARE_STEPS) ||
      (adx == 2 * SQUARE_STEPS && ady == SQUARE_STEPS);

  if (isKnight) {
    const bool longIsX = adx > ady;
    const long longDelta = longIsX ? dx : dy;
    const long shortDelta = longIsX ? dy : dx;

    if (longIsX) {
      if (gantryToSegmentedStepsInternal(currStepsX + longDelta, currStepsY, SEGMENT_STEPS_DEFAULT, true) != 0) {
        return -1;
      }
      if (gantryToSegmentedStepsInternal(targetStepsX, targetStepsY, SEGMENT_STEPS_DEFAULT, true) != 0) {
        return -1;
      }
    } else {
      if (gantryToSegmentedStepsInternal(currStepsX, currStepsY + longDelta, SEGMENT_STEPS_DEFAULT, true) != 0) {
        return -1;
      }
      if (gantryToSegmentedStepsInternal(targetStepsX, targetStepsY, SEGMENT_STEPS_DEFAULT, true) != 0) {
        return -1;
      }
    }

    (void)shortDelta;
    return 0;
  }

  return gantryToSegmentedStepsInternal(targetStepsX, targetStepsY, SEGMENT_STEPS_DEFAULT, false);
}
/*
Purpose:
  Move the gantry to an (x,y) workspace target using step-segmented motion for accuracy and safety.
Parameters / Return:
  xPct, yPct: Target position in percent.
  Return: 0 on success; -1 if the target is outside physical limits.
Units:
  Percent for inputs; internal motion uses absolute motor steps derived from X_RANGE_STEPS/Y_RANGE_STEPS.
Hardware assumptions:
  - AccelStepper is configured for the correct STEP/DIR pins and motor drivers.
  - currStepsX/currStepsY reflect the last known absolute motor positions.
Usage example:
  (void)gantryTo(50.0, 50.0);
*/

#if defined(ARDUINO)
int gantryTo(const String& pos) {
  const Coords c = getCoords(pos);
  return gantryTo(c.x, c.y);
}
/*
Purpose:
  Move the gantry to a chess square specified as an Arduino String.
Parameters / Return:
  pos: Arduino String containing "a1".."h8".
  Return: 0 on success; -1 on limit error (or if coordinate lookup fails).
Units:
  Square labels map to percent via getCoords(); motion executes in steps.
Hardware assumptions:
  - The board-position CSV is available and accurate.
Usage example:
  (void)gantryTo(String("h8"));
*/
#endif

int gantryTo(const char* pos) {
  const Coords c = getCoords(pos);
  return gantryTo(c.x, c.y);
}
/*
Purpose:
  Move the gantry to a chess square specified as a C-string.
Parameters / Return:
  pos: C-string containing "a1".."h8".
  Return: 0 on success; -1 on limit error (or if coordinate lookup fails).
Units:
  Square labels map to percent via getCoords(); motion executes in steps.
Hardware assumptions:
  - The board-position CSV is available and accurate.
Usage example:
  (void)gantryTo("a1");
*/

int gantryTo(const std::string& pos) {
  const Coords c = getCoords(pos);
  return gantryTo(c.x, c.y);
}
/*
Purpose:
  Move the gantry to a chess square specified as a std::string.
Parameters / Return:
  pos: std::string containing "a1".."h8".
  Return: 0 on success; -1 on limit error (or if coordinate lookup fails).
Units:
  Square labels map to percent via getCoords(); motion executes in steps.
Hardware assumptions:
  - The board-position CSV is available and accurate.
Usage example:
  (void)gantryTo(std::string("e4"));
*/

int setX(double percent) {
  ensureInitialized();

  if (!inLimits(percent, getY())) {
    return -1;
  }

  runToStepsBlocking(pctToStepsX(percent), currStepsY);
  return 0;
}
/*
Purpose:
  Command an absolute X-axis move in percent (Y is held constant).
Parameters / Return:
  percent: Absolute X target in percent.
  Return: 0 on success; -1 if the requested X target violates limits.
Units:
  Percent for input; internal motion is in steps via X_RANGE_STEPS.
Hardware assumptions:
  - X axis is driven by stepperX and is calibrated by X_RANGE_STEPS over 0–100%.
Usage example:
  setX(50.0);
*/

int setY(double percent) {
  ensureInitialized();

  if (!inLimits(getX(), percent)) {
    return -1;
  }

  runToStepsBlocking(currStepsX, pctToStepsY(percent));
  return 0;
}
/*
Purpose:
  Command an absolute Y-axis move in percent (X is held constant).
Parameters / Return:
  percent: Absolute Y target in percent.
  Return: 0 on success; -1 if the requested Y target violates limits.
Units:
  Percent for input; internal motion is in steps via Y_RANGE_STEPS.
Hardware assumptions:
  - Y axis is driven by stepperY and is calibrated by Y_RANGE_STEPS over 0–100%.
Usage example:
  setY(50.0);
*/

double getX() {
  ensureInitialized();
  stepperX.setCurrentPosition(currStepsX);
  return stepsToPctX(currStepsX);
}
/*
Purpose:
  Read the current X position in percent based on currStepsX and the configured X travel range.
Parameters / Return:
  Return: Current X position (percent).
Units:
  Percent.
Hardware assumptions:
  - currStepsX is maintained correctly by all motion functions in this module.
Usage example:
  Serial.println(getX(), 3);
*/

double getY() {
  ensureInitialized();
  stepperY.setCurrentPosition(currStepsY);
  return stepsToPctY(currStepsY);
}
/*
Purpose:
  Read the current Y position in percent based on currStepsY and the configured Y travel range.
Parameters / Return:
  Return: Current Y position (percent).
Units:
  Percent.
Hardware assumptions:
  - currStepsY is maintained correctly by all motion functions in this module.
Usage example:
  Serial.println(getY(), 3);
*/

static int gantryToCardinal025(double xPct, double yPct) {
  return gantryToSegmentedStepsInternal(pctToStepsX(xPct), pctToStepsY(yPct), SEGMENT_STEPS_DEFAULT, true);
}

int castle(bool kingside) {
  ensureInitialized();

  const double y = getY();
  const double rank1 = getCoords("e1").y;
  const double rank8 = getCoords("e8").y;
  const bool whiteSide = std::abs(y - rank1) <= std::abs(y - rank8);

  const char* kingStart = whiteSide ? "e1" : "e8";
  const char* rookStart = kingside ? (whiteSide ? "h1" : "h8") : (whiteSide ? "a1" : "a8");
  const char* kingFinal = kingside ? (whiteSide ? "g1" : "g8") : (whiteSide ? "c1" : "c8");
  const char* rookFinal = kingside ? (whiteSide ? "f1" : "f8") : (whiteSide ? "d1" : "d8");
  const char* kingHalf  = kingside ? (whiteSide ? "f1" : "f8") : (whiteSide ? "d1" : "d8");

  const Coords k0 = getCoords(kingStart);
  const Coords kh = getCoords(kingHalf);
  const Coords kf = getCoords(kingFinal);
  const Coords r0 = getCoords(rookStart);
  const Coords rf = getCoords(rookFinal);

  if (gantryToCardinal025(k0.x, k0.y) != 0) {
    return -1;
  }
  if (!magnetOn()) {
    return -1;
  }
  sweepTile();
  if (gantryToCardinal025(kh.x, kh.y) != 0) {
    return -1;
  }
  if (!magnetOff()) {
    return -1;
  }

  if (gantryToCardinal025(r0.x, r0.y) != 0) {
    return -1;
  }
  if (!magnetOn()) {
    return -1;
  }
  sweepTile();
  if (gantryToCardinal025(rf.x, rf.y) != 0) {
    return -1;
  }
  if (!magnetOff()) {
    return -1;
  }

  if (gantryToCardinal025(kh.x, kh.y) != 0) {
    return -1;
  }
  if (!magnetOn()) {
    return -1;
  }
  sweepTile();
  if (gantryToCardinal025(kf.x, kf.y) != 0) {
    return -1;
  }
  if (!magnetOff()) {
    return -1;
  }

  return 0;
}

int castle(const std::string& move) {
  if (move == "O-O" || move == "o-o" || move == "0-0") {
    return castle(true);
  } else if (move == "O-O-O" || move == "o-o-o" || move == "0-0-0") {
    return castle(false);
  }
  return -1;
}

int castle(const char* move) {
  if (move == nullptr) {
    return -1;
  }
  return castle(std::string(move));
}

#if defined(ARDUINO)
int castle(const String& move) {
  return castle(std::string(move.c_str()));
}
#endif
/*
Purpose:
  Execute a complete castling sequence by moving the king halfway, relocating the rook, then
  finishing the king move, using only 0.25-inch cardinal steps for predictable clearance.
Parameters / Return:
  kingside: true for kingside castling (e.g., e1g1 + h1f1), false for queenside.
  Return: 0 on success; -1 on limit errors or Hall-confirmation failures during pickups.
Units:
  Motion coordinates are percent; segmentation is fixed at a small step increment.
Hardware assumptions:
  - Pieces are present at the correct starting squares.
  - Hall sensor reads HIGH when a piece is attached to the magnet.
  - Magnet can safely lift and transport king/rook without collisions.
Usage example:
  (void)gantryTo("e1");
  if (castle(true) == 0) { Serial.println("Castled kingside"); }
*/

#if !defined(ARDUINO)
static bool near(double a, double b, double tol) {
  return std::abs(a - b) <= tol;
}

int main() {
  try {
    ensureInitialized();

    currStepsX = 0;
    currStepsY = 0;
    stepperX.setCurrentPosition(0);
    stepperY.setCurrentPosition(0);

    const double tol = 0.01;

    {
      const Coords a1 = getCoords("a1");
      if (gantryTo(a1.x, a1.y) != 0) {
        return 1;
        
      }
      if (!near(getX(), a1.x, tol) || !near(getY(), a1.y, tol)) {
        return 2;
      }
      std::cout << "a1: " << getX() << ", " << getY() << "\n";
    }

    {
      const Coords h8 = getCoords("h8");
      if (gantryTo("h8") != 0) {
        return 3;
      }
      if (!near(getX(), h8.x, tol) || !near(getY(), h8.y, tol)) {
        return 4;
      }
      std::cout << "h8: " << getX() << ", " << getY() << "\n";
    }

    {
      const Coords b3 = getCoords("b3");
      if (gantryTo(b3.x, b3.y) != 0) {
        return 5;
      }
      if (!near(getX(), b3.x, tol) || !near(getY(), b3.y, tol)) {
        return 6;
      }
      std::cout << "b3: " << getX() << ", " << getY() << "\n";
    }

    {
      if (setX(7.0) != 0) {
        return 7;
      }
      if (!near(getX(), 7.0, tol)) {
        return 8;
      }
      std::cout << "setX: " << getX() << ", " << getY() << "\n";
    }

    {
      if (setY(9.0) != 0) {
        return 9;
      }
      if (!near(getY(), 9.0, tol)) {
        return 10;
      }
      std::cout << "setY: " << getX() << ", " << getY() << "\n";
    }

    (void)magnetOn();
    sweepTile();
    (void)magnetOff();

    {
      const Coords e1 = getCoords("e1");
      if (gantryTo("e1") != 0) {
        return 11;
      }
      if (!near(getX(), e1.x, tol) || !near(getY(), e1.y, tol)) {
        return 12;
      }
      std::cout << "e1: " << getX() << ", " << getY() << "\n";

      if (castle(true) != 0) {
        return 13;
      }
      const Coords g1 = getCoords("g1");
      if (!near(getX(), g1.x, tol) || !near(getY(), g1.y, tol)) {
        return 14;
      }
      std::cout << "castle(kingside): " << getX() << ", " << getY() << "\n";
    }

    std::cout << "GANTRY_OK\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "SELF_TEST_ERROR: " << e.what() << "\n";
    return 100;
  }
}
#endif
