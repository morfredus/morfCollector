/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "morfcollector/HttpServer.h"
#include "morfcollector/Collector.h"
#include "morfcollector/SelfDescription.h"
#include "morfcollector/Version.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QHostInfo>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDateTime>
#include <QUrl>
#include <QUrlQuery>

#include <utility>

namespace morfcollector {

namespace {
constexpr int kMaxRequestBytes = 1 << 20;   // 1 Mio : un manifeste peut etre gros

QByteArray toJson(const QJsonObject& o) {
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

QByteArray errorBody(const QString& msg) {
    return toJson(QJsonObject{ {"error", msg} });
}

int contentLength(const QByteArray& headerBlock) {
    for (const QByteArray& line : headerBlock.split('\n')) {
        const QByteArray l = line.trimmed();
        if (l.toLower().startsWith("content-length:"))
            return l.mid(l.indexOf(':') + 1).trimmed().toInt();
    }
    return 0;
}

bool parseJsonObject(const QByteArray& body, QJsonObject& out) {
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    out = doc.object();
    return true;
}
} // namespace

HttpServer::HttpServer(ServiceConfig config, Collector* collector, QObject* parent)
    : QObject(parent),
      m_config(std::move(config)),
      m_collector(collector),
      m_server(new QTcpServer(this)) {
    connect(m_server, &QTcpServer::newConnection, this, &HttpServer::onNewConnection);
}

HttpServer::~HttpServer() = default;

bool HttpServer::start() {
    if (m_config.httpPort == 0)
        return false;
    m_uptime.start();
    QHostAddress addr(m_config.bindAddress);
    if (addr.isNull())
        addr = QHostAddress(QHostAddress::AnyIPv4);
    return m_server->listen(addr, m_config.httpPort);
}

void HttpServer::stop()              { m_server->close(); }
bool HttpServer::isListening() const { return m_server->isListening(); }
quint16 HttpServer::port() const     { return m_server->isListening() ? m_server->serverPort() : 0; }

void HttpServer::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        QTcpSocket* sock = m_server->nextPendingConnection();
        connect(sock, &QTcpSocket::readyRead, this, [this, sock]() { onSocketReadyRead(sock); });
        connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
    }
}

void HttpServer::onSocketReadyRead(QTcpSocket* sock) {
    QByteArray buf = sock->property("buf").toByteArray();
    buf += sock->readAll();

    const int headerEnd = buf.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        if (buf.size() > kMaxRequestBytes) { sock->abort(); return; }
        sock->setProperty("buf", buf);
        return;
    }

    const QByteArray headerBlock = buf.left(headerEnd);
    const int needed = contentLength(headerBlock);
    const int bodyStart = headerEnd + 4;
    if (buf.size() - bodyStart < needed) {
        if (buf.size() > kMaxRequestBytes) { sock->abort(); return; }
        sock->setProperty("buf", buf);
        return;
    }

    const int lineEnd = buf.indexOf("\r\n");
    const QList<QByteArray> parts = buf.left(lineEnd).split(' ');
    const QByteArray method = parts.value(0);
    const QByteArray path   = parts.value(1);
    const QByteArray body   = buf.mid(bodyStart, needed);

    sock->setProperty("buf", QByteArray());
    handleRequest(sock, method, path, body);
}

void HttpServer::handleRequest(QTcpSocket* sock, const QByteArray& method,
                               const QByteArray& rawPath, const QByteArray& body) {
    const int q = rawPath.indexOf('?');
    const QString path  = QString::fromUtf8(q < 0 ? rawPath : rawPath.left(q));
    const QString query = q < 0 ? QString() : QString::fromUtf8(rawPath.mid(q + 1));

    // Cas particulier : recuperation d'un objet -> reponse BINAIRE, pas JSON.
    const QStringList seg = path.split('/', Qt::SkipEmptyParts);
    if (method == "GET" && seg.size() == 2 && seg[0] == "objects") {
        const QString objectId = QUrl::fromPercentEncoding(seg[1].toUtf8());
        QByteArray content; QString originalName;
        if (m_collector && m_collector->readObject(objectId, content, originalName)) {
            QByteArray resp;
            resp += "HTTP/1.1 200 OK\r\n";
            resp += "Content-Type: application/octet-stream\r\n";
            resp += "Content-Disposition: attachment; filename=\"" + originalName.toUtf8() + "\"\r\n";
            resp += "Content-Length: " + QByteArray::number(content.size()) + "\r\n";
            resp += "Access-Control-Allow-Origin: *\r\n";
            resp += "Connection: close\r\n\r\n";
            resp += content;
            sock->write(resp);
            // Vider le tampon d'écriture AVANT de fermer : un objet récupéré peut
            // peser bien plus que le tampon socket (~20 Ko), et `disconnectFromHost`
            // seul en tronquerait la fin. On draine jusqu'au bout, avec un délai de
            // garde pour ne jamais bloquer indéfiniment.
            while (sock->bytesToWrite() > 0)
                if (!sock->waitForBytesWritten(2000))
                    break;
            sock->disconnectFromHost();
            return;
        }
        reply(sock, 404, "Not Found", errorBody("objet inconnu"));
        return;
    }

    int        code   = 200;
    QByteArray reason = "OK";
    const QByteArray out = route(method, path, query, body, code, reason);
    reply(sock, code, reason, out);
}

