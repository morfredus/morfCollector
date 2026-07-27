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
#include <QStandardPaths>
#include <utility>

namespace morfcollector {

namespace {
// Racine de conservation : celle de la config, sinon un defaut par plateforme.
QString resolveStorageRoot(const QString& configured) {
    if (!configured.isEmpty())
        return configured;
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!base.isEmpty())
        return QDir(base).filePath(QStringLiteral("morfcollector-data"));
    return QDir::current().filePath(QStringLiteral("morfcollector-data"));
}
} // namespace

Service::Service(ServiceConfig config, QObject* parent)
    : QObject(parent),
      m_config(std::move(config)),
      m_collector(new Collector(resolveStorageRoot(m_config.storageRoot), this)),
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
        pc.capabilities = { QStringLiteral("collection") };

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
Collector* Service::collector() const { return m_collector; }

} // namespace morfcollector
