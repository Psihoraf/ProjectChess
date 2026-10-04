#include "chess_bot.h"

#include <algorithm>

namespace chess {

namespace {

const int MATE = 30000;
const int INF = 32000;

// Indexed by PieceType: NoPiece, Pawn, Knight, Bishop, Rook, Queen, King
const int VALUE[7] = {0, 100, 320, 330, 500, 900, 0};

// Piece-square tables from White's point of view, row 0 = rank 8 (same as the board).
const int PST_PAWN[64] = {
      0,  0,  0,  0,  0,  0,  0,  0,
     50, 50, 50, 50, 50, 50, 50, 50,
     10, 10, 20, 30, 30, 20, 10, 10,
      5,  5, 10, 25, 25, 10,  5,  5,
      0,  0,  0, 20, 20,  0,  0,  0,
      5, -5,-10,  0,  0,-10, -5,  5,
      5, 10, 10,-20,-20, 10, 10,  5,
      0,  0,  0,  0,  0,  0,  0,  0};
const int PST_KNIGHT[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50};
const int PST_BISHOP[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20};
const int PST_ROOK[64] = {
      0,  0,  0,  0,  0,  0,  0,  0,
      5, 10, 10, 10, 10, 10, 10,  5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
      0,  0,  0,  5,  5,  0,  0,  0};
const int PST_QUEEN[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20};
const int PST_KING_MID[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,
     20, 30, 10,  0,  0, 10, 30, 20};
const int PST_KING_END[64] = {
    -50,-40,-30,-20,-20,-30,-40,-50,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -50,-30,-30,-30,-30,-30,-30,-50};

const int *const PST[7] = {nullptr, PST_PAWN, PST_KNIGHT, PST_BISHOP, PST_ROOK, PST_QUEEN, nullptr};

} // namespace

chess_bot::chess_bot(BotLevel level, const std::atomic<bool> *abortFlag)
    : abort_(abortFlag)
    , rng_(std::random_device{}())
{
    switch (level) {
    case BotLevel::Easy:
        maxDepth_ = 1; timeMs_ = 300;  jitter_ = 40; randomChance_ = 20; break;
    case BotLevel::Medium:
        maxDepth_ = 3; timeMs_ = 1200; jitter_ = 6;  randomChance_ = 0;  break;
    case BotLevel::Hard:
    default:
        maxDepth_ = 6; timeMs_ = 3000; jitter_ = 0;  randomChance_ = 0;  break;
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::vector<Move> chess_bot::allLegal(const State &s) const
{
    std::vector<Move> all;
    all.reserve(48);
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c) {
            if (s.board[r][c].empty() || s.board[r][c].color != s.turn)
                continue;
            std::vector<Move> m = game_logic::legalFrom(s, r, c);
            all.insert(all.end(), m.begin(), m.end());
        }
    return all;
}

std::vector<Move> chess_bot::noisyMoves(const State &s) const
{
    std::vector<Move> noisy;
    for (const Move &m : allLegal(s)) {
        const Piece &mover = s.board[m.fromRow][m.fromCol];
        const bool capture = !s.board[m.toRow][m.toCol].empty()
                             || (mover.type == PieceType::Pawn && m.fromCol != m.toCol);
        const bool queening = (m.promotion == PieceType::Queen);
        if (capture || queening)
            noisy.push_back(m);
    }
    return noisy;
}

int chess_bot::moveOrderScore(const State &s, const Move &m) const
{
    const Piece &mover = s.board[m.fromRow][m.fromCol];
    const Piece &target = s.board[m.toRow][m.toCol];
    int score = 0;
    if (!target.empty())                                       // most valuable victim,
        score += 1000 + 10 * VALUE[static_cast<int>(target.type)] // least valuable attacker
                 - VALUE[static_cast<int>(mover.type)];
    else if (mover.type == PieceType::Pawn && m.fromCol != m.toCol)
        score += 1000 + 900;                                   // en passant
    if (m.promotion != PieceType::NoPiece)
        score += VALUE[static_cast<int>(m.promotion)];
    return score;
}

void chess_bot::orderMoves(const State &s, std::vector<Move> &moves) const
{
    std::vector<std::pair<int, Move>> scored;
    scored.reserve(moves.size());
    for (const Move &m : moves)
        scored.push_back({moveOrderScore(s, m), m});
    std::stable_sort(scored.begin(), scored.end(),
                     [](const std::pair<int, Move> &a, const std::pair<int, Move> &b) {
                         return a.first > b.first;
                     });
    for (size_t i = 0; i < moves.size(); ++i)
        moves[i] = scored[i].second;
}

int chess_bot::evaluate(const State &s) const
{
    int score = 0;                       // from White's point of view
    int material[2] = {0, 0};            // non-pawn material per side
    int kingIdx[2] = {0, 0};

    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c) {
            const Piece &p = s.board[r][c];
            if (p.empty())
                continue;
            const bool white = (p.color == Color::White);
            const int t = static_cast<int>(p.type);
            const int idx = white ? r * 8 + c : (7 - r) * 8 + c;   // mirror for Black
            const int sign = white ? 1 : -1;

            if (p.type == PieceType::King) {
                kingIdx[white ? 0 : 1] = idx;
                continue;
            }
            score += sign * (VALUE[t] + PST[t][idx]);
            if (p.type != PieceType::Pawn)
                material[white ? 0 : 1] += VALUE[t];
        }

    const bool endgame = (material[0] + material[1]) <= 2600;
    const int *kingTable = endgame ? PST_KING_END : PST_KING_MID;
    score += kingTable[kingIdx[0]];
    score -= kingTable[kingIdx[1]];

    return (s.turn == Color::White) ? score : -score;
}

bool chess_bot::shouldStop()
{
    if (stopped_)
        return true;
    if (abort_ && abort_->load())
        stopped_ = true;
    else if ((nodes_ & 1023) == 0 && Clock::now() >= deadline_)
        stopped_ = true;
    return stopped_;
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

int chess_bot::quiesce(const State &s, int alpha, int beta, int qdepth)
{
    ++nodes_;
    if (shouldStop())
        return 0;

    const int standPat = evaluate(s);
    if (qdepth >= 8 || standPat >= beta)
        return standPat;
    if (standPat > alpha)
        alpha = standPat;

    std::vector<Move> moves = noisyMoves(s);
    orderMoves(s, moves);
    for (const Move &m : moves) {
        State t = s;
        game_logic::applyMove(t, m);
        const int score = -quiesce(t, -beta, -alpha, qdepth + 1);
        if (stopped_)
            return 0;
        if (score >= beta)
            return score;
        if (score > alpha)
            alpha = score;
    }
    return alpha;
}

int chess_bot::negamax(const State &s, int depth, int alpha, int beta, int ply)
{
    ++nodes_;
    if (shouldStop())
        return 0;
    if (s.halfmove >= 100)
        return 0;                                    // fifty-move draw

    const bool check = game_logic::inCheckState(s, s.turn);
    if (check && ply < 16)
        ++depth;                                     // check extension
    if (depth <= 0)
        return quiesce(s, alpha, beta, 0);

    std::vector<Move> moves = allLegal(s);
    if (moves.empty())
        return check ? -(MATE - ply) : 0;            // checkmate / stalemate
    orderMoves(s, moves);

    int best = -INF;
    for (const Move &m : moves) {
        State t = s;
        game_logic::applyMove(t, m);
        const int score = -negamax(t, depth - 1, -beta, -alpha, ply + 1);
        if (stopped_)
            return 0;
        if (score > best)
            best = score;
        if (best > alpha)
            alpha = best;
        if (alpha >= beta)
            break;
    }
    return best;
}

bool chess_bot::chooseMove(const game_logic &game, Move &out)
{
    const State root = game.state_;
    std::vector<Move> moves = allLegal(root);
    if (moves.empty())
        return false;

    // Easy: sometimes just play a random legal move.
    if (randomChance_ > 0) {
        std::uniform_int_distribution<int> roll(0, 99);
        if (roll(rng_) < randomChance_) {
            std::uniform_int_distribution<size_t> pick(0, moves.size() - 1);
            out = moves[pick(rng_)];
            return true;
        }
    }

    nodes_ = 0;
    stopped_ = false;
    deadline_ = Clock::now() + std::chrono::milliseconds(timeMs_);

    orderMoves(root, moves);
    Move best = moves.front();

    for (int depth = 1; depth <= maxDepth_; ++depth) {
        int alpha = -INF;
        int bestNoisy = -INF - 1;
        int bestRaw = -INF;
        Move iterBest = moves.front();
        bool complete = true;

        for (const Move &m : moves) {
            State t = root;
            game_logic::applyMove(t, m);

            // With noise we need exact scores for every root move; otherwise prune.
            const int window = (jitter_ > 0) ? -INF : alpha;
            const int raw = -negamax(t, depth - 1, -INF, -window, 1);
            if (stopped_) {
                complete = false;
                break;
            }

            int noisy = raw;
            if (jitter_ > 0) {
                std::uniform_int_distribution<int> jit(-jitter_, jitter_);
                noisy += jit(rng_);
            }
            if (noisy > bestNoisy) {
                bestNoisy = noisy;
                bestRaw = raw;
                iterBest = m;
            }
            if (raw > alpha)
                alpha = raw;
        }

        if (!complete)
            break;                                   // time ran out: keep the last full iteration

        best = iterBest;
        // Try the best move first at the next depth.
        auto it = std::find_if(moves.begin(), moves.end(), [&](const Move &x) {
            return x.fromRow == best.fromRow && x.fromCol == best.fromCol
                && x.toRow == best.toRow && x.toCol == best.toCol && x.promotion == best.promotion;
        });
        if (it != moves.end())
            std::rotate(moves.begin(), it, it + 1);

        if (bestRaw > MATE - 100)
            break;                                   // forced mate found
    }

    out = best;
    return true;
}

} // namespace chess
