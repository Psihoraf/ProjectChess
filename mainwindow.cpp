#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QApplication>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "bot_thread.h"
#include "captured_panel.h"
#include "chess_table.h"
#include "move_list.h"
#include "network_manager.h"

namespace {

QPushButton *makeMenuButton(const QString &text)
{
    QPushButton *b = new QPushButton(text);
    b->setFixedSize(340, 50);
    QFont f = b->font();
    f.setPointSize(12);
    b->setFont(f);
    return b;
}

QLabel *makeTitle(const QString &text, int pointSize = 26)
{
    QLabel *l = new QLabel(text);
    QFont f = l->font();
    f.setPointSize(pointSize);
    f.setBold(true);
    l->setFont(f);
    l->setAlignment(Qt::AlignCenter);
    return l;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(tr("Chess"));
    resize(1050, 850);

    board_ = new chess_table(this);     // gets re-parented into the game page
    net_ = new network_manager(this);

    pages_ = new QStackedWidget(this);
    menuPage_ = buildMainMenuPage();
    computerPage_ = buildComputerPage();
    networkPage_ = buildNetworkPage();
    gamePage_ = buildGamePage();
    pages_->addWidget(menuPage_);
    pages_->addWidget(computerPage_);
    pages_->addWidget(networkPage_);
    pages_->addWidget(gamePage_);

    QVBoxLayout *root = new QVBoxLayout(ui->centralwidget);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(pages_);

    // A local move goes to the opponent only in network games.
    connect(board_, &chess_table::moveMade, this, [this](const chess::Move &m) {
        if (mode_ == Mode::Network)
            net_->sendMove(m);
    });
    connect(board_, &chess_table::positionChanged, this, &MainWindow::onBoardChanged);

    connect(net_, &network_manager::listening, this, &MainWindow::onListening);
    connect(net_, &network_manager::connected, this, &MainWindow::onConnected);
    connect(net_, &network_manager::disconnected, this, &MainWindow::onDisconnected);
    connect(net_, &network_manager::errorOccurred, this, &MainWindow::onNetworkError);
    connect(net_, &network_manager::moveReceived, this, &MainWindow::onMoveReceived);
    connect(net_, &network_manager::resignReceived, this, &MainWindow::onResignReceived);
    connect(net_, &network_manager::newGameReceived, this, &MainWindow::onNewGameReceived);
    connect(net_, &network_manager::undoRequested, this, &MainWindow::onUndoRequested);
    connect(net_, &network_manager::undoAnswered, this, &MainWindow::onUndoAnswered);

    showMainMenu();
}

MainWindow::~MainWindow()
{
    cancelBot();
    delete ui;
}

// ---------------------------------------------------------------------------
// Building the pages
// ---------------------------------------------------------------------------

QWidget *MainWindow::buildMainMenuPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *lay = new QVBoxLayout(page);

    QPushButton *vsComputer = makeMenuButton(tr("Play against computer"));
    QPushButton *vsPlayer = makeMenuButton(tr("Play against player"));
    QPushButton *quit = makeMenuButton(tr("Quit"));

    lay->addStretch(2);
    lay->addWidget(makeTitle(tr("Chess"), 40));
    QLabel *sub = new QLabel(tr("Choose a game mode"));
    sub->setAlignment(Qt::AlignCenter);
    lay->addWidget(sub);
    lay->addSpacing(24);
    lay->addWidget(vsComputer, 0, Qt::AlignHCenter);
    lay->addSpacing(8);
    lay->addWidget(vsPlayer, 0, Qt::AlignHCenter);
    lay->addSpacing(8);
    lay->addWidget(quit, 0, Qt::AlignHCenter);
    lay->addStretch(3);

