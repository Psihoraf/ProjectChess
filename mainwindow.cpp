#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "chess_table.h"
#include "network_manager.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(tr("Chess"));
    resize(1000, 760);

    board_ = new chess_table(this);
    net_ = new network_manager(this);

    // ---- side panel -------------------------------------------------------
    QWidget *side = new QWidget(this);
    side->setFixedWidth(270);
    QVBoxLayout *sideLayout = new QVBoxLayout(side);

    QGroupBox *connBox = new QGroupBox(tr("Network game"), side);
    QFormLayout *form = new QFormLayout(connBox);
    hostEdit_ = new QLineEdit(QStringLiteral("127.0.0.1"), connBox);
    portSpin_ = new QSpinBox(connBox);
    portSpin_->setRange(1024, 65535);
    portSpin_->setValue(5555);
    form->addRow(tr("Address:"), hostEdit_);
    form->addRow(tr("Port:"), portSpin_);

    hostBtn_ = new QPushButton(tr("Host game (play White)"), connBox);
    joinBtn_ = new QPushButton(tr("Join game (play Black)"), connBox);
    disconnectBtn_ = new QPushButton(tr("Disconnect"), connBox);
    form->addRow(hostBtn_);
    form->addRow(joinBtn_);
    form->addRow(disconnectBtn_);

    resignBtn_ = new QPushButton(tr("Resign"), side);
    newGameBtn_ = new QPushButton(tr("New game"), side);

    statusLabel_ = new QLabel(tr("Host a game, or join one."), side);
    statusLabel_->setWordWrap(true);
    statusLabel_->setMinimumHeight(80);
    statusLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    sideLayout->addWidget(connBox);
    sideLayout->addWidget(resignBtn_);
    sideLayout->addWidget(newGameBtn_);
    sideLayout->addWidget(statusLabel_);
    sideLayout->addStretch(1);

    QHBoxLayout *layout = new QHBoxLayout(ui->centralwidget);
    layout->addWidget(board_, 1);
    layout->addWidget(side);

    // ---- connections ------------------------------------------------------
    connect(hostBtn_, &QPushButton::clicked, this, &MainWindow::onHostClicked);
    connect(joinBtn_, &QPushButton::clicked, this, &MainWindow::onJoinClicked);
    connect(disconnectBtn_, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(resignBtn_, &QPushButton::clicked, this, &MainWindow::onResignClicked);
    connect(newGameBtn_, &QPushButton::clicked, this, &MainWindow::onNewGameClicked);

    connect(board_, &chess_table::moveMade, net_, &network_manager::sendMove);
    connect(board_, &chess_table::positionChanged, this, &MainWindow::updateStatus);

    connect(net_, &network_manager::listening, this, &MainWindow::onListening);
    connect(net_, &network_manager::connected, this, &MainWindow::onConnected);
    connect(net_, &network_manager::disconnected, this, &MainWindow::onDisconnected);
    connect(net_, &network_manager::errorOccurred, this, &MainWindow::onNetworkError);
    connect(net_, &network_manager::moveReceived, this, &MainWindow::onMoveReceived);
    connect(net_, &network_manager::resignReceived, this, &MainWindow::onResignReceived);
    connect(net_, &network_manager::newGameReceived, this, &MainWindow::onNewGameReceived);

    setLinkState(LinkState::Idle);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ---------------------------------------------------------------------------
// UI state
// ---------------------------------------------------------------------------

void MainWindow::setLinkState(LinkState state)
{
    state_ = state;
    const bool idle = (state == LinkState::Idle);

    hostBtn_->setEnabled(idle);
    joinBtn_->setEnabled(idle);
    hostEdit_->setEnabled(idle);
    portSpin_->setEnabled(idle);
    disconnectBtn_->setEnabled(!idle);
    disconnectBtn_->setText(state == LinkState::Waiting ? tr("Cancel") : tr("Disconnect"));

    if (state != LinkState::Playing) {
        resignBtn_->setEnabled(false);
        newGameBtn_->setEnabled(false);
        board_->setInteractive(false);
        setWindowTitle(tr("Chess"));
    }
}

void MainWindow::startNewGame()
{
    resigned_ = false;
    resignText_.clear();
    board_->newGame();            // also emits positionChanged -> updateStatus()
    board_->setInteractive(true);
    updateStatus();
}

void MainWindow::updateStatus()
{
    if (state_ != LinkState::Playing)
        return;

    using chess::GameStatus;
    const chess::game_logic &g = board_->logic();
    const chess::Color me = board_->localColor();

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
            text = (g.turn() == me) ? tr("Your turn.") : tr("Waiting for the opponent...");
            if (g.status() == GameStatus::Check)
                text += tr(" Check!");
            break;
        }
    }

    const QString who = (me == chess::Color::White) ? tr("You play White.") : tr("You play Black.");
    statusLabel_->setText(who + QLatin1Char('\n') + text);

    board_->setInteractive(!over);
    resignBtn_->setEnabled(!over);
    newGameBtn_->setEnabled(over);
}

