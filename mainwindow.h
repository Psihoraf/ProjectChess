#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

#include "chess_bot.h"
#include "game_logic.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class bot_thread;
class captured_panel;
class chess_table;
class network_manager;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;

// The window holds four pages in a QStackedWidget:
//   main menu  ->  computer settings  ->  game
//              ->  network (host/join) ->  game
// "Back" / "Main menu" buttons always lead back to the main menu.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // navigation
    void showMainMenu();            // leaves whatever is running (no question asked)
    void onMenuButtonInGame();      // same, but asks first if a game is in progress

    // starting games
    void onStartComputerClicked();
    void onHostClicked();
    void onJoinClicked();

    // in-game buttons
    void onResignClicked();
    void onNewGameClicked();

    // network events
    void onListening(quint16 port);
    void onConnected();
    void onDisconnected();
    void onNetworkError(const QString &message);
    void onMoveReceived(const chess::Move &move);
    void onResignReceived();
    void onNewGameReceived();

    // computer player / status
    void onBotFinished();
    void updateStatus();

private:
    enum class Mode { Network, Computer };

    QWidget *buildMainMenuPage();
    QWidget *buildComputerPage();
    QWidget *buildNetworkPage();
    QWidget *buildGamePage();

    void setNetworkControlsEnabled(bool enabled);
    void startNewGame();
    bool gameInProgress() const;
    void maybeStartBot();
    void cancelBot();

    Ui::MainWindow *ui;
    chess_table *board_;
    network_manager *net_;
    bot_thread *botThread_ = nullptr;

    QStackedWidget *pages_;
    QWidget *menuPage_;
    QWidget *computerPage_;
    QWidget *networkPage_;
    QWidget *gamePage_;

    // computer settings page
    QComboBox *difficultyCombo_;
    QComboBox *colorCombo_;

    // network page
    QLineEdit *hostEdit_;
    QSpinBox *portSpin_;
    QPushButton *hostBtn_;
    QPushButton *joinBtn_;
    QLabel *netStatusLabel_;

    // game page
    QLabel *gameStatusLabel_;
    captured_panel *capturedPanel_;
    QPushButton *resignBtn_;
    QPushButton *newGameBtn_;
    QPushButton *menuBtn_;

    Mode mode_ = Mode::Computer;
    chess::BotLevel botLevel_ = chess::BotLevel::Medium;
    bool playing_ = false;          // a game is on the game page
    bool connectionLost_ = false;   // network game whose opponent is gone
    bool resigned_ = false;
    QString resignText_;
};

#endif // MAINWINDOW_H
