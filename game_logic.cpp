#include "game_logic.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace chess {

namespace {

const int KNIGHT_D[8][2] = {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
                            {1, -2},  {1, 2},  {2, -1},  {2, 1}};
const int KING_D[8][2]   = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                            {0, 1},   {1, -1}, {1, 0},  {1, 1}};
const int ROOK_D[4][2]   = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
const int BISHOP_D[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};

inline bool inBounds(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }
inline int ci(Color c) { return c == Color::White ? 0 : 1; }
inline int homeRow(Color c) { return c == Color::White ? 7 : 0; }
inline int pawnDir(Color c) { return c == Color::White ? -1 : 1; }

} // namespace

game_logic::game_logic()
{
    reset();
}

void game_logic::reset()
{
    loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

bool game_logic::loadFen(const std::string &fen)
{
    State s;
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
            s.castle[i][j] = false;

    std::istringstream in(fen);
    std::string placement, active, castling = "-", ep = "-";
    int half = 0, full = 1;
    if (!(in >> placement >> active))
        return false;
    in >> castling >> ep >> half >> full;

    int r = 0, c = 0;
    for (char ch : placement) {
        if (ch == '/') {
            if (c != 8) return false;
            ++r;
            c = 0;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            c += ch - '0';
            continue;
        }
        if (r > 7 || c > 7) return false;
        Piece p;
        p.color = std::isupper(static_cast<unsigned char>(ch)) ? Color::White : Color::Black;
        switch (std::tolower(static_cast<unsigned char>(ch))) {
        case 'p': p.type = PieceType::Pawn;   break;
        case 'n': p.type = PieceType::Knight; break;
        case 'b': p.type = PieceType::Bishop; break;
        case 'r': p.type = PieceType::Rook;   break;
        case 'q': p.type = PieceType::Queen;  break;
        case 'k': p.type = PieceType::King;   break;
        default: return false;
        }
        s.board[r][c++] = p;
    }
    if (r != 7 || c != 8) return false;

    if (active == "w") s.turn = Color::White;
    else if (active == "b") s.turn = Color::Black;
    else return false;

    for (char ch : castling) {
        if (ch == 'K') s.castle[0][0] = true;
        else if (ch == 'Q') s.castle[0][1] = true;
        else if (ch == 'k') s.castle[1][0] = true;
        else if (ch == 'q') s.castle[1][1] = true;
    }

    if (ep.size() == 2 && ep[0] >= 'a' && ep[0] <= 'h' && ep[1] >= '1' && ep[1] <= '8') {
        s.epCol = ep[0] - 'a';
        s.epRow = 8 - (ep[1] - '0');
    }

    s.halfmove = half;
    s.fullmove = full;

    state_ = s;
    hasLast_ = false;
    lastMove_ = Move();
    positionCounts_.clear();
    positionCounts_[positionKey(state_)] = 1;
    status_ = GameStatus::Ongoing;
    updateStatus();
    return true;
}

Piece game_logic::pieceAt(int row, int col) const
{
    if (!inBounds(row, col))
        return Piece();
    return state_.board[row][col];
}

bool game_logic::isGameOver() const
{
    return status_ != GameStatus::Ongoing && status_ != GameStatus::Check;
}

bool game_logic::inCheck() const
{
    return inCheckState(state_, state_.turn);
}

bool game_logic::findKing(Color color, int &row, int &col) const
{
    return findKing(state_, color, row, col);
}

// ---------------------------------------------------------------------------
// Attack detection
// ---------------------------------------------------------------------------

bool game_logic::isAttacked(const State &s, int r, int c, Color by)
{
    // Pawns: a pawn of colour `by` on (r - dir, c +- 1) attacks (r, c).
    const int pr = r - pawnDir(by);
    for (int dc = -1; dc <= 1; dc += 2) {
        const int pc = c + dc;
        if (inBounds(pr, pc)) {
            const Piece &p = s.board[pr][pc];
            if (p.type == PieceType::Pawn && p.color == by)
                return true;
        }
    }

    for (const auto &d : KNIGHT_D) {
        const int nr = r + d[0], nc = c + d[1];
        if (inBounds(nr, nc)) {
            const Piece &p = s.board[nr][nc];
            if (p.type == PieceType::Knight && p.color == by)
                return true;
        }
    }

    for (const auto &d : KING_D) {
        const int nr = r + d[0], nc = c + d[1];
        if (inBounds(nr, nc)) {
            const Piece &p = s.board[nr][nc];
            if (p.type == PieceType::King && p.color == by)
                return true;
        }
    }

    for (const auto &d : ROOK_D) {
        int nr = r + d[0], nc = c + d[1];
        while (inBounds(nr, nc)) {
            const Piece &p = s.board[nr][nc];
            if (!p.empty()) {
                if (p.color == by && (p.type == PieceType::Rook || p.type == PieceType::Queen))
                    return true;
                break;
            }
            nr += d[0];
            nc += d[1];
        }
    }

    for (const auto &d : BISHOP_D) {
        int nr = r + d[0], nc = c + d[1];
        while (inBounds(nr, nc)) {
            const Piece &p = s.board[nr][nc];
            if (!p.empty()) {
                if (p.color == by && (p.type == PieceType::Bishop || p.type == PieceType::Queen))
                    return true;
                break;
            }
            nr += d[0];
            nc += d[1];
        }
    }
    return false;
}

bool game_logic::findKing(const State &s, Color color, int &row, int &col)
{
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c)
            if (s.board[r][c].type == PieceType::King && s.board[r][c].color == color) {
                row = r;
                col = c;
                return true;
            }
    return false;
}