QByteArray HttpServer::route(const QByteArray& method, const QString& path,
                             const QString& query, const QByteArray& body,
                             int& code, QByteArray& reason) {
    const auto set = [&](int c, const char* r) { code = c; reason = r; };

    // Segments non vides : "/sources/{id}/collect" -> [sources, {id}, collect].
    const QStringList seg = path.split('/', Qt::SkipEmptyParts);

    // ---- Cadre --------------------------------------------------------------
    if (method == "GET" && path == "/healthz")
        return QByteArray("{\"status\":\"ok\"}");
    if (method == "GET" && path == "/status")
        return buildStatusJson();

    // ---- Manifeste ----------------------------------------------------------
    if (path == "/manifest") {
        if (method != "POST") { set(405, "Method Not Allowed"); return errorBody("use POST /manifest"); }
        QJsonObject root;
        if (!parseJsonObject(body, root)) { set(400, "Bad Request"); return errorBody("corps JSON invalide"); }
        int hc = 200; QJsonObject resp;
        m_collector->applyManifest(root, hc, resp);
        set(hc, hc == 200 ? "OK" : (hc == 409 ? "Conflict" : "Unprocessable Entity"));
        return toJson(resp);
    }
    if (seg.size() == 2 && seg[0] == "manifest" && seg[1] == "state") {
        if (method != "GET") { set(405, "Method Not Allowed"); return errorBody("use GET /manifest/state"); }
        const QString instance = QUrlQuery(query).queryItemValue(
            QStringLiteral("instance"), QUrl::FullyDecoded);
        QJsonObject out;
        if (instance.isEmpty()) { set(400, "Bad Request"); return errorBody("parametre 'instance' requis"); }
        if (!m_collector->manifestState(instance, out)) { set(404, "Not Found"); return errorBody("fournisseur inconnu"); }
        return toJson(out);
    }

    // ---- Coffre de secrets --------------------------------------------------
    if (path == "/credentials") {
        if (method != "POST") { set(405, "Method Not Allowed"); return errorBody("use POST /credentials"); }
        QJsonObject root;
        if (!parseJsonObject(body, root)) { set(400, "Bad Request"); return errorBody("corps JSON invalide"); }
        const QString ref = root.value("ref").toString();
        const QJsonObject secret = root.value("secret").toObject();
        // Coffre indisponible (dossier non accessible en ecriture, chiffrement
        // absent) : distinguer d'une requete mal formee pour ne pas masquer un
        // probleme de deploiement derriere un 400 trompeur.
        if (!m_collector->vaultReady()) {
            set(503, "Service Unavailable");
            return errorBody("coffre de secrets indisponible : verifier 'vault_root' "
                             "et les droits d'ecriture du service");
        }
        if (ref.isEmpty() || secret.isEmpty()) {
            set(400, "Bad Request"); return errorBody("'ref' et 'secret' requis");
        }
        if (!m_collector->storeCredentials(ref, secret)) {
            set(500, "Internal Server Error");
            return errorBody("ecriture du secret impossible");
        }
        set(204, "No Content");
        return QByteArray();
    }

    // ---- Objets -------------------------------------------------------------
    if (seg.size() == 2 && seg[0] == "objects") {
        // GET /objects/{id} est servi en binaire dans handleRequest (avant route).
        const QString objectId = QUrl::fromPercentEncoding(seg[1].toUtf8());
        if (method == "DELETE") {
            if (!m_collector->deleteObject(objectId)) { set(404, "Not Found"); return errorBody("objet inconnu"); }
            return toJson(QJsonObject{ {"status", "deleted"}, {"object_id", objectId} });
        }
        set(405, "Method Not Allowed");
        return errorBody("method not allowed");
    }

    // ---- Sources ------------------------------------------------------------
    if (!seg.isEmpty() && seg[0] == "sources") {
        // GET /sources
        if (seg.size() == 1) {
            if (method != "GET") { set(405, "Method Not Allowed"); return errorBody("use GET /sources"); }
            return toJson(m_collector->sourcesEnvelope());
        }
        const QString id = QUrl::fromPercentEncoding(seg[1].toUtf8());

        // /sources/{id}
        if (seg.size() == 2) {
            if (method == "GET") {
                QJsonObject out;
                if (!m_collector->sourceDetail(id, out)) { set(404, "Not Found"); return errorBody("source inconnue"); }
                return toJson(out);
            }
            if (method == "DELETE") {
                switch (m_collector->deleteSource(id)) {
                    case Collector::Op::Done:     return toJson(QJsonObject{ {"status", "deleted"}, {"source_id", id} });
                    case Collector::Op::Conflict: set(409, "Conflict"); return errorBody("source non retiree : suppression refusee");
                    case Collector::Op::NotFound: set(404, "Not Found"); return errorBody("source inconnue");
                }
            }
            set(405, "Method Not Allowed");
            return errorBody("method not allowed");
        }

        // /sources/{id}/<action>
        if (seg.size() == 3) {
            const QString action = seg[2];
            if (method == "GET" && action == "periods") {
                QJsonArray arr;
                if (!m_collector->periods(id, arr)) { set(404, "Not Found"); return errorBody("source inconnue"); }
                return toJson(QJsonObject{ {"periods", arr} });
            }
            if (method == "GET" && action == "objects") {
                QJsonArray arr;
                if (!m_collector->objects(id, arr)) { set(404, "Not Found"); return errorBody("source inconnue"); }
                return toJson(QJsonObject{ {"objects", arr} });
            }
            if (method == "DELETE" && action == "objects") {
                int removed = 0;
                switch (m_collector->deleteSourceObjects(id, removed)) {
                    case Collector::Op::Done:     return toJson(QJsonObject{ {"status", "deleted"}, {"removed", removed} });
                    case Collector::Op::NotFound: set(404, "Not Found"); return errorBody("source inconnue");
                    case Collector::Op::Conflict: set(409, "Conflict"); return errorBody("conflit");
                }
            }
            if (method == "POST" && action == "collect") {
                QJsonObject out;
                switch (m_collector->collectNow(id, out)) {
                    case Collector::Op::Done:     set(202, "Accepted"); return toJson(out);
                    case Collector::Op::Conflict: set(409, "Conflict"); return toJson(out);
                    case Collector::Op::NotFound: set(404, "Not Found"); return errorBody("source inconnue");
                }
            }
            if (method == "POST" && action == "suspend") {
                if (!m_collector->suspend(id)) { set(404, "Not Found"); return errorBody("source inconnue"); }
                QJsonObject out; m_collector->sourceDetail(id, out);
                return toJson(out);
            }
            if (method == "POST" && action == "resume") {
                if (!m_collector->resume(id)) { set(404, "Not Found"); return errorBody("source inconnue"); }
                QJsonObject out; m_collector->sourceDetail(id, out);
                return toJson(out);
            }
            set(405, "Method Not Allowed");
            return errorBody("method not allowed");
        }
    }

    set(404, "Not Found");
    return errorBody("not found");
}

