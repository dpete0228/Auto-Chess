String move;

#define MAX_QUEUE 20

String moveQueue[MAX_QUEUE];
int queueHead = 0;
int queueTail = 0;
int queueCount = 0;

bool enqueueMove(String move) {
  if (queueCount >= MAX_QUEUE) {
    Serial.println("Queue Full!");
    return false;
  }

  moveQueue[queueTail] = move;
  queueTail = (queueTail + 1) % MAX_QUEUE;
  queueCount++;
  return true;
}

bool dequeueMove(String &move) {
  if (queueCount == 0) {
    return false;
  }

  move = moveQueue[queueHead];
  queueHead = (queueHead + 1) % MAX_QUEUE;
  queueCount--;
  return true;
}

void setup() {
  Serial.begin(115200);
}


String inputBuffer = "";

void loop() {

  // Read serial characters
  while (Serial.available()) {

    char c = Serial.read();

    if (c == '\n') {

      inputBuffer.trim();

      if (inputBuffer.length() > 0) {

        // Send acknowledgement
        Serial.print("ACK ");
        Serial.println(inputBuffer);

        // Store in queue
        if (!enqueueMove(inputBuffer)) {
          Serial.println("ERROR: Enqueue Failed");
        }
      }

      inputBuffer = ""; // clear buffer
    }
    else {
      inputBuffer += c;
    }
  }

  // Process queued moves
  if (queueCount > 0) {

    String nextMove;

    if (dequeueMove(nextMove)) {

      Serial.print("Processing move: ");
      Serial.println(nextMove);

      delay(1000); // simulate motors/ processMove()

      Serial.print("Finished move: ");
      Serial.println(nextMove);
    }
  }
}