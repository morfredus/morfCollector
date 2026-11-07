/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Adapte de SiteWatch/SftpClient (code libssh2 eprouve). Non testable en local
 * (reseau + hebergeur) : la rigueur vient de la reutilisation d'un code de
 * production, pas d'un test unitaire.
 */

#include "morfcollector/SftpConnector.h"
#include "morfcollector/Guards.h"

// --- Couche socket portable (Windows Winsock / POSIX) -----------------------
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using socket_t = SOCKET;
  static constexpr socket_t kInvalidSocket = INVALID_SOCKET;
  static inline void closeSocket(socket_t s) { closesocket(s); }
#else
  #include <sys/socket.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <sys/select.h>
  #include <cerrno>
  using socket_t = int;
  static constexpr socket_t kInvalidSocket = -1;
  static inline void closeSocket(socket_t s) { ::close(s); }
#endif

#include <libssh2.h>
#include <libssh2_sftp.h>

#include <QByteArray>
#include <QFileInfo>
#include <QRegularExpression>
#include <QString>

#include <QDebug>

#include <cstring>

namespace morfcollector {

namespace {

constexpr int    kConnectTimeoutMs = 10000;               // connexion TCP
constexpr long   kSessionTimeoutMs = 30000;               // chaque operation libssh2
constexpr qint64 kMaxFileBytes     = 256LL * 1024 * 1024; // plafond de lecture en memoire

// connect() avec delai : passage temporaire en non bloquant + select(). Sans cela,
// un hote qui ne repond pas gele le thread de travail (et l'arret du service)
// pendant le delai TCP systeme.
bool connectWithTimeout(socket_t s, const struct sockaddr* addr, socklen_t len, int timeoutMs) {
#ifdef _WIN32
    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
    const int rc = ::connect(s, addr, len);
    bool ok = (rc == 0);
    if (rc != 0 && WSAGetLastError() == WSAEWOULDBLOCK) {
        fd_set wr, ex;
        FD_ZERO(&wr); FD_ZERO(&ex); FD_SET(s, &wr); FD_SET(s, &ex);
        timeval tv{ timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
        ok = (select(0, nullptr, &wr, &ex, &tv) > 0) && FD_ISSET(s, &wr);
    }
    nb = 0;
    ioctlsocket(s, FIONBIO, &nb);
    return ok;
#else
    const int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
    const int rc = ::connect(s, addr, len);
    bool ok = (rc == 0);
    if (rc != 0 && errno == EINPROGRESS) {
        fd_set wr;
        FD_ZERO(&wr); FD_SET(s, &wr);
        timeval tv{ timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
        if (select(s + 1, nullptr, &wr, nullptr, &tv) > 0) {
            int soErr = 0;
            socklen_t el = sizeof(soErr);
            ok = (getsockopt(s, SOL_SOCKET, SO_ERROR, &soErr, &el) == 0 && soErr == 0);
        }
    }
    fcntl(s, F_SETFL, flags);
    return ok;
#endif
}

// Correspondance d'un fichier a la source (reprise de SiteWatch/LogDiscovery) :
//   - `match` non vide  : le nom contient le motif (insensible a la casse) ;
//   - sinon `siteKey`   : prefixe (avant le 1er point) == siteKey, points retires.
QString keyWithoutDots(const QString& s) {
    QString r;
    for (const QChar c : s)
        if (c != '.') r.append(c.toLower());
    return r;
}
bool belongsToSource(const QString& filename, const QString& match, const QString& siteKey) {
    if (!match.isEmpty())
        return filename.contains(match, Qt::CaseInsensitive);
    if (siteKey.isEmpty())
        return true;
    const QString prefix = filename.left(filename.indexOf('.'));
    return keyWithoutDots(prefix) == keyWithoutDots(siteKey);
}

QString periodFromName(const QString& name) {
    static const QRegularExpression re(QStringLiteral("(20\\d{2})[-_]?(\\d{2})[-_]?(\\d{2})"));
    const QRegularExpressionMatch m = re.match(name);
    if (!m.hasMatch())
        return QString();
    const int mm = m.captured(2).toInt();
    const int dd = m.captured(3).toInt();
    if (mm < 1 || mm > 12 || dd < 1 || dd > 31)
        return QString();
    return QStringLiteral("%1-%2-%3").arg(m.captured(1), m.captured(2), m.captured(3));
}

// Session SFTP a usage unique : connexion + authentification + canal SFTP. Toute
// la matiere bas niveau reste confinee ici.
class Session {
public:
    ~Session() { close(); }

    // Connecte et authentifie. Renseigne `outcome`/`error` en cas d'echec.
    bool open(const QJsonObject& params, const QJsonObject& creds,
              IConnector::Outcome& outcome, QString& error) {
        const std::string host = params.value(QStringLiteral("host")).toString().toStdString();
        const int port = params.value(QStringLiteral("port")).toInt(22);
        const std::string portStr = std::to_string(port);

#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            outcome = IConnector::Outcome::Error; error = QStringLiteral("WSAStartup impossible");
            return false;
        }
        wsaInit_ = true;
#endif
        struct addrinfo hints;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family   = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo* res = nullptr;
        if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
            outcome = IConnector::Outcome::Unreachable;
            error = QStringLiteral("resolution DNS echouee : %1").arg(QString::fromStdString(host));
            return false;
        }
        socket_t s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (s == kInvalidSocket) {
            freeaddrinfo(res);
            outcome = IConnector::Outcome::Error; error = QStringLiteral("socket impossible");
            return false;
        }
        if (!connectWithTimeout(s, res->ai_addr, static_cast<socklen_t>(res->ai_addrlen),
                                kConnectTimeoutMs)) {
            freeaddrinfo(res); closeSocket(s);
            outcome = IConnector::Outcome::Unreachable;
            error = QStringLiteral("connexion TCP impossible vers %1:%2")
                        .arg(QString::fromStdString(host)).arg(port);
            return false;
        }
        freeaddrinfo(res);
        sock_ = s;

        session_ = libssh2_session_init();
        if (!session_) { outcome = IConnector::Outcome::Error; error = QStringLiteral("session SSH impossible"); return false; }
        libssh2_session_set_blocking(session_, 1);
        // Delai sur chaque operation bloquante (handshake, auth, lecture) : un serveur
        // qui se tait ne doit pas figer le thread de travail indefiniment.
        libssh2_session_set_timeout(session_, kSessionTimeoutMs);
        if (libssh2_session_handshake(session_, s) != 0) {
            outcome = IConnector::Outcome::Unreachable; error = QStringLiteral("handshake SSH echoue");
            return false;
        }

        // Cle d'hote verifiee AVANT l'authentification : sinon le mot de passe part
        // vers n'importe quel faux serveur place sur le chemin.
        if (!checkHostKey(params, error)) {
            outcome = IConnector::Outcome::AuthFailed;
            return false;
        }

        if (!authenticate(creds, error)) {
            outcome = IConnector::Outcome::AuthFailed;
            return false;
        }

        sftp_ = libssh2_sftp_init(session_);
        if (!sftp_) { outcome = IConnector::Outcome::Error; error = QStringLiteral("canal SFTP impossible"); return false; }
        return true;
    }

    // Liste les .gz du dossier avec leur taille.
    bool listGz(const QString& dir, QVector<QPair<QString, quint64>>& out, QString& error) {
        LIBSSH2_SFTP_HANDLE* handle = libssh2_sftp_opendir(sftp_, dir.toStdString().c_str());
        if (!handle) { error = QStringLiteral("ouverture du dossier distant impossible : %1").arg(dir); return false; }
        char name[512];
        LIBSSH2_SFTP_ATTRIBUTES attrs;
        ssize_t rc;
        while ((rc = libssh2_sftp_readdir(handle, name, sizeof(name), &attrs)) > 0) {
            const QString fn = QString::fromUtf8(name, rc);
            if (fn.endsWith(QStringLiteral(".gz"))) {
                const quint64 size = (attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) ? attrs.filesize : 0;
                out.push_back({ fn, size });
            }
        }
        libssh2_sftp_closedir(handle);
        // rc < 0 : la liste s'est interrompue en route. L'accepter reviendrait a
        // croire que des fichiers n'existent pas ; on remonte l'erreur.
        if (rc < 0) {
            error = QStringLiteral("erreur de lecture du dossier distant : %1").arg(dir);
            return false;
        }
        return true;
    }

    // Lit un fichier distant entierement en memoire.
    bool read(const QString& remotePath, QByteArray& out, QString& error) {
        LIBSSH2_SFTP_HANDLE* handle =
            libssh2_sftp_open(sftp_, remotePath.toStdString().c_str(), LIBSSH2_FXF_READ, 0);
        if (!handle) { error = QStringLiteral("ouverture distante impossible : %1").arg(remotePath); return false; }
        char buffer[32768];
        ssize_t n;
        while ((n = libssh2_sftp_read(handle, buffer, sizeof(buffer))) > 0) {
            // Plafond : tout le fichier est garde en memoire, un fichier enorme ou
            // infini ne doit pas epuiser la RAM du Pi.
            if (out.size() + static_cast<qint64>(n) > kMaxFileBytes) {
                libssh2_sftp_close(handle);
                out.clear();
                error = QStringLiteral("fichier trop volumineux (> %1 Mio) : %2")
                            .arg(kMaxFileBytes / (1024 * 1024)).arg(remotePath);
                return false;
            }
            out.append(buffer, static_cast<int>(n));
        }
        libssh2_sftp_close(handle);
        if (n < 0) { error = QStringLiteral("erreur de lecture SFTP : %1").arg(remotePath); return false; }
        return true;
    }

private:
    bool checkHostKey(const QJsonObject& params, QString& error) {
#ifdef LIBSSH2_HOSTKEY_HASH_SHA256
        const char* h = libssh2_hostkey_hash(session_, LIBSSH2_HOSTKEY_HASH_SHA256);
        const QByteArray seen = h ? QByteArray(h, 32) : QByteArray();
#else
        const QByteArray seen;
#endif
        const QString expected = params.value(QStringLiteral("host_key_sha256")).toString();
        if (expected.isEmpty()) {
            // Mode non verifie, explicite : journaliser l'empreinte pour que
            // l'operateur puisse la renseigner (host_key_sha256) et fermer la faille.
            qWarning().noquote()
                << "morfCollector sftp : cle d'hote NON verifiee (host_key_sha256 absent) ;"
                   " empreinte vue : SHA256:"
                << QString::fromLatin1(seen.toBase64(QByteArray::OmitTrailingEquals));
            return true;
        }
        if (!hostKeyMatches(seen, expected)) {
            error = QStringLiteral("empreinte de cle d'hote differente de host_key_sha256 : "
                                   "connexion refusee, aucun secret envoye");
            return false;
        }
        return true;
    }

    bool authenticate(const QJsonObject& creds, QString& error) {
        const std::string user = creds.value(QStringLiteral("user")).toString().toStdString();
        if (user.empty()) { error = QStringLiteral("credentials.user requis"); return false; }

        const QString keyFile    = creds.value(QStringLiteral("key_file")).toString();
        const QString keyPub     = creds.value(QStringLiteral("key_pub")).toString();
        const QString privateKey = creds.value(QStringLiteral("private_key")).toString();
        const QString password   = creds.value(QStringLiteral("password")).toString();
        const std::string pass   = creds.value(QStringLiteral("passphrase")).toString().toStdString();

        int  rc = -1;
        bool attempted = false;

        // 1. Cle privee sur fichier local (a la machine de collecte).
        if (!keyFile.isEmpty()) {
            attempted = true;
            const std::string priv = keyFile.toStdString();
            std::string pub = keyPub.toStdString();
            if (pub.empty() && QFileInfo::exists(keyFile + QStringLiteral(".pub")))
                pub = (keyFile + QStringLiteral(".pub")).toStdString();
            rc = libssh2_userauth_publickey_fromfile(
                session_, user.c_str(), pub.empty() ? nullptr : pub.c_str(),
                priv.c_str(), pass.empty() ? nullptr : pass.c_str());
        }

        // 2. Cle privee en memoire (matiere remise au coffre).
        if (rc != 0 && !privateKey.isEmpty()) {
            attempted = true;
            const QByteArray priv = privateKey.toUtf8();
            rc = libssh2_userauth_publickey_frommemory(
                session_, user.c_str(), user.size(),
                nullptr, 0,
                priv.constData(), static_cast<size_t>(priv.size()),
                pass.empty() ? nullptr : pass.c_str());
        }

        // 3. Repli sur le mot de passe.
        if (rc != 0 && !password.isEmpty()) {
            attempted = true;
            const std::string pw = password.toStdString();
            rc = libssh2_userauth_password(session_, user.c_str(), pw.c_str());
        }

        if (!attempted) { error = QStringLiteral("aucun secret d'authentification (cle ou mot de passe)"); return false; }
        if (rc != 0)    { error = QStringLiteral("authentification refusee"); return false; }
        return true;
    }

    void close() {
        if (sftp_)    { libssh2_sftp_shutdown(sftp_); sftp_ = nullptr; }
        if (session_) {
            libssh2_session_disconnect(session_, "morfCollector: fin de session");
            libssh2_session_free(session_);
            session_ = nullptr;
        }
        if (sock_ != kInvalidSocket) { closeSocket(sock_); sock_ = kInvalidSocket; }
#ifdef _WIN32
        if (wsaInit_) { WSACleanup(); wsaInit_ = false; }
#endif
    }

    LIBSSH2_SESSION* session_ = nullptr;
    LIBSSH2_SFTP*    sftp_    = nullptr;
    socket_t         sock_    = kInvalidSocket;
    bool             wsaInit_ = false;
};

} // namespace

SftpConnector::SftpConnector()  { libssh2_init(0); }
SftpConnector::~SftpConnector() { libssh2_exit(); }

bool SftpConnector::validate(const QJsonObject& params, QString& error) const {
    if (params.value(QStringLiteral("host")).toString().isEmpty()) {
        error = QStringLiteral("params.host requis pour le connecteur 'sftp'");
        return false;
    }
    if (params.value(QStringLiteral("remote_dir")).toString().isEmpty()) {
        error = QStringLiteral("params.remote_dir requis pour le connecteur 'sftp'");
        return false;
    }
    return true;
}

IConnector::Outcome SftpConnector::list(const QJsonObject& params, const QJsonObject& credentials,
                                        QVector<RemoteItem>& out, QString& error) {
    const QString dir     = params.value(QStringLiteral("remote_dir")).toString();
    const QString match   = params.value(QStringLiteral("match")).toString();
    const QString siteKey = params.value(QStringLiteral("site_key")).toString();

    Session s;
    Outcome oc = Outcome::Error;
    if (!s.open(params, credentials, oc, error))
        return oc;

    QVector<QPair<QString, quint64>> files;
    if (!s.listGz(dir, files, error))
        return Outcome::Error;

    for (const auto& f : files) {
        if (!belongsToSource(f.first, match, siteKey))
            continue;
        RemoteItem item;
        item.name   = f.first;
        item.size   = static_cast<qint64>(f.second);
        item.period = periodFromName(f.first);
        out.push_back(item);
    }
    return Outcome::Ok;
}

IConnector::Outcome SftpConnector::fetch(const QJsonObject& params, const QJsonObject& credentials,
                                         const QString& name, QByteArray& out, QString& error) {
    // Securite : on ne recupere qu'un simple nom de fichier, jamais un chemin.
    if (name.contains('/') || name.contains('\\') || name.contains(QStringLiteral(".."))) {
        error = QStringLiteral("nom de ressource invalide : %1").arg(name);
        return Outcome::Error;
    }
    const QString dir = params.value(QStringLiteral("remote_dir")).toString();
    const QString remotePath = dir.endsWith('/') ? dir + name : dir + '/' + name;

    Session s;
    Outcome oc = Outcome::Error;
    if (!s.open(params, credentials, oc, error))
        return oc;

    if (!s.read(remotePath, out, error))
        return Outcome::Error;
    return Outcome::Ok;
}

} // namespace morfcollector