// ---------------------------------------------------------------------------
// Button handlers
// ---------------------------------------------------------------------------

void MainWindow::onHostClicked()
{
    setLinkState(LinkState::Waiting);
    statusLabel_->setText(tr("Starting server..."));
    net_->host(static_cast<quint16>(portSpin_->value()));
}

void MainWindow::onJoinClicked()
{
    setLinkState(LinkState::Waiting);
    statusLabel_->setText(tr("Connecting to %1:%2 ...")
                              .arg(hostEdit_->text())
                              .arg(portSpin_->value()));
    net_->connectTo(hostEdit_->text().trimmed(), static_cast<quint16>(portSpin_->value()));
}

void MainWindow::onDisconnectClicked()
{
    net_->close();
    setLinkState(LinkState::Idle);
    statusLabel_->setText(tr("Disconnected."));
}

void MainWindow::onResignClicked()
{
    net_->sendResign();
    resigned_ = true;
    resignText_ = tr("You resigned. You lose.");
    updateStatus();
}

void MainWindow::onNewGameClicked()
{
    net_->sendNewGame();
    startNewGame();
}

// ---------------------------------------------------------------------------
// Network events
// ---------------------------------------------------------------------------

void MainWindow::onListening(quint16 port)
{
    statusLabel_->setText(tr("Waiting for an opponent on 127.0.0.1:%1 ...\n"
                             "Start a second copy of the program and press \"Join game\".")
                              .arg(port));
}

void MainWindow::onConnected()
{
    setLinkState(LinkState::Playing);
    board_->setLocalColor(net_->isHost() ? chess::Color::White : chess::Color::Black);
    setWindowTitle(net_->isHost() ? tr("Chess - White (host)") : tr("Chess - Black"));
    startNewGame();
}

void MainWindow::onDisconnected()
{
    if (state_ == LinkState::Idle)
        return;
    setLinkState(LinkState::Idle);
    statusLabel_->setText(tr("The opponent disconnected."));
}

void MainWindow::onNetworkError(const QString &message)
{
    setLinkState(LinkState::Idle);
    statusLabel_->setText(message);
}

void MainWindow::onMoveReceived(const chess::Move &move)
{
    if (state_ != LinkState::Playing || resigned_)
        return;
    if (!board_->applyRemoteMove(move)) {
        net_->close();
        setLinkState(LinkState::Idle);
        statusLabel_->setText(tr("The opponent sent an illegal move. Connection closed."));
    }
}

void MainWindow::onResignReceived()
{
    if (state_ != LinkState::Playing)
        return;
    resigned_ = true;
    resignText_ = tr("The opponent resigned. You win!");
    updateStatus();
}

void MainWindow::onNewGameReceived()
{
    if (state_ != LinkState::Playing)
        return;
    startNewGame();
}
