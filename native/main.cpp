#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <unistd.h>
#include <QThread>
#include <climits>

static QString socketPath() { return QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/caffeine-timer-" + QString::number(getuid()) + ".sock"; }
static QString output(const QJsonObject &o) { return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)); }

class Server : public QObject {
    Q_OBJECT
    QLocalServer server;
    QTimer timer;
    uint powerCookie = 0, screenCookie = 0;
    qint64 until = 0;
    bool display = true;
    bool active = false;
    bool inhibit(const QString &service, const QString &path, uint &cookie) {
        QDBusInterface iface(service, path, service == "org.freedesktop.ScreenSaver" ? "org.freedesktop.ScreenSaver" : "org.freedesktop.PowerManagement.Inhibit", QDBusConnection::sessionBus());
        auto msg = iface.call("Inhibit", "Caffeine Timer", "Timed awake mode");
        if (msg.type() != QDBusMessage::ReplyMessage || msg.arguments().isEmpty()) return false;
        cookie = msg.arguments().first().toUInt();
        return true;
    }
    void uninhibit(const QString &service, const QString &path, uint &cookie) {
        if (!cookie) return;
        QDBusInterface iface(service, path, service == "org.freedesktop.ScreenSaver" ? "org.freedesktop.ScreenSaver" : "org.freedesktop.PowerManagement.Inhibit", QDBusConnection::sessionBus());
        iface.call("UnInhibit", cookie);
        cookie = 0;
    }
    void stop() {
        uninhibit("org.freedesktop.ScreenSaver", "/ScreenSaver", screenCookie);
        uninhibit("org.freedesktop.PowerManagement", "/org/freedesktop/PowerManagement/Inhibit", powerCookie);
        active = false; until = 0; timer.stop();
    }
    QJsonObject status() const { return {{"active",active},{"until",double(until)},{"mode",display ? "full" : "sleep"}}; }
    QJsonObject command(const QJsonObject &req) {
        QString cmd = req.value("command").toString();
        if (cmd == "status") return status();
        if (cmd == "stop") { stop(); QTimer::singleShot(150, qApp, &QCoreApplication::quit); return status(); }
        if (cmd != "start") return {{"error","Unknown command"}};
        stop();
        display = req.value("display").toBool(true);
        int minutes = req.value("minutes").toInt(60);
        if (minutes < 0 || minutes > 10080) return {{"error","Invalid duration"}};
        if (!inhibit("org.freedesktop.PowerManagement", "/org/freedesktop/PowerManagement/Inhibit", powerCookie))
            return {{"error","KDE PowerManagement D-Bus inhibition unavailable"}};
        if (display && !inhibit("org.freedesktop.ScreenSaver", "/ScreenSaver", screenCookie)) {
            stop(); return {{"error","ScreenSaver D-Bus inhibition unavailable"}};
        }
        active = true;
        until = minutes ? QDateTime::currentSecsSinceEpoch() + qint64(minutes)*60 : 0;
        if (until) timer.start(int(qMin<qint64>(qint64(minutes)*60000, INT_MAX)));
        return status();
    }
public:
    Server() {
        connect(&timer, &QTimer::timeout, this, [this] { stop(); qApp->quit(); });
        timer.setSingleShot(true);
        QFile::remove(socketPath());
        connect(&server, &QLocalServer::newConnection, this, [this] {
            while (auto *sock = server.nextPendingConnection()) {
                connect(sock, &QLocalSocket::readyRead, sock, [this,sock] {
                    auto doc = QJsonDocument::fromJson(sock->readAll());
                    auto response = command(doc.object());
                    sock->write(output(response).toUtf8() + '\n'); sock->flush();
                    sock->disconnectFromServer();
                });
                connect(sock, &QLocalSocket::disconnected, sock, &QObject::deleteLater);
            }
        });
        if (!server.listen(socketPath())) { QTextStream(stderr) << "Cannot listen on socket\n"; QTimer::singleShot(0,qApp,&QCoreApplication::quit); }
    }
    ~Server() { stop(); QFile::remove(socketPath()); }
};

static QJsonObject send(const QJsonObject &request) {
    QLocalSocket sock;
    sock.connectToServer(socketPath());
    if (!sock.waitForConnected(500)) return {{"error","Not running"},{"active",false}};
    sock.write(QJsonDocument(request).toJson(QJsonDocument::Compact)); sock.flush();
    if (!sock.waitForReadyRead(1500)) return {{"error","Timeout"}};
    return QJsonDocument::fromJson(sock.readAll()).object();
}

int main(int argc, char **argv) {
    QCoreApplication app(argc,argv);
    QStringList args = app.arguments();
    if (args.contains("--serve")) { Server server; return app.exec(); }
    QString cmd = args.value(1,"status");
    QJsonObject request{{"command",cmd}};
    if (cmd == "start") {
        request["minutes"] = args.value(2,"60").toInt();
        request["display"] = args.value(3,"full") == "full";
    }
    auto result = send(request);
    if (cmd == "start" && result.value("error").toString() == "Not running") {
        if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), {"--serve"})) {
            QTextStream(stdout) << output({{"error","Cannot start helper"}}) << '\n'; return 1;
        }
        for (int i=0; i<15; ++i) {
            QThread::msleep(100);
            result = send(request);
            if (result.value("error").toString() != "Not running") break;
        }
    }
    if (cmd == "status" && result.value("error").toString() == "Not running") result = {{"active",false},{"until",0},{"mode","full"}};
    if (cmd == "stop" && result.value("error").toString() == "Not running") result = {{"active",false},{"until",0},{"mode","full"}};
    QTextStream(stdout) << output(result) << '\n';
    return result.contains("error") ? 1 : 0;
}
#include "main.moc"
