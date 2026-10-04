#include "network_manager.h"

#include <QHostAddress>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>

network_manager::network_manager(QObject *parent)
    : QObject(parent)
{
}

network_manager::~network_manager()
{
    close();
}

bool network_manager::isConnected() const
{
    return socket_ && socket_->state() == QAbstractSocket::ConnectedState;
}

void network_manager::close()
{
    if (server_) {
        server_->close();
        server_->deleteLater();
        server_ = nullptr;
    }
    if (socket_) {
        socket_->disconnect(this);      // no more signals from it
        socket_->abort();
        socket_->deleteLater();
        socket_ = nullptr;
    }
    established_ = false;
}

void network_manager::host(quint16 port)
{
    close();
    isHost_ = true;

    server_ = new QTcpServer(this);
    connect(server_, &QTcpServer::newConnection, this, &network_manager::onNewConnection);

    if (!server_->listen(QHostAddress::Any, port)) {
        const QString reason = server_->errorString();
        close();
        emit errorOccurred(tr("Cannot listen on port %1: %2").arg(port).arg(reason));
        return;
    }
    emit listening(port);
}

void network_manager::connectTo(const QString &address, quint16 port)
{
    close();
    isHost_ = false;

    QTcpSocket *s = new QTcpSocket(this);
    attachSocket(s);
    connect(s, &QTcpSocket::connected, this, &network_manager::onSocketConnected);
    s->connectToHost(address, port);
}

void network_manager::attachSocket(QTcpSocket *s)
{
    socket_ = s;
    s->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    connect(s, &QTcpSocket::readyRead, this, &network_manager::onReadyRead);
    connect(s, &QTcpSocket::disconnected, this, &network_manager::onSocketDisconnected);
    connect(s, &QAbstractSocket::errorOccurred, this, &network_manager::onSocketError);
}

void network_manager::onNewConnection()
{
    if (!server_)
        return;
    QTcpSocket *s = server_->nextPendingConnection();
    if (!s)
        return;

    if (socket_) {                       // we already have an opponent
        s->close();
        s->deleteLater();
        return;
    }

    server_->close();                    // one opponent only
    s->setParent(this);                  // do not die together with the server
    attachSocket(s);
    established_ = true;
    emit connected();
}

void network_manager::onSocketConnected()
{
    established_ = true;
    emit connected();
}

void network_manager::onSocketDisconnected()
{
    if (!socket_)
        return;
    QTcpSocket *s = socket_;
    socket_ = nullptr;
    established_ = false;
    s->deleteLater();
    emit disconnected();
}

void network_manager::onSocketError(QAbstractSocket::SocketError)
{
    if (!socket_ || established_)
        return;                          // after the link was up, disconnected() reports it

    const QString message = tr("Connection failed: %1").arg(socket_->errorString());
    QTcpSocket *s = socket_;
    socket_ = nullptr;
    s->disconnect(this);
    s->abort();
    s->deleteLater();
    emit errorOccurred(message);
}

void network_manager::onReadyRead()
{
    while (socket_ && socket_->canReadLine()) {
        const QString line = QString::fromUtf8(socket_->readLine()).trimmed();
        const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty())
            continue;

        if (parts[0] == "MOVE" && parts.size() == 6) {
            bool ok[5];
            const int fr = parts[1].toInt(&ok[0]);
            const int fc = parts[2].toInt(&ok[1]);
            const int tr = parts[3].toInt(&ok[2]);
            const int tc = parts[4].toInt(&ok[3]);
            const int pr = parts[5].toInt(&ok[4]);
            bool valid = true;
            for (bool b : ok) valid = valid && b;
            valid = valid && fr >= 0 && fr < 8 && fc >= 0 && fc < 8
                          && tr >= 0 && tr < 8 && tc >= 0 && tc < 8 && pr >= 0 && pr <= 6;
            if (!valid)
                continue;                // ignore garbage
            emit moveReceived(chess::Move{fr, fc, tr, tc, static_cast<chess::PieceType>(pr)});
        } else if (parts[0] == "RESIGN") {
            emit resignReceived();
        } else if (parts[0] == "NEWGAME") {
            emit newGameReceived();
        }
    }
}

void network_manager::sendLine(const QString &line)
{
    if (!isConnected())
        return;
    socket_->write(line.toUtf8() + '\n');
}

void network_manager::sendMove(const chess::Move &m)
{
    sendLine(QStringLiteral("MOVE %1 %2 %3 %4 %5")
                 .arg(m.fromRow).arg(m.fromCol).arg(m.toRow).arg(m.toCol)
                 .arg(static_cast<int>(m.promotion)));
}

void network_manager::sendResign()
{
    sendLine(QStringLiteral("RESIGN"));
}

void network_manager::sendNewGame()
{
    sendLine(QStringLiteral("NEWGAME"));
}
