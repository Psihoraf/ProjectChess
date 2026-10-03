#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <QAbstractSocket>
#include <QObject>
#include <QString>

#include "game_logic.h"

class QTcpServer;
class QTcpSocket;

// One TCP connection between two players, using a tiny line protocol:
//   MOVE <fromRow> <fromCol> <toRow> <toCol> <promotion 0..6>
//   RESIGN
//   NEWGAME
// The host listens on 127.0.0.1 and plays White; the joiner plays Black.
class network_manager : public QObject
{
    Q_OBJECT

public:
    explicit network_manager(QObject *parent = nullptr);
    ~network_manager() override;

    void host(quint16 port);                          // start listening (localhost only)
    void connectTo(const QString &address, quint16 port);
    void close();                                     // stop listening / drop the connection

    bool isHost() const { return isHost_; }
    bool isConnected() const;

    void sendMove(const chess::Move &move);
    void sendResign();
    void sendNewGame();

signals:
    void listening(quint16 port);
    void connected();                                 // the opponent is there, play can start
    void disconnected();                              // the opponent went away
    void moveReceived(const chess::Move &move);
    void resignReceived();
    void newGameReceived();
    void errorOccurred(const QString &message);

private slots:
    void onNewConnection();
    void onSocketConnected();
    void onReadyRead();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    void attachSocket(QTcpSocket *socket);
    void sendLine(const QString &line);

    QTcpServer *server_ = nullptr;
    QTcpSocket *socket_ = nullptr;
    bool isHost_ = false;
    bool established_ = false;
};

#endif // NETWORK_MANAGER_H
