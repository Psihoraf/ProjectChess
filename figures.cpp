#include "figures.h"

#include <QDebug>
#include <QString>
#include <QStringList>

figures::figures()
{
    // Order matters: pixmapFor() relies on it (pawn, rook, knight, bishop,
    // queen, king - each as black then white).
    const QStringList files = {
        ":/images/images/blackPawn.png",     ":/images/images/whitePawn.png",
        ":/images/images/blackRook.png",     ":/images/images/whiteRook.png",
        ":/images/images/blackHorse.png",    ":/images/images/whiteHorse.png",
        ":/images/images/blackElephant.png", ":/images/images/whiteElephant.png",
        ":/images/images/blackQueen.png",    ":/images/images/whiteQueen.png",
        ":/images/images/blackKing.png",     ":/images/images/whiteKing.png"
    };

    for (const QString &file : files) {
        QPixmap pm(file);
        if (pm.isNull())
            qWarning() << "Cannot load image" << file;
        ALL_FIGURES.push_back(pm.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

std::vector<QGraphicsPixmapItem*> figures::setImages()
{
    std::vector<QGraphicsPixmapItem*> allFigures;
    for (const QPixmap &pm : ALL_FIGURES)
        allFigures.push_back(new QGraphicsPixmapItem(pm));
    return allFigures;
}

QPixmap figures::pixmapFor(chess::PieceType type, chess::Color color) const
{
    int base = 0;
    switch (type) {
    case chess::PieceType::Pawn:   base = 0; break;
    case chess::PieceType::Rook:   base = 1; break;
    case chess::PieceType::Knight: base = 2; break;
    case chess::PieceType::Bishop: base = 3; break;
    case chess::PieceType::Queen:  base = 4; break;
    case chess::PieceType::King:   base = 5; break;
    default: return QPixmap();
    }
    return ALL_FIGURES[base * 2 + (color == chess::Color::White ? 1 : 0)];
}
