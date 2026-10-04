#include "captured_panel.h"

#include <QFont>
#include <QPainter>
#include <QPen>

namespace {

// Order in which captured pieces are listed (the king can never be captured).
const chess::PieceType kTypes[5] = {
    chess::PieceType::Pawn, chess::PieceType::Knight, chess::PieceType::Bishop,
    chess::PieceType::Rook, chess::PieceType::Queen
};

const int kIcon = 15;      // icon size in pixels
const int kSlot = 52;      // width of one "icon + xN" slot

inline int colorIndex(chess::Color c) { return c == chess::Color::White ? 0 : 1; }

} // namespace

captured_panel::captured_panel(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(150);
}

void captured_panel::refresh(const chess::game_logic &game, chess::Color local)
{
    local_ = local;
    for (int c = 0; c < 2; ++c) {
        const chess::Color victim = (c == 0) ? chess::Color::White : chess::Color::Black;
        for (int i = 0; i < 5; ++i)
            counts_[c][i] = game.capturedCount(victim, kTypes[i]);
    }
    update();
}

void captured_panel::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Light background so black pieces are visible in dark themes too.
    painter.fillRect(rect(), QColor(245, 245, 245));
    painter.setPen(QPen(QColor(200, 200, 200), 1));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
    painter.drawLine(8, height() / 2, width() - 8, height() / 2);

    drawRow(painter, 0, tr("You captured:"), chess::opposite(local_));
    drawRow(painter, 1, tr("Opponent captured:"), local_);
}

void captured_panel::drawRow(QPainter &painter, int rowIndex, const QString &title,
                             chess::Color victim)
{
    const int rowHeight = height() / 2;
    const int top = rowIndex * rowHeight;

    QFont titleFont = font();
    titleFont.setBold(true);
    titleFont.setPointSize(9);
    painter.setFont(titleFont);
    painter.setPen(Qt::black);
    painter.drawText(QRect(8, top + 4, width() - 16, 18), Qt::AlignLeft | Qt::AlignVCenter, title);

    QFont countFont = font();
    countFont.setBold(true);
    countFont.setPointSize(11);

    const int iconTop = top + 26;
    int x = 8;
    bool any = false;
    for (int i = 0; i < 5; ++i) {
        const int count = counts_[colorIndex(victim)][i];
        if (count == 0)
            continue;
        any = true;

        painter.drawPixmap(QRect(x, iconTop, kIcon, kIcon), figures_.pixmapFor(kTypes[i], victim));

        painter.setFont(countFont);
        painter.setPen(Qt::black);
        const QString text = QString(QChar(0x00D7)) + QString::number(count);   // "x3"
        painter.drawText(QRect(x + kIcon + 1, iconTop, kSlot - kIcon - 1, kIcon),
                         Qt::AlignLeft | Qt::AlignVCenter, text);
        x += kSlot;
    }

    if (!any) {
        QFont hint = font();
        hint.setItalic(true);
        hint.setPointSize(9);
        painter.setFont(hint);
        painter.setPen(QColor(130, 130, 130));
        painter.drawText(QRect(8, iconTop, width() - 16, kIcon), Qt::AlignLeft | Qt::AlignVCenter,
                         tr("nothing yet"));
    }
}
