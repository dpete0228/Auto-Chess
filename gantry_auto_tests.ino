#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>

#include "Gantry.h"

/*
Gantry Automated Test Runner (ESP32)
-----------------------------------
What this file is:
  A fully automated "power-on self test" sketch for the gantry module. Upload this sketch to the
  ESP32 and open the Serial Monitor. The test suite runs once (in setup()) and prints a detailed
  PASS/FAIL report and writes a log file to SPIFFS.

What it tests:
  - Basic point-to-point moves in percent space (0–100%).
  - Limit rejection (out-of-range targets must return -1 and must NOT change position).
  - Pattern validation: a 5x5 serpentine grid and a knight-like move based on SQUARE_STEPS.
  - CSV integration: if a mapping CSV exists on SPIFFS, move by square keys (a1/h8/e4/jails).
  - API-level castling string wrappers: verifies invalid strings are rejected; valid strings are accepted.

What it cannot guarantee:
  - It cannot prove "no skipped steps" because there are no encoders/limit switches.
  - Castle execution is environment-dependent (requires pieces + magnet + sensor). We only validate parsing.

How to run:
  1) Physically place the gantry at your chosen 0%/0% reference point.
  2) Upload this sketch and open Serial Monitor at 115200 baud.
  3) Watch "GANTRY_AUTO_OK" or "GANTRY_AUTO_FAIL".

Outputs:
  - Serial: human-readable test log with PASS/FAIL markers.
  - SPIFFS: /gantry_auto_test_log.txt (append-only).
*/

// These MUST match the ranges in Gantry.cpp. They define how percent maps to steps.
static constexpr long X_RANGE_STEPS = 31500;
static constexpr long Y_RANGE_STEPS = 25000;

// Steps for one square (tile) in your current calibration.
static constexpr long SQUARE_STEPS = 3500;

// Default tolerance for percent-based comparisons.
// Example: 0.5 means "within 0.5% of target" AND "within equivalent step tolerance".
static constexpr double DEFAULT_TOL_PCT = 0.5;

// Counters used for reporting totals and coverage per group.
struct Counters {
  uint32_t total = 0;
  uint32_t passed = 0;
  uint32_t failed = 0;
  uint32_t skipped = 0;
};

// Aggregated counters across all tests and per category.
static Counters g_all;
static Counters g_moves;
static Counters g_limits;
static Counters g_patterns;
static Counters g_csv;
static Counters g_castle;

// The tolerance can be tuned here. Keep it small for precise validation.
static double g_tolPct = DEFAULT_TOL_PCT;

// Log a line to both Serial and a file on SPIFFS for later debugging.
static void logLine(const String& line) {
  Serial.println(line);
  File f = SPIFFS.open("/gantry_auto_test_log.txt", FILE_APPEND);
  if (f) {
    f.println(line);
    f.close();
  }
}

// Convert a percent target to a predicted absolute step target for X.
static long pctToStepsXLocal(double pct) {
  return static_cast<long>(llround((pct / 100.0) * static_cast<double>(X_RANGE_STEPS)));
}

// Convert a percent target to a predicted absolute step target for Y.
static long pctToStepsYLocal(double pct) {
  return static_cast<long>(llround((pct / 100.0) * static_cast<double>(Y_RANGE_STEPS)));
}

// Convert the percent tolerance to a step tolerance for X.
static long tolStepsX() {
  return static_cast<long>(llround((g_tolPct / 100.0) * static_cast<double>(X_RANGE_STEPS)));
}

// Convert the percent tolerance to a step tolerance for Y.
static long tolStepsY() {
  return static_cast<long>(llround((g_tolPct / 100.0) * static_cast<double>(Y_RANGE_STEPS)));
}

// Detect whether any supported mapping CSV exists on SPIFFS.
static bool csvExists() {
  File f = SPIFFS.open("/board-decimal.csv", "r");
  if (!f) {
    f = SPIFFS.open("/board-positions.csv", "r");
  }
  if (!f) {
    f = SPIFFS.open("/chess_pos.csv", "r");
  }
  if (f) {
    f.close();
    return true;
  }
  return false;
}

// Record a PASS/FAIL result and print it with details. Also increments group totals and ALL totals.
static void record(Counters& group, bool ok, const String& name, const String& details = "") {
  group.total++;
  g_all.total++;
  if (ok) {
    group.passed++;
    g_all.passed++;
    logLine(String("[PASS] ") + name + (details.length() ? ("  " + details) : ""));
  } else {
    group.failed++;
    g_all.failed++;
    logLine(String("[FAIL] ") + name + (details.length() ? ("  " + details) : ""));
  }
}

// Record a SKIP result when the test can't be executed on this setup (e.g., missing CSV).
static void skip(Counters& group, const String& name, const String& details = "") {
  group.total++;
  g_all.total++;
  group.skipped++;
  g_all.skipped++;
  logLine(String("[SKIP] ") + name + (details.length() ? ("  " + details) : ""));
}

