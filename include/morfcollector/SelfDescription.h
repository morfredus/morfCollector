/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include "morfbeacon/PresenceConfig.h"

namespace morfcollector {

// -----------------------------------------------------------------------------
// fillAnnouncedDetail : renseigne le DETAIL annonce du service (bloc `api` de
// /status), via morfbeacon::describeService. Point UNIQUE de description.
//
// N'y figurent PAS les routes de cadre (/status, /healthz), qu'un observateur
// connait deja par le protocole. Les routes exposant des coordonnees de source
// ou des secrets ne sont evidemment pas plus bavardes ici qu'ailleurs : la liste
// ne donne que methode + chemin + resume (CONTRAT.md §7.3).
// -----------------------------------------------------------------------------
inline void fillAnnouncedDetail(morfbeacon::PresenceConfig& pc) {
    pc.api = {
        {QStringLiteral("POST"), QStringLiteral("/manifest"),
         QStringLiteral("recevoir un manifeste de collecte (morfcollect/1)")},
        {QStringLiteral("GET"),  QStringLiteral("/manifest/state"),
         QStringLiteral("generation + revision appliquees pour un fournisseur")},
        {QStringLiteral("POST"), QStringLiteral("/credentials"),
         QStringLiteral("remise unique de secrets vers le coffre")},
        {QStringLiteral("GET"),  QStringLiteral("/sources"),
         QStringLiteral("liste des sources et de leur etat")},
        {QStringLiteral("GET"),  QStringLiteral("/sources/{id}"),
         QStringLiteral("detail d'une source (triplet de suspension)")},
        {QStringLiteral("POST"), QStringLiteral("/sources/{id}/collect"),
         QStringLiteral("declencher une collecte immediate")},
        {QStringLiteral("POST"), QStringLiteral("/sources/{id}/suspend"),
         QStringLiteral("suspendre la collecte (override operateur)")},
        {QStringLiteral("POST"), QStringLiteral("/sources/{id}/resume"),
         QStringLiteral("reprendre la collecte")},
        {QStringLiteral("GET"),  QStringLiteral("/sources/{id}/objects"),
         QStringLiteral("objets collectes pour une source")},
        {QStringLiteral("GET"),  QStringLiteral("/objects/{object_id}"),
         QStringLiteral("recuperer l'original conserve")},
    };
    pc.apiBasePath = QStringLiteral("/");
}

} // namespace morfcollector
