#ifndef CHESS_BOT_H
#define CHESS_BOT_H

// The computer player: iterative-deepening negamax with alpha-beta pruning,
// capture quiescence search, and a material + piece-square evaluation.
// Pure C++ (no Qt) so it can run on a worker thread.

#include <atomic>
#include <chrono>
#include <random>
#include <vector>

#include "game_logic.h"

namespace chess {

enum class BotLevel { Easy, Medium, Hard };

class chess_bot
{
public:
    // If abortFlag is given and becomes true, the search stops as soon as possible.
    explicit chess_bot(BotLevel level, const std::atomic<bool> *abortFlag = nullptr);

    // Picks a move for the side to move. Returns false if there is no legal move.
    bool chooseMove(const game_logic &game, Move &out);

private:
    using State = game_logic::State;
    using Clock = std::chrono::steady_clock;

    std::vector<Move> allLegal(const State &s) const;
    std::vector<Move> noisyMoves(const State &s) const;   // captures / queen promotions
    void orderMoves(const State &s, std::vector<Move> &moves) const;
    int moveOrderScore(const State &s, const Move &m) const;
    int evaluate(const State &s) const;                   // from the side to move's view
    int negamax(const State &s, int depth, int alpha, int beta, int ply);
    int quiesce(const State &s, int alpha, int beta, int qdepth);
    bool shouldStop();

    int maxDepth_;
    int timeMs_;
    int jitter_;          // random noise added to root scores (weaker, less repetitive play)
    int randomChance_;    // % chance to play a completely random move (Easy only)

    const std::atomic<bool> *abort_;
    Clock::time_point deadline_;
    unsigned long nodes_ = 0;
    bool stopped_ = false;
    std::mt19937 rng_;
};

} // namespace chess

#endif // CHESS_BOT_H