QByteArray HttpServer::buildStatusJson() const {
    QJsonObject o;
    o["app"]      = m_config.appName;
    o["host"]     = QHostInfo::localHostName();
    o["version"]  = morfcollector::version();
    o["proto"]    = QString::fromLatin1(morfcollector::kProtocol);
    o["state"]    = m_collector ? m_collector->state() : QStringLiteral("ok");
    o["uptime_s"] = static_cast<double>(m_uptime.isValid() ? m_uptime.elapsed() / 1000 : 0);
    o["ts"]       = static_cast<double>(QDateTime::currentSecsSinceEpoch());
    o["metrics"]  = m_collector ? m_collector->metrics() : QJsonObject{};

    // Contrat supporte : un fournisseur verifie la compatibilite AVANT de pousser
    // (CONTRAT.md §7.2). Cle optionnelle de premier niveau, additive au sens
    // morfbeacon/1.
    o["collect"] = QJsonObject{
        {"proto", QJsonArray{ QString::fromLatin1(kManifestProto) }},
        {"manifest_endpoint", QStringLiteral("/manifest")},
        {"state_endpoint", QStringLiteral("/manifest/state")},
    };

    // Ce que ce build sait reellement faire (connecteurs enregistres). Honnete :
    // vide tant que le plan de donnees n'est pas branche.
    QJsonArray caps;
    if (m_collector)
        for (const QString& c : m_collector->capabilities())
            caps.append(c);
    o["capabilities"] = caps;

    // Detail annonce (bloc `api`) depuis le point UNIQUE.
    morfbeacon::PresenceConfig self;
    fillAnnouncedDetail(self);
    const QJsonObject detail = morfbeacon::describeService(self, port());
    for (auto it = detail.constBegin(); it != detail.constEnd(); ++it)
        o[it.key()] = it.value();

    return toJson(o);
}

void HttpServer::reply(QTcpSocket* sock, int code, const QByteArray& reason, const QByteArray& body) {
    QByteArray resp;
    resp += "HTTP/1.1 " + QByteArray::number(code) + " " + reason + "\r\n";
    resp += "Content-Type: application/json; charset=utf-8\r\n";
    resp += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    resp += "Access-Control-Allow-Origin: *\r\n";
    resp += "Connection: close\r\n\r\n";
    resp += body;
    sock->write(resp);
    // Vider le tampon d'écriture AVANT de fermer : sur une grande réponse (page HTML,
    // /status volumineux), le corps déborde du tampon socket (~20 Ko constaté) et
    // `disconnectFromHost` seul en tronque la fin côté client. On draine jusqu'à ce
    // qu'il ne reste rien à écrire, avec un délai de garde pour ne jamais bloquer.
    while (sock->bytesToWrite() > 0)
        if (!sock->waitForBytesWritten(2000))
            break;
    sock->disconnectFromHost();
}

} // namespace morfcollector
