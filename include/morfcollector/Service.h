/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QObject>
#include <QStringList>
#include "morfcollector/ServiceConfig.h"

namespace morfbeacon { class Heartbeat; }

namespace morfcollector {

class Collector;
class HttpServer;

// -----------------------------------------------------------------------------
// Service : facade qui cable tout a partir d'une ServiceConfig.
//
//   config JSON (reglages machine)
//     -> Collector (moteur : plan de controle du contrat morfcollect/1)
//     -> HttpServer (API)
//     -> morfbeacon::Heartbeat (annonce de presence sur le LAN, capacite
//        "collection")
//
// Les SOURCES ne viennent pas de la config : elles arrivent par le manifeste
// (POST /manifest). Seul objet manipule par le demon (service/main.cpp).
// -----------------------------------------------------------------------------
class Service : public QObject {
    Q_OBJECT
public:
    explicit Service(ServiceConfig config, QObject* parent = nullptr);
    ~Service() override;

    bool start();
    void stop();

    int         sourceCount() const;
    quint16     httpPort() const;

    Collector* collector() const;

private:
    ServiceConfig          m_config;
    Collector*             m_collector;
    HttpServer*            m_http;
    morfbeacon::Heartbeat* m_heartbeat = nullptr;
};

} // namespace morfcollector
