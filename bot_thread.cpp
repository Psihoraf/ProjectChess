#include "bot_thread.h"

bot_thread::bot_thread(const chess::game_logic &position, chess::BotLevel level, QObject *parent)
    : QThread(parent)
    , position_(position)
    , level_(level)
{
}

void bot_thread::run()
{
    chess::chess_bot bot(level_, &abort_);
    found_ = bot.chooseMove(position_, result_);
}
