const int magnetPin = 27; 

// --- PWM Settings ---
const int pwmChannel = 0;
const int freq = 20000;
const int resolution = 8;

void setup() {
  Serial.begin(115200);
  
  // Setup PWM
  ledcSetup(pwmChannel, freq, resolution);
  ledcAttachPin(magnetPin, pwmChannel);

  Serial.println("--- MAGNET TEST STARTING ---");
  Serial.println("Blinking Magnet every 2 seconds on GPIO 26");
}

void loop() {
  // 1. TURN ON (Full Power Kickstart then Hold)
  Serial.println("Magnet ON...");
  ledcWrite(pwmChannel, 255); // Full 12V
  delay(200);
  ledcWrite(pwmChannel, 160); // Holding Power
  delay(2000);

  // 2. TURN OFF
  Serial.println("Magnet OFF...");
  ledcWrite(pwmChannel, 0);
  delay(2000);
}