    connect(vsComputer, &QPushButton::clicked, this, [this] {
        pages_->setCurrentWidget(computerPage_);
    });
    connect(vsPlayer, &QPushButton::clicked, this, [this] {
        netStatusLabel_->setText(tr("Host a game and wait for a friend, or join a game that is already hosted."));
        setNetworkControlsEnabled(true);
        pages_->setCurrentWidget(networkPage_);
    });
    connect(quit, &QPushButton::clicked, qApp, &QApplication::quit);
    return page;
}

QWidget *MainWindow::buildComputerPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *lay = new QVBoxLayout(page);

    QWidget *box = new QWidget;
    box->setFixedWidth(340);
    QFormLayout *form = new QFormLayout(box);
    difficultyCombo_ = new QComboBox;
    difficultyCombo_->addItems({tr("Easy"), tr("Medium"), tr("Hard")});
    difficultyCombo_->setCurrentIndex(1);
    colorCombo_ = new QComboBox;
    colorCombo_->addItems({tr("White"), tr("Black"), tr("Random")});
    form->addRow(tr("Level:"), difficultyCombo_);
    form->addRow(tr("You play:"), colorCombo_);

    QPushButton *start = makeMenuButton(tr("Start game"));
    QPushButton *back = makeMenuButton(tr("Back to main menu"));

    lay->addStretch(2);
    lay->addWidget(makeTitle(tr("Play against the computer")));
    lay->addSpacing(20);
    lay->addWidget(box, 0, Qt::AlignHCenter);
    lay->addSpacing(20);
    lay->addWidget(start, 0, Qt::AlignHCenter);
    lay->addSpacing(8);
    lay->addWidget(back, 0, Qt::AlignHCenter);
    lay->addStretch(3);

    connect(start, &QPushButton::clicked, this, &MainWindow::onStartComputerClicked);
    connect(back, &QPushButton::clicked, this, &MainWindow::showMainMenu);
    return page;
}

QWidget *MainWindow::buildNetworkPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *lay = new QVBoxLayout(page);

    QWidget *box = new QWidget;
    box->setFixedWidth(340);
    QFormLayout *form = new QFormLayout(box);
    hostEdit_ = new QLineEdit(QStringLiteral("127.0.0.1"));
    portSpin_ = new QSpinBox;
    portSpin_->setRange(1024, 65535);
    portSpin_->setValue(5555);
    form->addRow(tr("Address (to join):"), hostEdit_);
    form->addRow(tr("Port:"), portSpin_);

    hostBtn_ = makeMenuButton(tr("Host a game (you play White)"));
    joinBtn_ = makeMenuButton(tr("Join a game (you play Black)"));
    QPushButton *back = makeMenuButton(tr("Back to main menu"));

    netStatusLabel_ = new QLabel;
    netStatusLabel_->setWordWrap(true);
    netStatusLabel_->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    netStatusLabel_->setFixedWidth(360);
    netStatusLabel_->setMinimumHeight(70);

    lay->addStretch(2);
    lay->addWidget(makeTitle(tr("Play against a player")));
    lay->addSpacing(20);
    lay->addWidget(box, 0, Qt::AlignHCenter);
    lay->addSpacing(12);
    lay->addWidget(hostBtn_, 0, Qt::AlignHCenter);
    lay->addSpacing(8);
    lay->addWidget(joinBtn_, 0, Qt::AlignHCenter);
    lay->addSpacing(12);
    lay->addWidget(netStatusLabel_, 0, Qt::AlignHCenter);
    lay->addWidget(back, 0, Qt::AlignHCenter);
    lay->addStretch(3);

    connect(hostBtn_, &QPushButton::clicked, this, &MainWindow::onHostClicked);
    connect(joinBtn_, &QPushButton::clicked, this, &MainWindow::onJoinClicked);
    connect(back, &QPushButton::clicked, this, &MainWindow::showMainMenu);   // also cancels waiting
    return page;
}

