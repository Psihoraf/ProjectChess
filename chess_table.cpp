#include "chess_table.h"

#include <QAction>
#include <QBrush>
#include <QCursor>
#include <QFont>
#include <QGraphicsEllipseItem>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <cmath>

namespace {
const QColor BROWN(139, 69, 19);
const QColor WHITE(Qt::white);
}

chess_table::chess_table(QWidget *parent)
    : QGraphicsView(parent)
{
    scene_ = new QGraphicsScene(this);
    setScene(scene_);
    scene_->setSceneRect(-40, -40, 8 * kCell + 80, 8 * kCell + 80);

    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setBackgroundBrush(QColor(250, 250, 250));
    setMinimumSize(480, 480);

    redraw();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void chess_table::newGame()
{
    logic_.reset();
    clearSelection();
    redraw();
    emit positionChanged();
}

void chess_table::setLocalColor(chess::Color color)
{
    localColor_ = color;
    flipped_ = (color == chess::Color::Black);   // local player is always at the bottom
    clearSelection();
    redraw();
}

void chess_table::setInteractive(bool enabled)
{
    interactive_ = enabled;
    if (!enabled)
        clearSelection();
    redraw();
}

bool chess_table::applyRemoteMove(const chess::Move &move)
{
    if (logic_.turn() == localColor_)   // it is not the opponent's turn
        return false;
    if (!logic_.makeMove(move))
        return false;
    clearSelection();
    redraw();
    emit positionChanged();
    return true;
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

bool chess_table::canMoveNow() const
{
    return interactive_ && !logic_.isGameOver() && logic_.turn() == localColor_;
}

void chess_table::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QGraphicsView::mousePressEvent(event);
        return;
    }
    if (!canMoveNow())
        return;

    const QPointF pos = mapToScene(event->pos());
    const int dc = static_cast<int>(std::floor(pos.x() / kCell));
    const int dr = static_cast<int>(std::floor(pos.y() / kCell));
    if (dr < 0 || dr > 7 || dc < 0 || dc > 7) {
        clearSelection();
        redraw();
        return;
    }

    int row, col;
    mapCell(dr, dc, row, col);
    handleSquareClick(row, col);
}

void chess_table::handleSquareClick(int row, int col)
{
    if (hasSelection_) {
        if (row == selRow_ && col == selCol_) {      // click again = deselect
            clearSelection();
            redraw();
            return;
        }

        bool isTarget = false;
        for (const chess::Move &m : legalTargets_)
            if (m.toRow == row && m.toCol == col) {
                isTarget = true;
                break;
            }

        if (isTarget) {
            chess::Move mv{selRow_, selCol_, row, col, chess::PieceType::NoPiece};
            if (logic_.needsPromotion(selRow_, selCol_, row, col)) {
                const chess::PieceType choice = askPromotion();
                if (choice == chess::PieceType::NoPiece)
                    return;                           // dialog cancelled
                mv.promotion = choice;
            }
            if (logic_.makeMove(mv)) {
                clearSelection();
                redraw();
                emit moveMade(mv);
                emit positionChanged();
            }
            return;
        }
    }

    const chess::Piece p = logic_.pieceAt(row, col);
    if (!p.empty() && p.color == logic_.turn()) {
        hasSelection_ = true;
        selRow_ = row;
        selCol_ = col;
        legalTargets_ = logic_.legalMovesFrom(row, col);
    } else {
        clearSelection();
    }
    redraw();
}

chess::PieceType chess_table::askPromotion()
{
    QMenu menu(this);
    QAction *queen  = menu.addAction(tr("Queen"));
    QAction *rook   = menu.addAction(tr("Rook"));
    QAction *bishop = menu.addAction(tr("Bishop"));
    QAction *knight = menu.addAction(tr("Knight"));
    QAction *chosen = menu.exec(QCursor::pos());

    if (chosen == queen)  return chess::PieceType::Queen;
    if (chosen == rook)   return chess::PieceType::Rook;
    if (chosen == bishop) return chess::PieceType::Bishop;
    if (chosen == knight) return chess::PieceType::Knight;
    return chess::PieceType::NoPiece;
}

void chess_table::clearSelection()
{
    hasSelection_ = false;
    selRow_ = selCol_ = -1;
    legalTargets_.clear();
}

