String move;
String startSquare;
String endSquare;

void processMove(String moveStr) {

    // Store move globally if your helper functions use it
    move = moveStr;

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
    gantryTo(start);

    // Grab piece
    magnetOn();

    delay(200);

    // Ensure magnet contacts piece
    sweepTile();

    // Move to destination
    gantryTo(end);

    // Drop piece
    magnetOff();

    delay(200);

    Serial.println("Piece moved");
}



//  Capture Logic Skeleton
void capturePiece(String square) {
    Serial.print("Capturing piece at ");
    Serial.println(square);

    gantryTo(square);
    magnetOn();
    sweepTile();

    if (isWhitePiece(square)) {
        gantryToWhiteJail();
        whiteJail++;
    }
    else {
        gantryToBlackJail();
        blackJail++;
    }

    magnetOff(); 
}

//  check special move Skeleton

void checkMove(move) {

    // 1. Check for castling
    if (isCastle(move)) {

        castle(move);
        return;
    }

    // 2. Check for capture
    if (isCapture(move)) {

        capturePiece(endSquare);
        return;
    }
    
    // 3. Check for promotion
    if (isPromotion(move)) {
        
        Promotion();
        return;
    }
    
    // 4. regular move
     movePiece(startSquare, endSquare);

    Serial.println("Move completed");
}