bool game_logic::inCheckState(const State &s, Color color)
{
    int kr, kc;
    if (!findKing(s, color, kr, kc))
        return false;
    return isAttacked(s, kr, kc, opposite(color));
}

// ---------------------------------------------------------------------------
// Move generation
// ---------------------------------------------------------------------------

void game_logic::pseudoMoves(const State &s, int r, int c, std::vector<Move> &out)
{
    const Piece p = s.board[r][c];
    if (p.empty())
        return;

    auto add = [&](int tr, int tc) {
        out.push_back(Move{r, c, tr, tc, PieceType::NoPiece});
    };

    auto slide = [&](const int (*dirs)[2], int n) {
        for (int i = 0; i < n; ++i) {
            int nr = r + dirs[i][0], nc = c + dirs[i][1];
            while (inBounds(nr, nc)) {
                const Piece &t = s.board[nr][nc];
                if (t.empty()) {
                    add(nr, nc);
                } else {
                    if (t.color != p.color) add(nr, nc);
                    break;
                }
                nr += dirs[i][0];
                nc += dirs[i][1];
            }
        }
    };

    switch (p.type) {
    case PieceType::Pawn: {
        const int dir = pawnDir(p.color);
        const int startRow = (p.color == Color::White) ? 6 : 1;
        const int promoRow = (p.color == Color::White) ? 0 : 7;

        auto addPawn = [&](int tr, int tc) {
            if (tr == promoRow) {
                const PieceType promos[4] = {PieceType::Queen, PieceType::Rook,
                                             PieceType::Bishop, PieceType::Knight};
                for (PieceType pt : promos)
                    out.push_back(Move{r, c, tr, tc, pt});
            } else {
                out.push_back(Move{r, c, tr, tc, PieceType::NoPiece});
            }
        };

        const int tr = r + dir;
        if (inBounds(tr, c) && s.board[tr][c].empty()) {
            addPawn(tr, c);
            if (r == startRow && s.board[r + 2 * dir][c].empty())
                out.push_back(Move{r, c, r + 2 * dir, c, PieceType::NoPiece});
        }
        for (int dc = -1; dc <= 1; dc += 2) {
            const int tc = c + dc;
            if (!inBounds(tr, tc)) continue;
            const Piece &t = s.board[tr][tc];
            if (!t.empty() && t.color != p.color)
                addPawn(tr, tc);
            else if (t.empty() && tr == s.epRow && tc == s.epCol)
                addPawn(tr, tc); // en passant
        }
        break;
    }
    case PieceType::Knight:
        for (const auto &d : KNIGHT_D) {
            const int nr = r + d[0], nc = c + d[1];
            if (inBounds(nr, nc) && (s.board[nr][nc].empty() || s.board[nr][nc].color != p.color))
                add(nr, nc);
        }
        break;
    case PieceType::Bishop:
        slide(BISHOP_D, 4);
        break;
    case PieceType::Rook:
        slide(ROOK_D, 4);
        break;
    case PieceType::Queen:
        slide(ROOK_D, 4);
        slide(BISHOP_D, 4);
        break;
    case PieceType::King: {
        for (const auto &d : KING_D) {
            const int nr = r + d[0], nc = c + d[1];
            if (inBounds(nr, nc) && (s.board[nr][nc].empty() || s.board[nr][nc].color != p.color))
                add(nr, nc);
        }
        // Castling
        const Color enemy = opposite(p.color);
        if (r == homeRow(p.color) && c == 4 && !isAttacked(s, r, c, enemy)) {
            auto ownRook = [&](int col) {
                return s.board[r][col].type == PieceType::Rook && s.board[r][col].color == p.color;
            };
            if (s.castle[ci(p.color)][0] && s.board[r][5].empty() && s.board[r][6].empty()
                && ownRook(7) && !isAttacked(s, r, 5, enemy) && !isAttacked(s, r, 6, enemy))
                add(r, 6);
            if (s.castle[ci(p.color)][1] && s.board[r][1].empty() && s.board[r][2].empty()
                && s.board[r][3].empty() && ownRook(0)
                && !isAttacked(s, r, 3, enemy) && !isAttacked(s, r, 2, enemy))
                add(r, 2);
        }
        break;
    }
    case PieceType::NoPiece:
        break;
    }
}

