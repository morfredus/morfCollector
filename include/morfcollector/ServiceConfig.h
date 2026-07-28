/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QString>
#include <QJsonObject>
#include <QtGlobal>

namespace morfcollector {

// -----------------------------------------------------------------------------
// ServiceConfig : configuration GLOBALE du service morfCollector.
//
// Elle ne declare PAS les sources : celles-ci ne viennent QUE du manifeste pousse
// par le fournisseur (CONTRAT.md : « une seule configuration utilisateur »).
// Elle ne porte donc que les reglages propres a la machine : port, annonce LAN,
// et l'emplacement local de conservation.
// -----------------------------------------------------------------------------
struct ServiceConfig {
    QString appName    = QStringLiteral("morfCollector");
    QString instanceId;                              // defaut = appName@hostname

    // Racine des DONNEES metier : objets collectes + index, source de verite du
    // service (docs/FILESYSTEM.md). Vide => <app_dir>/data choisi a l'execution
    // (/opt/morfcollector/data sous Linux).
    QString storageRoot;

    // Racine du COFFRE de secrets (vault.enc + vault.key). Volontairement SEPAREE
    // des donnees : copier le dossier data/ ne doit jamais emporter la cle. Vide
    // => dossier de configuration (/etc/morfsystem/morfcollector sous Linux), la ou vivent
    // deja les secrets de la machine.
    QString vaultRoot;

    // Port de service morfCollector (bloc de service 8787-8799), reserve dans
    // 'ports.allocations' de morfTools/ecosystem.json. C'est le defaut COMPILE :
    // il s'applique quand aucun fichier de configuration n'est trouve. Il doit
    // rester identique a la valeur du registre et de config/ : 'morf doctor'
    // echoue tant qu'ils divergent.
    quint16 httpPort    = 8792;                      // 0 => pas de serveur HTTP
    QString bindAddress = QStringLiteral("0.0.0.0");

    // Annonce de presence sur le LAN via morfBeacon.
    bool    beaconEnabled    = true;
    quint16 beaconUdpPort    = 45454;                // port du parc morfSystem
    int     beaconIntervalMs = 15000;

    static ServiceConfig fromJson(const QJsonObject& root) {
        ServiceConfig c;
        if (root.contains("app_name"))     c.appName     = root.value("app_name").toString(c.appName);
        if (root.contains("instance_id"))  c.instanceId  = root.value("instance_id").toString();
        if (root.contains("http_port"))    c.httpPort    = static_cast<quint16>(root.value("http_port").toInt(c.httpPort));
        if (root.contains("bind_address")) c.bindAddress = root.value("bind_address").toString(c.bindAddress);
        if (root.contains("storage_root")) c.storageRoot = root.value("storage_root").toString(c.storageRoot);
        if (root.contains("vault_root"))   c.vaultRoot   = root.value("vault_root").toString(c.vaultRoot);

        const QJsonObject beacon = root.value("beacon").toObject();
        if (beacon.contains("enabled"))     c.beaconEnabled    = beacon.value("enabled").toBool(c.beaconEnabled);
        if (beacon.contains("udp_port"))    c.beaconUdpPort    = static_cast<quint16>(beacon.value("udp_port").toInt(c.beaconUdpPort));
        if (beacon.contains("interval_ms")) c.beaconIntervalMs = beacon.value("interval_ms").toInt(c.beaconIntervalMs);

        return c;
    }
};

} // namespace morfcollector
