#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

// Pure C++ chess rules engine. It knows nothing about Qt, so it can be tested
// on its own and shared by the GUI and the network layer.
//
// Coordinates: row 0 is rank 8 (black's back rank), row 7 is rank 1.
//              col 0 is file A, col 7 is file H.

#include <map>
#include <string>
#include <vector>

namespace chess {

enum class Color { White, Black };

inline Color opposite(Color c)
{
    return c == Color::White ? Color::Black : Color::White;
}

enum class PieceType { NoPiece, Pawn, Knight, Bishop, Rook, Queen, King };

struct Piece {
    PieceType type = PieceType::NoPiece;
    Color color = Color::White;
    bool empty() const { return type == PieceType::NoPiece; }
};

struct Move {
    int fromRow = 0;
    int fromCol = 0;
    int toRow = 0;
    int toCol = 0;
    PieceType promotion = PieceType::NoPiece; // only used for pawn promotion
};

enum class GameStatus {
    Ongoing,
    Check,
    Checkmate,
    Stalemate,
    DrawFiftyMove,
    DrawInsufficientMaterial,
    DrawRepetition
};

class chess_bot;

class game_logic
{
public:
    game_logic();

    void reset();                           // standard start position
    bool loadFen(const std::string &fen);   // returns false on malformed FEN

    // --- queries -----------------------------------------------------------
    Piece pieceAt(int row, int col) const;
    Color turn() const { return state_.turn; }
    GameStatus status() const { return status_; }
    bool isGameOver() const;
    bool inCheck() const;                   // is the side to move in check?
    bool findKing(Color color, int &row, int &col) const;
    bool hasLastMove() const { return hasLast_; }
    const Move &lastMove() const { return lastMove_; }
    int halfmoveClock() const { return state_.halfmove; }

    // How many pieces of this colour and type have been captured so far
    // (a captured promoted piece counts as the piece it had become).
    int capturedCount(Color victimColor, PieceType type) const;

    // --- move history and undo -------------------------------------------
    int moveCount() const { return static_cast<int>(history_.size()); }   // half-moves played
    std::string sanAt(int ply) const;       // standard notation: "e4", "Nf3", "exd5", "O-O", "e8=Q+", "Qxf7#"
    bool canUndo() const { return !history_.empty(); }
    bool undoMove();                        // takes back the last half-move

    // Legal moves of the piece on (row, col). Empty if it is not that side's
    // turn, the square is empty, or the game is over. A pawn move to the last
    // rank is returned 4 times (one per promotion piece).
    std::vector<Move> legalMovesFrom(int row, int col) const;
    std::vector<Move> allLegalMoves() const;

    // True if moving (from -> to) would be a promotion (so the UI must ask).
    bool needsPromotion(int fromRow, int fromCol, int toRow, int toCol) const;

    // Validates and plays the move (castling, en passant and promotion
    // included). Returns false and changes nothing if it is not legal.
    // A promotion move with promotion == NoPiece promotes to a queen.
    bool makeMove(const Move &move);

private:
    friend class chess_bot;   // the computer player searches on raw States

    struct State {
        Piece board[8][8];
        Color turn = Color::White;
        bool castle[2][2] = {{true, true}, {true, true}}; // [color][0=king side,1=queen side]
        int epRow = -1;      // en passant target square (the square "behind" the pawn)
        int epCol = -1;
        int halfmove = 0;    // half-moves since last capture or pawn move
        int fullmove = 1;
    };

    // Everything needed to take one half-move back.
    struct HistoryEntry {
        State before;
        int captured[2][7] = {};
        Move lastMove;
        bool hadLast = false;
        std::string san;
    };

    // Static helpers work on an explicit State so they can try moves on copies.
    static bool isAttacked(const State &s, int row, int col, Color by);
    static bool findKing(const State &s, Color color, int &row, int &col);
    static bool inCheckState(const State &s, Color color);
    static void pseudoMoves(const State &s, int row, int col, std::vector<Move> &out);
    static std::vector<Move> legalFrom(const State &s, int row, int col);
    static bool leavesKingSafe(const State &s, const Move &m);
    static bool hasAnyLegalMove(const State &s);
    static void applyMove(State &s, const Move &m);
    static bool insufficientMaterial(const State &s);
    static std::string positionKey(const State &s);
    static std::string sanBase(const State &before, const Move &m);   // notation without + or #

    void updateStatus();

    State state_;
    GameStatus status_ = GameStatus::Ongoing;
    Move lastMove_;
    bool hasLast_ = false;
    int captured_[2][7] = {};   // [colour of the captured piece][PieceType]
    std::vector<HistoryEntry> history_;
    std::map<std::string, int> positionCounts_;
};

} // namespace chess

#endif // GAME_LOGIC_H