void game_logic::applyMove(State &s, const Move &m)
{
    Piece p = s.board[m.fromRow][m.fromCol];
    const Piece target = s.board[m.toRow][m.toCol];
    const bool wasPawn = (p.type == PieceType::Pawn);
    bool capture = !target.empty();

    // En passant: a pawn moves diagonally onto an empty square.
    if (wasPawn && m.fromCol != m.toCol && target.empty()) {
        s.board[m.fromRow][m.toCol] = Piece();
        capture = true;
    }

    s.board[m.fromRow][m.fromCol] = Piece();

    // Promotion
    if (wasPawn && (m.toRow == 0 || m.toRow == 7))
        p.type = (m.promotion == PieceType::NoPiece) ? PieceType::Queen : m.promotion;

    s.board[m.toRow][m.toCol] = p;

    // Castling: also move the rook.
    if (p.type == PieceType::King && std::abs(m.toCol - m.fromCol) == 2) {
        if (m.toCol > m.fromCol) {
            s.board[m.toRow][5] = s.board[m.toRow][7];
            s.board[m.toRow][7] = Piece();
        } else {
            s.board[m.toRow][3] = s.board[m.toRow][0];
            s.board[m.toRow][0] = Piece();
        }
    }

    // Castling rights are lost when a king/rook moves or a rook is captured.
    auto touchCorner = [&](int r, int c) {
        if (r == 7 && c == 7) s.castle[0][0] = false;
        else if (r == 7 && c == 0) s.castle[0][1] = false;
        else if (r == 0 && c == 7) s.castle[1][0] = false;
        else if (r == 0 && c == 0) s.castle[1][1] = false;
    };
    touchCorner(m.fromRow, m.fromCol);
    touchCorner(m.toRow, m.toCol);
    if (p.type == PieceType::King) {
        s.castle[ci(p.color)][0] = false;
        s.castle[ci(p.color)][1] = false;
    }

    // En passant target for the next move.
    s.epRow = s.epCol = -1;
    if (wasPawn && std::abs(m.toRow - m.fromRow) == 2) {
        s.epRow = (m.fromRow + m.toRow) / 2;
        s.epCol = m.fromCol;
    }

    // Clocks and turn.
    if (wasPawn || capture) s.halfmove = 0;
    else ++s.halfmove;
    if (s.turn == Color::Black) ++s.fullmove;
    s.turn = opposite(s.turn);
}

bool game_logic::leavesKingSafe(const State &s, const Move &m)
{
    const Color mover = s.board[m.fromRow][m.fromCol].color;
    State t = s;
    applyMove(t, m);
    return !inCheckState(t, mover);
}

std::vector<Move> game_logic::legalFrom(const State &s, int r, int c)
{
    std::vector<Move> legal;
    if (!inBounds(r, c))
        return legal;
    const Piece &p = s.board[r][c];
    if (p.empty() || p.color != s.turn)
        return legal;

    std::vector<Move> pseudo;
    pseudoMoves(s, r, c, pseudo);
    for (const Move &m : pseudo)
        if (leavesKingSafe(s, m))
            legal.push_back(m);
    return legal;
}

