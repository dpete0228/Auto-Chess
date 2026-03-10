import time
import chess
import chess.pgn
import serial
import os

# ==============================
# CONFIG
# ==============================
PGN_PATH = r" Path of file that stock fish saves moves to"
SERIAL_PORT = "COM4" # Port ESP32 is connected to
BAUD_RATE = 115200
CHECK_INTERVAL = 1.0
ACK_TIMEOUT = 5
# ==============================


def get_game_data(path):
    """Return (moves_list, result_string)."""
    moves = []
    result = "*"

    if not os.path.exists(path):
        return moves, result

    with open(path) as pgn:
        game = chess.pgn.read_game(pgn)
        if game is None:
            return moves, result

        result = game.headers.get("Result", "*")

        board = game.board()
        for move in game.mainline_moves():
            moves.append(move.uci())
            board.push(move)

    return moves, result


def wait_for_ack(ser, expected_move):
    start = time.time()

    while time.time() - start < ACK_TIMEOUT:
        if ser.in_waiting:
            line = ser.readline().decode(errors="ignore").strip()
            print("ESP32:", line)

            if line == f"ACK {expected_move}":
                return True

    return False


# ------------------------------
# Helper Functions
# ------------------------------

def piece_name(piece):
    """Return human readable piece description."""
    piece_types = {
        chess.PAWN: "Pawn",
        chess.KNIGHT: "Knight",
        chess.BISHOP: "Bishop",
        chess.ROOK: "Rook",
        chess.QUEEN: "Queen",
        chess.KING: "King"
    }

    color = "White" if piece.color else "Black"
    return f"{color} {piece_types[piece.piece_type]}"


def print_board_matrix(board):
    """Print board as 8x8 matrix."""
    print("\nBoard Matrix:")
    matrix = []

    for rank in range(7, -1, -1):
        row = []
        for file in range(8):
            square = chess.square(file, rank)
            piece = board.piece_at(square)

            if piece:
                row.append(piece.symbol())
            else:
                row.append('.')

        matrix.append(row)
        print(row)

    return matrix


# ------------------------------
# MAIN PROGRAM
# ------------------------------

def main():

    print("Opening serial...")
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    time.sleep(2)
    ser.reset_input_buffer()

    sent_move_count = 0

    # Create persistent board state
    board = chess.Board()

    print("Watching PGN for new moves...")

    while True:
        try:
            moves, result = get_game_data(PGN_PATH)

            # If new moves appeared
            while sent_move_count < len(moves):

                move_str = moves[sent_move_count]
                move = chess.Move.from_uci(move_str)

                print("\n============================")
                print("Sending:", move_str)

                # Determine moving piece
                piece = board.piece_at(move.from_square)

                if piece:
                    print("Moving piece:", piece_name(piece))

                # Check capture
                if board.is_capture(move):
                    captured = board.piece_at(move.to_square)
                    if captured:
                        print("Capture:", piece_name(captured))

                # Check promotion
                if move.promotion:
                    promotion_piece = chess.piece_symbol(move.promotion).upper()
                    print("Promotion to:", promotion_piece)

                # Send move to ESP32
                ser.write((move_str + "\n").encode())

                if wait_for_ack(ser, move_str):

                    print("Confirmed:", move_str)

                    # Update board state
                    board.push(move)

                    # Print human readable board
                    print("\nHuman Board View:")
                    print(board)

                    # Print matrix representation
                    print_board_matrix(board)

                    # Print FEN (robot friendly)
                    print("\nFEN String:")
                    print(board.fen())

                    sent_move_count += 1

                else:
                    print("⚠ ACK TIMEOUT — resending")

            if result != "*":
                print(f"\nGame finished! Result: {result}")
                ser.write((f"GAME_OVER {result}\n").encode())
                break

            time.sleep(CHECK_INTERVAL)

        except KeyboardInterrupt:
            print("Exiting.")
            break


if __name__ == "__main__":
    main()