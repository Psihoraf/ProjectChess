#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

#include "game_logic.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class chess_table;
class network_manager;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onHostClicked();
    void onJoinClicked();
    void onDisconnectClicked();
    void onResignClicked();
    void onNewGameClicked();

    void onListening(quint16 port);
    void onConnected();
    void onDisconnected();
    void onNetworkError(const QString &message);
    void onMoveReceived(const chess::Move &move);
    void onResignReceived();
    void onNewGameReceived();

    void updateStatus();

private:
    enum class LinkState { Idle, Waiting, Playing };

    void setLinkState(LinkState state);
    void startNewGame();

    Ui::MainWindow *ui;
    chess_table *board_;
    network_manager *net_;

    QLineEdit *hostEdit_;
    QSpinBox *portSpin_;
    QPushButton *hostBtn_;
    QPushButton *joinBtn_;
    QPushButton *disconnectBtn_;
    QPushButton *resignBtn_;
    QPushButton *newGameBtn_;
    QLabel *statusLabel_;

    LinkState state_ = LinkState::Idle;
    bool resigned_ = false;
    QString resignText_;
};

#endif // MAINWINDOW_H