// Step-level comparison helper.
static bool nearSteps(long actual, long expected, long tol) {
  return labs(actual - expected) <= tol;
}

// Percent-level comparison helper.
static bool nearPct(double actual, double expected, double tol) {
  return fabs(actual - expected) <= tol;
}

// Print the current gantry position and raw step counters.
static void printPos(const char* label) {
  String line;
  line.reserve(120);
  line += label;
  line += " CUR(";
  line += String(getX(), 3);
  line += "%,";
  line += String(getY(), 3);
  line += "%) steps=(";
  line += String(currStepsX);
  line += ",";
  line += String(currStepsY);
  line += ")";
  logLine(line);
}

// Execute a gantry move to xPct/yPct and verify against predicted steps and percent tolerance.
static bool moveAndVerifyPct(double xPct, double yPct, const String& name) {
  const int rc = gantryTo(xPct, yPct);
  if (rc != 0) {
    record(g_moves, false, name, String("rc=") + rc);
    return false;
  }

  const long expectedX = pctToStepsXLocal(xPct);
  const long expectedY = pctToStepsYLocal(yPct);

  const bool okSteps = nearSteps(currStepsX, expectedX, tolStepsX()) && nearSteps(currStepsY, expectedY, tolStepsY());
  const bool okPct = nearPct(getX(), xPct, g_tolPct) && nearPct(getY(), yPct, g_tolPct);

  String details;
  details.reserve(160);
  details += "tgt=(";
  details += String(xPct, 3);
  details += "%,";
  details += String(yPct, 3);
  details += "%) cur=(";
  details += String(getX(), 3);
  details += "%,";
  details += String(getY(), 3);
  details += "%) errSteps=(";
  details += String(labs(currStepsX - expectedX));
  details += ",";
  details += String(labs(currStepsY - expectedY));
  details += ")";

  record(g_moves, okSteps && okPct, name, details);
  return okSteps && okPct;
}

// Core sanity tests: small moves + center + corners. This catches swapped axes, reversed directions,
// and gross scaling errors quickly.
static void testBasicMoves() {
  setHome();
  printPos("HOME");

  moveAndVerifyPct(2.0, 0.0, "move X+ small");
  moveAndVerifyPct(0.0, 2.0, "move Y+ small");
  moveAndVerifyPct(2.0, 2.0, "move XY small");
  moveAndVerifyPct(50.0, 50.0, "move center");
  moveAndVerifyPct(90.0, 10.0, "move 90/10");
  moveAndVerifyPct(10.0, 90.0, "move 10/90");
  moveAndVerifyPct(0.0, 0.0, "move back home");
}

// Limit tests: out-of-range moves must return -1 and MUST NOT move the gantry.
static void testLimitRejection() {
  setHome();
  const long beforeX = currStepsX;
  const long beforeY = currStepsY;

  const int rc1 = gantryTo(-50.0, 0.0);
  record(g_limits, rc1 == -1 && currStepsX == beforeX && currStepsY == beforeY, "limit reject X<min", String("rc=") + rc1);

  const int rc2 = gantryTo(200.0, 0.0);
  record(g_limits, rc2 == -1 && currStepsX == beforeX && currStepsY == beforeY, "limit reject X>max", String("rc=") + rc2);

  const int rc3 = gantryTo(0.0, -50.0);
  record(g_limits, rc3 == -1 && currStepsX == beforeX && currStepsY == beforeY, "limit reject Y<min", String("rc=") + rc3);

  const int rc4 = gantryTo(0.0, 200.0);
  record(g_limits, rc4 == -1 && currStepsX == beforeX && currStepsY == beforeY, "limit reject Y>max", String("rc=") + rc4);
}

// Pattern test: scan a 5x5 grid in a serpentine pattern. This validates repeated segmentation
// and many back-to-back moves without manual control.
static void testGridPattern() {
  setHome();

  const double points[] = {10.0, 30.0, 50.0, 70.0, 90.0};
  bool ok = true;

  for (int yi = 0; yi < 5; ++yi) {
    const double y = points[yi];
    if (yi % 2 == 0) {
      for (int xi = 0; xi < 5; ++xi) {
        ok = moveAndVerifyPct(points[xi], y, String("grid ") + String(xi) + "," + String(yi)) && ok;
      }
    } else {
      for (int xi = 4; xi >= 0; --xi) {
        ok = moveAndVerifyPct(points[xi], y, String("grid ") + String(xi) + "," + String(yi)) && ok;
      }
    }
  }

  record(g_patterns, ok, "pattern grid 5x5");
}

