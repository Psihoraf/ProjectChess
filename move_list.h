#ifndef MOVE_LIST_H
#define MOVE_LIST_H

#include <QTableWidget>

#include "game_logic.h"

// Table with the moves of the current game in standard notation:
//   1.  e4    e5
//   2.  Nf3   Nc6
// The latest move is shown in bold and the list scrolls to it.
class move_list : public QTableWidget
{
    Q_OBJECT

public:
    explicit move_list(QWidget *parent = nullptr);

    void refresh(const chess::game_logic &game);

    QSize sizeHint() const override { return QSize(260, 200); }
};

#endif // MOVE_LIST_H
