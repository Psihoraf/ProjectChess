#include "move_list.h"

#include <QFont>
#include <QHeaderView>
#include <QTableWidgetItem>

move_list::move_list(QWidget *parent)
    : QTableWidget(0, 3, parent)
{
    setHorizontalHeaderLabels({tr("#"), tr("White"), tr("Black")});
    verticalHeader()->setVisible(false);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::NoSelection);
    setFocusPolicy(Qt::NoFocus);
    setShowGrid(false);
    setMinimumHeight(120);
}

void move_list::refresh(const chess::game_logic &game)
{
    // Games start from the normal position, so White plays the even half-moves.
    const int plies = game.moveCount();

    clearContents();
    setRowCount((plies + 1) / 2);

    QFont bold = font();
    bold.setBold(true);

    for (int i = 0; i < plies; ++i) {
        const int row = i / 2;
        const int column = 1 + (i % 2);

        if (i % 2 == 0) {
            QTableWidgetItem *number = new QTableWidgetItem(QString::number(row + 1) + QLatin1Char('.'));
            number->setFlags(Qt::ItemIsEnabled);
            number->setTextAlignment(Qt::AlignCenter);
            setItem(row, 0, number);
        }

        QTableWidgetItem *item = new QTableWidgetItem(QString::fromStdString(game.sanAt(i)));
        item->setFlags(Qt::ItemIsEnabled);
        if (i == plies - 1)
            item->setFont(bold);
        setItem(row, column, item);
    }
    scrollToBottom();
}