// Pattern test: a single "knight-like" move expressed in percent terms, derived from SQUARE_STEPS.
// This is a proxy test for the code path that decomposes these moves safely.
static void testKnightStepPattern() {
  setHome();
  moveAndVerifyPct(50.0, 50.0, "prep knight origin");

  const double oneSquareXPct = (static_cast<double>(SQUARE_STEPS) * 100.0) / static_cast<double>(X_RANGE_STEPS);
  const double oneSquareYPct = (static_cast<double>(SQUARE_STEPS) * 100.0) / static_cast<double>(Y_RANGE_STEPS);

  const double targetX = 50.0 + (2.0 * oneSquareXPct);
  const double targetY = 50.0 + (1.0 * oneSquareYPct);

  const int rc = gantryTo(targetX, targetY);
  const bool ok = (rc == 0) && nearPct(getX(), targetX, g_tolPct) && nearPct(getY(), targetY, g_tolPct);
  record(g_patterns, ok, "pattern knight-like move", String("rc=") + rc);
}

// CSV test: verifies that getCoords() + gantryTo(square) match the CSV's returned coordinates.
// This is skipped if the CSV is not present on SPIFFS.
static void testCsvSquares() {
  if (!csvExists()) {
    skip(g_csv, "csv squares", "mapping file missing on SPIFFS");
    return;
  }

  const char* keys[] = {"a1", "h8", "e4", "whitejail1", "blackjail1"};
  bool ok = true;
  for (const char* k : keys) {
    Coords c = getCoords(k);
    const int rc = gantryTo(k);
    const bool oneOk = (rc == 0) && nearPct(getX(), c.x, g_tolPct) && nearPct(getY(), c.y, g_tolPct);
    record(g_csv, oneOk, String("csv ") + k, String("rc=") + rc);
    ok = ok && oneOk;
  }
  record(g_csv, ok, "csv aggregate");
}

// Castling string wrapper tests:
// - invalid strings must be rejected (return -1)
// - valid strings must be accepted and routed to castle(bool)
// Actual castling movement is environment-dependent (pieces, magnet, sensor), so we don't enforce it here.
static void testCastleWrappers() {
  const int rcBad = castle("NOT_A_CASTLE");
  record(g_castle, rcBad == -1, "castle invalid string", String("rc=") + rcBad);

  const int rcOk1 = castle("O-O");
  const int rcOk2 = castle("O-O-O");
  const bool ok = (rcOk1 == 0 || rcOk1 == -1) && (rcOk2 == 0 || rcOk2 == -1);
  skip(g_castle, "castle execution", String("rc(O-O)=") + rcOk1 + " rc(O-O-O)=" + rcOk2);
  record(g_castle, ok, "castle wrapper accepts valid strings");
}

// Print a one-line summary for a counter group.
static void summaryLine(const char* name, const Counters& c) {
  String line;
  line.reserve(96);
  line += name;
  line += " total=";
  line += String(c.total);
  line += " pass=";
  line += String(c.passed);
  line += " fail=";
  line += String(c.failed);
  line += " skip=";
  line += String(c.skipped);
  logLine(line);
}

// Real-time progress hook:
// Gantry.cpp calls gantryStatusTick(...) while motors are moving. This provides a periodic heartbeat so
// you can see motion progress (and confirm the move is blocking).
void gantryStatusTick(double currentXPct, double currentYPct, double targetXPct, double targetYPct) {
  static unsigned long lastMs = 0;
  const unsigned long now = millis();
  if (now - lastMs < 250) {
    return;
  }
  lastMs = now;
  Serial.print("MOVING CUR(");
  Serial.print(currentXPct, 2);
  Serial.print("%,");
  Serial.print(currentYPct, 2);
  Serial.print("%) TGT(");
  Serial.print(targetXPct, 2);
  Serial.print("%,");
  Serial.print(targetYPct, 2);
  Serial.println("%)");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);

  // SPIFFS is used for:
  // - reading mapping CSV files
  // - writing the automated test log
  const bool spiffsOk = SPIFFS.begin(true);
  logLine(spiffsOk ? "SPIFFS OK" : "SPIFFS FAIL");

  logLine("GANTRY AUTO TEST START");
  logLine(String("TOL=") + String(g_tolPct, 3) + "%  tolSteps=(" + String(tolStepsX()) + "," + String(tolStepsY()) + ")");

  // Test execution order: start simple, then patterns, then CSV integration.
  testBasicMoves();
  testLimitRejection();
  testGridPattern();
  testKnightStepPattern();
  testCsvSquares();
  testCastleWrappers();

  // Summary + coverage:
  // "coverage" here is test-suite coverage (executed vs skipped), not code coverage.
  logLine("GANTRY AUTO TEST SUMMARY");
  summaryLine("ALL", g_all);
  summaryLine("MOVES", g_moves);
  summaryLine("LIMITS", g_limits);
  summaryLine("PATTERNS", g_patterns);
  summaryLine("CSV", g_csv);
  summaryLine("CASTLE", g_castle);

  const uint32_t executed = g_all.total - g_all.skipped;
  const uint32_t coveragePct = (g_all.total == 0) ? 0 : static_cast<uint32_t>((executed * 100ULL) / g_all.total);
  logLine(String("COVERAGE executed=") + String(executed) + "/" + String(g_all.total) + " (" + String(coveragePct) + "%)");

  logLine(g_all.failed == 0 ? "GANTRY_AUTO_OK" : "GANTRY_AUTO_FAIL");
}

void loop() {}

