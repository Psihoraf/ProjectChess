#ifndef FIGURES_H
#define FIGURES_H

#include <QGraphicsPixmapItem>
#include <QPixmap>
#include <vector>

#include "game_logic.h"

class figures {
private:
    std::vector<QPixmap> ALL_FIGURES;

public:
    figures();
    std::vector<QGraphicsPixmapItem*> setImages();
    QPixmap pixmapFor(chess::PieceType type, chess::Color color) const;
};

#endif // FIGURES_H
