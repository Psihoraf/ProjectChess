#ifndef CHESS_TABLE_H
#define CHESS_TABLE_H

#include <QGraphicsScene>
#include <QGraphicsView>
#include <vector>

#include "figures.h"
#include "game_logic.h"

// The board widget. It only draws the position held by game_logic and turns
// mouse clicks into moves; all rules live in game_logic.
class chess_table : public QGraphicsView
{
    Q_OBJECT

public:
    explicit chess_table(QWidget *parent = nullptr);

    void newGame();
    void setLocalColor(chess::Color color);   // side this window plays (also flips the board)
    chess::Color localColor() const { return localColor_; }
    void setInteractive(bool enabled);        // false = clicks are ignored
    bool applyRemoteMove(const chess::Move &move);
    const chess::game_logic &logic() const { return logic_; }

signals:
    void moveMade(const chess::Move &move);   // the LOCAL player made a move
    void positionChanged();                   // any change (local, remote, new game)

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    static constexpr int kCell = 100;

    void redraw();
    void drawLabels();
    void addLabel(const QString &text, qreal x, qreal y);
    void mapCell(int r, int c, int &outR, int &outC) const;  // logical <-> display (symmetric)
    bool canMoveNow() const;
    void clearSelection();
    void handleSquareClick(int row, int col);
    chess::PieceType askPromotion();

    QGraphicsScene *scene_;
    figures figures_;
    chess::game_logic logic_;
    chess::Color localColor_ = chess::Color::White;
    bool flipped_ = false;
    bool interactive_ = false;

    bool hasSelection_ = false;
    int selRow_ = -1;
    int selCol_ = -1;
    std::vector<chess::Move> legalTargets_;
};

#endif // CHESS_TABLE_H
