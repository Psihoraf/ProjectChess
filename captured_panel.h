#ifndef CAPTURED_PANEL_H
#define CAPTURED_PANEL_H

#include <QWidget>

#include "figures.h"
#include "game_logic.h"

// Small panel (placed in the bottom right of the game page) that shows which
// pieces have been captured and how many of each type, for both sides:
//   "You captured:"       - the opponent's pieces you have taken
//   "Opponent captured:"  - your pieces that have been taken
class captured_panel : public QWidget
{
    Q_OBJECT

public:
    explicit captured_panel(QWidget *parent = nullptr);

    // Re-reads the counts from the game and repaints. `local` is the side
    // this window plays (decides which row is "yours").
    void refresh(const chess::game_logic &game, chess::Color local);

    QSize sizeHint() const override { return QSize(260, 150); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void drawRow(QPainter &painter, int rowIndex, const QString &title, chess::Color victim);

    figures figures_;
    int counts_[2][5] = {};                 // [victim colour][pawn, knight, bishop, rook, queen]
    chess::Color local_ = chess::Color::White;
};

#endif // CAPTURED_PANEL_H
