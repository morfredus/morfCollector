/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "morfcollector/Service.h"
#include "morfcollector/Collector.h"
#include "morfcollector/HttpServer.h"
#include "morfcollector/Version.h"

#include "morfbeacon/Heartbeat.h"
#include "morfbeacon/PresenceConfig.h"

#include <QDir>
#include <utility>

namespace morfcollector {

namespace {
// Repertoires par defaut du service, alignes sur service.json (docs/FILESYSTEM.md).
// Trois zones distinctes : le PROGRAMME dans /opt/morfcollector, la CONFIG ADMIN
// (lecture seule) dans /etc/morfsystem/morfcollector, et tout l'ETAT PERSISTANT
// (donnees collectees + coffre) dans /var/lib/morfsystem/morfcollector. Le coffre
// et les donnees restent des sous-dossiers SEPARES de l'etat (vault/ et data/),
// pour que copier/sauvegarder data/ n'emporte jamais la cle (CONTRAT.md §1.5).
// Racine de l'ETAT PERSISTANT, garantie accessible en ecriture. Honore
// $STATE_DIRECTORY pose par systemd (StateDirectory=morfsystem/morfcollector) ;
// repli conforme a l'OS sinon. C'est la reponse structurelle au probleme de
// droits qui rendait le coffre inecrivable sous un /etc appartenant a root.
QString stateDir() {
    const QByteArray env = qgetenv("STATE_DIRECTORY");
    if (!env.isEmpty()) {
        const QString first = QString::fromLocal8Bit(env).split(QLatin1Char(':')).first();
        if (!first.isEmpty()) { QDir().mkpath(first); return first; }
    }
#if defined(Q_OS_WIN)
    const QString base = qEnvironmentVariable("ProgramData", QStringLiteral("C:/ProgramData"));
    const QString dir  = QDir(base).filePath(QStringLiteral("morfsystem/morfcollector/state"));
#else
    const QString dir  = QStringLiteral("/var/lib/morfsystem/morfcollector");
#endif
    QDir().mkpath(dir);
    return dir;
}

// Donnees metier (objets + index) : <etat>/data. Ecrasable par 'storage_root'.
QString resolveDataRoot(const QString& configured) {
    if (!configured.isEmpty())
        return configured;
    return QDir(stateDir()).filePath(QStringLiteral("data"));
}

// Coffre de secrets : <etat>/vault, sous-dossier SEPARE des donnees pour que
// copier data/ n'emporte jamais la cle (CONTRAT.md §1.5). Ecrasable par
// 'vault_root'. N'est PLUS dans /etc : le coffre est de l'etat genere par le
// service (cle + secrets chiffres), pas de la config admin.
QString resolveVaultRoot(const QString& configured) {
    if (!configured.isEmpty())
        return configured;
    return QDir(stateDir()).filePath(QStringLiteral("vault"));
}
} // namespace

Service::Service(ServiceConfig config, QObject* parent)
    : QObject(parent),
      m_config(std::move(config)),
      m_collector(new Collector(resolveDataRoot(m_config.storageRoot),
                                resolveVaultRoot(m_config.vaultRoot), this)),
      m_http(new HttpServer(m_config, m_collector, this)) {}

Service::~Service() = default;

bool Service::start() {
    const bool httpOk = (m_config.httpPort == 0) ? true : m_http->start();

    if (m_config.beaconEnabled) {
        morfbeacon::PresenceConfig pc;
        pc.appName             = m_config.appName;
        pc.version             = morfcollector::version();
        pc.instanceId          = m_config.instanceId;
        pc.udpPort             = m_config.beaconUdpPort;
        pc.broadcastIntervalMs = m_config.beaconIntervalMs;
        pc.statusPort          = m_http ? m_http->port() : 0;
        pc.statusBindAddress   = m_config.bindAddress;

        // Capacite du parc : un fournisseur decouvre morfCollector par ce qu'il
        // sait faire, jamais par son nom (CONTRAT.md §7.1).
        pc.capabilities = m_collector->capabilities();
        if (!pc.capabilities.contains(QStringLiteral("collection")))
            pc.capabilities.prepend(QStringLiteral("collection"));

        m_heartbeat = new morfbeacon::Heartbeat(pc, m_collector, this);
        m_heartbeat->start();
    }

    return httpOk;
}

void Service::stop() {
    if (m_heartbeat)
        m_heartbeat->stop();
    if (m_http)
        m_http->stop();
}

int Service::sourceCount() const  { return m_collector->sourcesEnvelope().value("count").toInt(); }
quint16 Service::httpPort() const { return m_http ? m_http->port() : 0; }
bool Service::vaultReady() const  { return m_collector->vaultReady(); }
Collector* Service::collector() const { return m_collector; }

} // namespace morfcollector