QWidget *MainWindow::buildGamePage()
{
    QWidget *page = new QWidget;
    QHBoxLayout *lay = new QHBoxLayout(page);

    QWidget *side = new QWidget;
    side->setFixedWidth(280);
    QVBoxLayout *sl = new QVBoxLayout(side);

    gameStatusLabel_ = new QLabel;
    gameStatusLabel_->setWordWrap(true);
    gameStatusLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    gameStatusLabel_->setMinimumHeight(100);

    undoBtn_ = new QPushButton(tr("Undo move"));
    resignBtn_ = new QPushButton(tr("Resign"));
    newGameBtn_ = new QPushButton(tr("New game"));
    menuBtn_ = new QPushButton(tr("Main menu"));
    moveList_ = new move_list;

    QHBoxLayout *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(undoBtn_);
    buttonRow->addWidget(resignBtn_);

    sl->addWidget(gameStatusLabel_);
    sl->addLayout(buttonRow);
    sl->addWidget(newGameBtn_);
    sl->addWidget(moveList_, 1);                // the list takes the free space
    capturedPanel_ = new captured_panel;        // bottom right: captured pieces and counts
    sl->addWidget(capturedPanel_);
    sl->addWidget(menuBtn_);

    lay->addWidget(board_, 1);
    lay->addWidget(side);

    connect(undoBtn_, &QPushButton::clicked, this, &MainWindow::onUndoClicked);
    connect(resignBtn_, &QPushButton::clicked, this, &MainWindow::onResignClicked);
    connect(newGameBtn_, &QPushButton::clicked, this, &MainWindow::onNewGameClicked);
    connect(menuBtn_, &QPushButton::clicked, this, &MainWindow::onMenuButtonInGame);
    return page;
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

void MainWindow::showMainMenu()
{
    cancelBot();
    closeUndoDialog();
    net_->close();                         // stops listening / drops the connection
    undoPending_ = false;
    notice_.clear();
    playing_ = false;
    connectionLost_ = false;
    resigned_ = false;
    resignText_.clear();
    board_->setInteractive(false);

    setNetworkControlsEnabled(true);
    setWindowTitle(tr("Chess"));
    pages_->setCurrentWidget(menuPage_);
}

bool MainWindow::gameInProgress() const
{
    return playing_ && !connectionLost_ && !resigned_ && !board_->logic().isGameOver();
}

void MainWindow::onMenuButtonInGame()
{
    if (gameInProgress()) {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this, tr("Leave the game"),
            tr("The game is not finished. Do you really want to go back to the main menu?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }
    showMainMenu();
}

void MainWindow::setNetworkControlsEnabled(bool enabled)
{
    hostBtn_->setEnabled(enabled);
    joinBtn_->setEnabled(enabled);
    hostEdit_->setEnabled(enabled);
    portSpin_->setEnabled(enabled);
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void MainWindow::startNewGame()
{
    cancelBot();
    closeUndoDialog();
    undoPending_ = false;
    notice_.clear();
    resigned_ = false;
    resignText_.clear();
    connectionLost_ = false;
    undoBtn_->setText(mode_ == Mode::Computer ? tr("Undo move") : tr("Ask to undo"));
    board_->newGame();            // emits positionChanged -> updateStatus()
    board_->setInteractive(true);
    updateStatus();
}

void MainWindow::updateStatus()
{
    if (!playing_ || connectionLost_)
        return;

    using chess::GameStatus;
    const chess::game_logic &g = board_->logic();
    const chess::Color me = board_->localColor();
    const bool vsComputer = (mode_ == Mode::Computer);

    QString text;
    bool over = false;

    if (resigned_) {
        text = resignText_;
        over = true;
    } else {
        switch (g.status()) {
        case GameStatus::Checkmate:
            text = (chess::opposite(g.turn()) == me) ? tr("Checkmate! You win.")
                                                     : tr("Checkmate. You lose.");
            over = true;
            break;
        case GameStatus::Stalemate:
            text = tr("Draw by stalemate.");
            over = true;
            break;
        case GameStatus::DrawInsufficientMaterial:
            text = tr("Draw: insufficient material.");
            over = true;
            break;
        case GameStatus::DrawFiftyMove:
            text = tr("Draw: fifty-move rule.");
            over = true;
            break;
        case GameStatus::DrawRepetition:
            text = tr("Draw: threefold repetition.");
            over = true;
            break;
        case GameStatus::Check:
        case GameStatus::Ongoing:
            if (g.turn() == me)
                text = tr("Your turn.");
            else
                text = vsComputer ? tr("Computer is thinking...") : tr("Waiting for the opponent...");
            if (g.status() == GameStatus::Check)
                text += tr(" Check!");
            break;
        }
    }

    const QString who = (me == chess::Color::White) ? tr("You play White.") : tr("You play Black.");
    if (!notice_.isEmpty())
        text += QLatin1Char('\n') + notice_;
    gameStatusLabel_->setText(who + QLatin1Char('\n') + text);

    board_->setInteractive(!over && !undoPending_);   // frozen while we wait for an undo answer
    resignBtn_->setEnabled(!over);
    newGameBtn_->setEnabled(over || vsComputer);   // against the computer you can restart any time

    if (!over)
        maybeStartBot();
    updateUndoButton();                            // after the bot may have started
}

void MainWindow::onResignClicked()
{
    cancelBot();
    if (mode_ == Mode::Network)
        net_->sendResign();
    resigned_ = true;
    resignText_ = tr("You resigned. You lose.");
    updateStatus();
}

void MainWindow::onNewGameClicked()
{
    if (mode_ == Mode::Network)
        net_->sendNewGame();
    startNewGame();
}

// ---------------------------------------------------------------------------
// Computer player
// ---------------------------------------------------------------------------

void MainWindow::onStartComputerClicked()
{
    mode_ = Mode::Computer;
    switch (difficultyCombo_->currentIndex()) {
    case 0: botLevel_ = chess::BotLevel::Easy; break;
    case 2: botLevel_ = chess::BotLevel::Hard; break;
    default: botLevel_ = chess::BotLevel::Medium; break;
    }

    chess::Color mine = chess::Color::White;
    switch (colorCombo_->currentIndex()) {
    case 1: mine = chess::Color::Black; break;
    case 2: mine = QRandomGenerator::global()->bounded(2) == 0 ? chess::Color::White
                                                               : chess::Color::Black; break;
    default: break;
    }

    playing_ = true;
    board_->setLocalColor(mine);
    setWindowTitle(tr("Chess - vs Computer (%1)").arg(difficultyCombo_->currentText()));
    pages_->setCurrentWidget(gamePage_);
    startNewGame();                               // the computer moves first if you are Black
}

void MainWindow::maybeStartBot()
{
    if (mode_ != Mode::Computer || !playing_ || resigned_ || botThread_)
        return;
    const chess::game_logic &g = board_->logic();
    if (g.isGameOver() || g.turn() == board_->localColor())
        return;                                   // game over, or it is the human's turn

    botThread_ = new bot_thread(g, botLevel_, this);
    connect(botThread_, &QThread::finished, this, &MainWindow::onBotFinished);
    botThread_->start();
}

void MainWindow::cancelBot()
{
    if (!botThread_)
        return;
    botThread_->disconnect(this);                 // we no longer care about its result
    botThread_->cancel();
    botThread_->wait();
    delete botThread_;
    botThread_ = nullptr;
}

void MainWindow::onBotFinished()
{
    bot_thread *t = botThread_;
    if (!t)
        return;
    botThread_ = nullptr;
    t->wait();
    const bool found = t->hasMove();
    const chess::Move move = t->result();
    t->deleteLater();

    if (mode_ != Mode::Computer || !playing_ || resigned_ || !found)
        return;
    board_->applyRemoteMove(move);                // the computer plays like a remote opponent
}

// ---------------------------------------------------------------------------
// Network games
// ---------------------------------------------------------------------------

void MainWindow::onHostClicked()
{
    mode_ = Mode::Network;
    setNetworkControlsEnabled(false);
    netStatusLabel_->setText(tr("Starting server..."));
    net_->host(static_cast<quint16>(portSpin_->value()));
}

void MainWindow::onJoinClicked()
{
    mode_ = Mode::Network;
    setNetworkControlsEnabled(false);
    netStatusLabel_->setText(tr("Connecting to %1:%2 ...")
                                 .arg(hostEdit_->text())
                                 .arg(portSpin_->value()));
    net_->connectTo(hostEdit_->text().trimmed(), static_cast<quint16>(portSpin_->value()));
}

void MainWindow::onListening(quint16 port)
{
    netStatusLabel_->setText(tr("Waiting for an opponent on port %1 ...\n"
                                "On the other computer choose \"Play against player\" and "
                                "\"Join a game\".")
                                 .arg(port));
}

void MainWindow::onConnected()
{
    mode_ = Mode::Network;
    playing_ = true;
    board_->setLocalColor(net_->isHost() ? chess::Color::White : chess::Color::Black);
    setWindowTitle(net_->isHost() ? tr("Chess - White (host)") : tr("Chess - Black"));
    pages_->setCurrentWidget(gamePage_);
    startNewGame();
}

void MainWindow::onDisconnected()
{
    if (!playing_ || mode_ != Mode::Network)
        return;
    connectionLost_ = true;
    board_->setInteractive(false);
    resignBtn_->setEnabled(false);
    newGameBtn_->setEnabled(false);
    undoPending_ = false;
    closeUndoDialog();
    updateUndoButton();
    gameStatusLabel_->setText(tr("The opponent disconnected.\n"
                                 "Use \"Main menu\" to start another game."));
}

void MainWindow::onNetworkError(const QString &message)
{
    if (playing_) {                               // a game was running
        connectionLost_ = true;
        board_->setInteractive(false);
        resignBtn_->setEnabled(false);
        newGameBtn_->setEnabled(false);
        undoPending_ = false;
        closeUndoDialog();
        updateUndoButton();
        gameStatusLabel_->setText(message);
        return;
    }
    netStatusLabel_->setText(message);            // failed to host / join
    setNetworkControlsEnabled(true);
}

void MainWindow::onMoveReceived(const chess::Move &move)
{
    if (mode_ != Mode::Network || !playing_ || connectionLost_ || resigned_)
        return;
    if (!board_->applyRemoteMove(move)) {
        net_->close();
        connectionLost_ = true;
        board_->setInteractive(false);
        resignBtn_->setEnabled(false);
        newGameBtn_->setEnabled(false);
        undoPending_ = false;
        closeUndoDialog();
        updateUndoButton();
        gameStatusLabel_->setText(tr("The opponent sent an illegal move. Connection closed.\n"
                                     "Use \"Main menu\" to start another game."));
    }
}

void MainWindow::onResignReceived()
{
    if (mode_ != Mode::Network || !playing_ || connectionLost_)
        return;
    resigned_ = true;
    resignText_ = tr("The opponent resigned. You win!");
    updateStatus();
}

void MainWindow::onNewGameReceived()
{
    if (mode_ != Mode::Network || !playing_ || connectionLost_)
        return;
    startNewGame();
}

// ---------------------------------------------------------------------------
// Move list, undo
// ---------------------------------------------------------------------------

void MainWindow::onBoardChanged()
{
    notice_.clear();
    capturedPanel_->refresh(board_->logic(), board_->localColor());
    moveList_->refresh(board_->logic());
    updateStatus();
}

// Undo takes the game back to the last position where it was YOUR turn:
// your own move if the opponent has not answered yet, otherwise both moves.
bool MainWindow::undoAvailable() const
{
    if (!playing_ || connectionLost_ || resigned_)
        return false;

    const chess::game_logic &g = board_->logic();
    const chess::Color me = board_->localColor();
    const int plies = (g.turn() == me) ? 2 : 1;
    if (g.moveCount() < plies)
        return false;

    if (mode_ == Mode::Computer)
        return botThread_ == nullptr;             // not while the computer is thinking

    // Network: the first move to be taken back must be ours, and no question may be open.
    const bool firstIsWhite = ((g.moveCount() - plies) % 2 == 0);
    if (firstIsWhite != (me == chess::Color::White))
        return false;
    return !undoPending_ && !undoDialog_;
}

void MainWindow::updateUndoButton()
{
    undoBtn_->setEnabled(undoAvailable());
}

void MainWindow::closeUndoDialog()
{
    if (!undoDialog_)
        return;
    QMessageBox *box = undoDialog_;
    undoDialog_ = nullptr;
    box->close();                                  // deletes itself when closed
}

void MainWindow::onUndoClicked()
{
    if (!undoAvailable())
        return;

    const chess::game_logic &g = board_->logic();
    const int plies = (g.turn() == board_->localColor()) ? 2 : 1;

    if (mode_ == Mode::Computer) {
        cancelBot();
        board_->undoPlies(plies);                  // -> onBoardChanged()
        return;
    }

    // Network game: the opponent has to agree.
    undoPending_ = true;
    undoPendingPlies_ = plies;
    net_->sendUndoRequest(g.moveCount(), plies);
    notice_ = tr("Undo requested. Waiting for the opponent's answer...");
    updateStatus();
}

void MainWindow::onUndoRequested(int plyCount, int plies)
{
    if (mode_ != Mode::Network || !playing_ || connectionLost_ || resigned_) {
        net_->sendUndoReply(false);
        return;
    }

    // Is the request still about the position we are looking at?
    const chess::game_logic &g = board_->logic();
    const chess::Color asker = chess::opposite(board_->localColor());
    const bool askerToMove = (g.turn() == asker);
    const bool firstIsWhite = ((plyCount - plies) % 2 == 0);
    const bool valid = plyCount == g.moveCount()
                       && plies == (askerToMove ? 2 : 1)
                       && plies <= plyCount
                       && firstIsWhite == (asker == chess::Color::White)
                       && !undoPending_ && !undoDialog_;
    if (!valid) {
        net_->sendUndoReply(false);
        return;
    }

    QMessageBox *box = new QMessageBox(QMessageBox::Question, tr("Take back a move"),
                                       tr("Your opponent asks to take back their last move.\n"
                                          "Do you allow it?"),
                                       QMessageBox::Yes | QMessageBox::No, this);
    box->setDefaultButton(QMessageBox::No);
    box->setAttribute(Qt::WA_DeleteOnClose);
    undoDialog_ = box;

    connect(box, &QObject::destroyed, this, [this, box] {
        if (undoDialog_ == box)                    // not a newer dialog
            undoDialog_ = nullptr;
        updateUndoButton();
    });
    connect(box, &QDialog::finished, this, [this, plyCount, plies](int result) {
        const bool stillValid = playing_ && !connectionLost_ && !resigned_
                                && mode_ == Mode::Network
                                && board_->logic().moveCount() == plyCount;
        if (result == QMessageBox::Yes && stillValid) {
            net_->sendUndoReply(true);
            board_->undoPlies(plies);
            notice_ = tr("The opponent's move was taken back.");
            updateStatus();
        } else if (net_->isConnected()) {
            net_->sendUndoReply(false);
        }
    });
    box->open();                                   // does not block the event loop
    updateUndoButton();
}

void MainWindow::onUndoAnswered(bool accepted)
{
    if (!undoPending_)
        return;
    undoPending_ = false;

    if (accepted) {
        board_->undoPlies(undoPendingPlies_);
        notice_ = tr("Your move was taken back.");
    } else {
        notice_ = tr("The opponent declined your undo request.");
    }
    updateStatus();
}
