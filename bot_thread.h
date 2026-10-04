#ifndef BOT_THREAD_H
#define BOT_THREAD_H

#include <QThread>
#include <atomic>

#include "chess_bot.h"
#include "game_logic.h"

// Runs one search on a copy of the position, so the GUI stays responsive.
// When the thread's finished() signal arrives, read hasMove() / result().
class bot_thread : public QThread
{
    Q_OBJECT

public:
    bot_thread(const chess::game_logic &position, chess::BotLevel level, QObject *parent = nullptr);

    void cancel() { abort_.store(true); }       // ask the search to stop early
    bool hasMove() const { return found_; }
    chess::Move result() const { return result_; }

protected:
    void run() override;

private:
    chess::game_logic position_;
    chess::BotLevel level_;
    std::atomic<bool> abort_{false};
    chess::Move result_;
    bool found_ = false;
};

#endif // BOT_THREAD_H