void chess_table::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void chess_table::mapCell(int r, int c, int &outR, int &outC) const
{
    outR = flipped_ ? 7 - r : r;
    outC = flipped_ ? 7 - c : c;
}

void chess_table::addLabel(const QString &text, qreal x, qreal y)
{
    QGraphicsTextItem *t = scene_->addText(text, QFont("Arial", 16, QFont::Bold));
    t->setDefaultTextColor(Qt::black);
    t->setPos(x, y);
    t->setZValue(3);
}

void chess_table::drawLabels()
{
    for (int d = 0; d < 8; ++d) {
        const QString file(QChar('A' + (flipped_ ? 7 - d : d)));
        const QString rank(QChar('1' + (flipped_ ? d : 7 - d)));
        addLabel(file, d * kCell + kCell / 2 - 10, 8 * kCell + 5);
        addLabel(file, d * kCell + kCell / 2 - 10, -36);
        addLabel(rank, -32, d * kCell + kCell / 2 - 16);
        addLabel(rank, 8 * kCell + 6, d * kCell + kCell / 2 - 16);
    }
}

void chess_table::redraw()
{
    scene_->clear();

    // Squares (display coordinates: dr/dc).
    for (int dr = 0; dr < 8; ++dr)
        for (int dc = 0; dc < 8; ++dc) {
            QGraphicsRectItem *sq = scene_->addRect(dc * kCell, dr * kCell, kCell, kCell,
                                                    QPen(Qt::black, 1),
                                                    QBrush(((dr + dc) % 2 == 1) ? BROWN : WHITE));
            sq->setZValue(0);
        }

    drawLabels();

    auto overlay = [&](int r, int c, const QColor &color) {
        int dr, dc;
        mapCell(r, c, dr, dc);
        QGraphicsRectItem *it = scene_->addRect(dc * kCell, dr * kCell, kCell, kCell,
                                                QPen(Qt::NoPen), QBrush(color));
        it->setZValue(1);
    };

    // Last move, check, selection.
    if (logic_.hasLastMove()) {
        const chess::Move &lm = logic_.lastMove();
        overlay(lm.fromRow, lm.fromCol, QColor(255, 235, 59, 110));
        overlay(lm.toRow, lm.toCol, QColor(255, 235, 59, 110));
    }
    if (logic_.inCheck()) {
        int kr, kc;
        if (logic_.findKing(logic_.turn(), kr, kc))
            overlay(kr, kc, QColor(220, 20, 60, 160));
    }
    if (hasSelection_)
        overlay(selRow_, selCol_, QColor(255, 215, 0, 150));

    // Legal destinations of the selected piece.
    if (hasSelection_) {
        const chess::Piece selected = logic_.pieceAt(selRow_, selCol_);
        bool drawn[8][8] = {};
        for (const chess::Move &m : legalTargets_) {
            if (drawn[m.toRow][m.toCol])
                continue;                                  // promotions appear 4 times
            drawn[m.toRow][m.toCol] = true;

            int dr, dc;
            mapCell(m.toRow, m.toCol, dr, dc);
            const bool capture = !logic_.pieceAt(m.toRow, m.toCol).empty()
                                 || (selected.type == chess::PieceType::Pawn && m.toCol != selCol_);
            QGraphicsEllipseItem *mark;
            if (capture) {
                mark = scene_->addEllipse(dc * kCell + 6, dr * kCell + 6, kCell - 12, kCell - 12,
                                          QPen(QColor(46, 160, 67, 200), 6), QBrush(Qt::NoBrush));
            } else {
                mark = scene_->addEllipse(dc * kCell + kCell / 2 - 14, dr * kCell + kCell / 2 - 14,
                                          28, 28, QPen(Qt::NoPen), QBrush(QColor(46, 160, 67, 190)));
            }
            mark->setZValue(1.5);
        }
    }

    // Pieces.
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c) {
            const chess::Piece p = logic_.pieceAt(r, c);
            if (p.empty())
                continue;
            const QPixmap pm = figures_.pixmapFor(p.type, p.color);
            int dr, dc;
            mapCell(r, c, dr, dc);
            QGraphicsPixmapItem *item = scene_->addPixmap(pm);
            item->setPos(dc * kCell + (kCell - pm.width()) / 2,
                         dr * kCell + (kCell - pm.height()) / 2);
            item->setZValue(2);
        }
}
