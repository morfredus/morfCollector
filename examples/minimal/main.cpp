/*
 * morfCollector — exemple de demonstration
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Demarre le service et expose l'API du contrat morfcollect/1. A tester :
 *   curl http://localhost:8792/status
 *   curl http://localhost:8792/sources
 *   curl -X POST http://localhost:8792/manifest -d @manifest.json
 */

#include <QCoreApplication>

#include <morfcollector/Service.h>
#include <morfcollector/ServiceConfig.h>
#include <morfcollector/Version.h>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    morfcollector::ServiceConfig cfg;
    cfg.httpPort         = 8792;
    cfg.beaconIntervalMs = 5000;

    morfcollector::Service service(cfg);
    if (!service.start()) {
        qWarning("API HTTP non demarree (port %u occupe ?)", cfg.httpPort);
        return 1;
    }

    qInfo("morfCollector demo v%s : API http://localhost:%u/  (POST /manifest ; GET /sources)",
          qUtf8Printable(morfcollector::version()), service.httpPort());

    return app.exec();
}