bool game_logic::hasAnyLegalMove(const State &s)
{
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c)
            if (!s.board[r][c].empty() && s.board[r][c].color == s.turn
                && !legalFrom(s, r, c).empty())
                return true;
    return false;
}

std::vector<Move> game_logic::legalMovesFrom(int row, int col) const
{
    if (isGameOver())
        return std::vector<Move>();
    return legalFrom(state_, row, col);
}

std::vector<Move> game_logic::allLegalMoves() const
{
    std::vector<Move> all;
    if (isGameOver())
        return all;
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c) {
            std::vector<Move> m = legalFrom(state_, r, c);
            all.insert(all.end(), m.begin(), m.end());
        }
    return all;
}

bool game_logic::needsPromotion(int fr, int fc, int tr, int tc) const
{
    for (const Move &m : legalMovesFrom(fr, fc))
        if (m.toRow == tr && m.toCol == tc && m.promotion != PieceType::NoPiece)
            return true;
    return false;
}

bool game_logic::makeMove(const Move &m)
{
    if (isGameOver())
        return false;
    if (!inBounds(m.fromRow, m.fromCol) || !inBounds(m.toRow, m.toCol))
        return false;

    const PieceType wanted =
        (m.promotion == PieceType::NoPiece) ? PieceType::Queen : m.promotion;

    bool found = false;
    Move chosen;
    for (const Move &lm : legalFrom(state_, m.fromRow, m.fromCol)) {
        if (lm.toRow != m.toRow || lm.toCol != m.toCol)
            continue;
        if (lm.promotion == PieceType::NoPiece || lm.promotion == wanted) {
            chosen = lm;
            found = true;
            break;
        }
    }
    if (!found)
        return false;

    applyMove(state_, chosen);
    lastMove_ = chosen;
    hasLast_ = true;
    ++positionCounts_[positionKey(state_)];
    updateStatus();
    return true;
}

// ---------------------------------------------------------------------------
// Game status
// ---------------------------------------------------------------------------

bool game_logic::insufficientMaterial(const State &s)
{
    int knights = 0, bishops = 0;
    int bishopSquareColor[2] = {0, 0};
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c) {
            const Piece &p = s.board[r][c];
            switch (p.type) {
            case PieceType::Pawn:
            case PieceType::Rook:
            case PieceType::Queen:
                return false;
            case PieceType::Knight:
                ++knights;
                break;
            case PieceType::Bishop:
                ++bishops;
                ++bishopSquareColor[(r + c) % 2];
                break;
            default:
                break;
            }
        }
    if (knights + bishops <= 1)
        return true;                                   // K v K, K+minor v K
    if (knights == 0 && (bishopSquareColor[0] == 0 || bishopSquareColor[1] == 0))
        return true;                                   // only same-coloured bishops
    return false;
}

std::string game_logic::positionKey(const State &s)
{
    static const char letters[] = "?PNBRQK";
    std::string k;
    k.reserve(80);
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c) {
            const Piece &p = s.board[r][c];
            if (p.empty()) {
                k += '.';
            } else {
                char ch = letters[static_cast<int>(p.type)];
                if (p.color == Color::Black)
                    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                k += ch;
            }
        }
    k += (s.turn == Color::White) ? 'w' : 'b';
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
            k += s.castle[i][j] ? '1' : '0';
    k += static_cast<char>('0' + (s.epCol + 1));
    return k;
}

void game_logic::updateStatus()
{
    const bool check = inCheckState(state_, state_.turn);
    const auto it = positionCounts_.find(positionKey(state_));
    const int repeats = (it == positionCounts_.end()) ? 0 : it->second;

    if (!hasAnyLegalMove(state_))
        status_ = check ? GameStatus::Checkmate : GameStatus::Stalemate;
    else if (insufficientMaterial(state_))
        status_ = GameStatus::DrawInsufficientMaterial;
    else if (repeats >= 3)
        status_ = GameStatus::DrawRepetition;
    else if (state_.halfmove >= 100)
        status_ = GameStatus::DrawFiftyMove;
    else
        status_ = check ? GameStatus::Check : GameStatus::Ongoing;
}

} // namespace chess
