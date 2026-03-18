String move;
String startSquare;
String endSquare;
bool isPromotion;

void processMove(String moveStr) {

    // Store move globally if your helper functions use it
    move = moveStr;

    // Checks if move is a promotion
    if (move.length() == 5) {
        isPromotion = true;
    }
    else {
        isPromotion = false;
    }

    // Extract squares from UCI move format
    // Example: "e2e4"
    String startSquare = moveStr.substring(0, 2);
    String endSquare   = moveStr.substring(2, 4);

    // Check what if special move and execute
    checkMove(move);
}


// Move Logic Skeleton
void movePiece(String start, String end) {

    // Move to starting square
    gantryTo(startSquare);

    // Grab piece
    magnetOn();

    delay(200);

    // Ensure magnet contacts piece
    sweepTile();

    // Move to destination
    gantryTo(endSquare);

    // Drop piece
    magnetOff();

    delay(200);

    Serial.println("Piece moved");
}

//  check special move Skeleton
void checkMove(move) {

    // 1. Check for castling
    if (isCastle(move)) {

        castle(move);
        return;
    }

    // 2. Check for capture
    else if (isCapture(move)) {

        capturePiece(endSquare);
        movePiece(startSquare, endSquare);
        return;
    }
    
    // 3. Check for promotion
    if (isPromotion()) {
        movePiece(startSquare, endSquare);
        char promotionTo = move[4];
        Promote(endSquare, promotionTo);
        return;
    }
    
    // 4. regular move
    else {
        movePiece(startSquare, endSquare);
    }

    Serial.println("Move completed");
}


//  Capture Logic Skeleton
void capturePiece(String square) {
    Serial.print("Capturing piece at ");
    Serial.println(square);

    gantryTo(endSquare);
    magnetOn();
    sweepTile();

    if (isWhitePiece(endSquare)) {
        gantryToWhiteJail();
        whiteJail++;
    }
    else {
        gantryToBlackJail();
        blackJail++;
    }

    magnetOff(); 
}

void castle(String move) {

    // White king side
    if (move == "e1g1") {

        Serial.println("White kingside castle");

        // Move king
        movePiece("e1", "g1");

        // Move rook
        movePiece("h1", "f1");
    }

    // White queen side
    else if (move == "e1c1") {

        Serial.println("White queenside castle");

        movePiece("e1", "c1");
        movePiece("a1", "d1");
    }

    // Black king side
    else if (move == "e8g8") {

        Serial.println("Black kingside castle");

        movePiece("e8", "g8");
        movePiece("h8", "f8");
    }

    // Black queen side
    else if (move == "e8c8") {

        Serial.println("Black queenside castle");

        movePiece("e8", "c8");
        movePiece("a8", "d8");
    }

    Serial.println("Castling completed");
}

void promote(String square, char promotionTo) {

    Serial.print("Promotion at ");
    Serial.println(square);

    Serial.print("Promoting to: ");
    Serial.println(promotionTo);

    // Remove pawn
    gantryTo(square);
    magnetOn();
    sweepTile();

    // Move pawn to graveyard
    gantryToCapturedPieces();
    magnetOff();

    // Retrieve promoted piece
    switch (promotionTo) {

        case 'q':
            Serial.println("Getting Queen");
            gantryToQueenStorage();
            break;

        case 'r':
            Serial.println("Getting Rook");
            gantryToRookStorage();
            break;

        case 'b':
            Serial.println("Getting Bishop");
            gantryToBishopStorage();
            break;

        case 'n':
            Serial.println("Getting Knight");
            gantryToKnightStorage();
            break;
    }

    // Pick up new piece
    magnetOn();
    sweepTile();

    // Place promoted piece
    gantryTo(square);
    magnetOff();

    Serial.println("Promotion complete");
}