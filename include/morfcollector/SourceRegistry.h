/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QString>
#include <QHash>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>

#include "morfcollector/Source.h"
#include "morfcollector/Manifest.h"

// -----------------------------------------------------------------------------
// SourceRegistry : detient les sources et applique la RECONCILIATION declarative
// du contrat (CONTRAT.md §4). Aucune E/S, aucune dependance Qt lourde : logique
// pure, testable sans reseau ni serveur.
//
// Responsabilites :
//   - suivre, par fournisseur (provider.instance), la (generation, revision)
//     appliquee ;
//   - decider d'appliquer / ignorer / rejeter un manifeste (CONTRAT.md §4.2/4.3) ;
//   - indexer les sources par source_id, en garantissant qu'un source_id
//     appartient a un seul fournisseur (CONTRAT.md §4.4) ;
//   - porter les overrides operateur (suspend/resume), prioritaires et
//     persistants (CONTRAT.md §4.3.1).
// -----------------------------------------------------------------------------
namespace morfcollector {

class SourceRegistry {
public:
    // Resultat de l'application d'un manifeste.
    enum class Apply {
        Applied,          // reconciliation effectuee
        IgnoredStale,     // revision <= appliquee dans une generation connue -> 409
        RejectedOwnership // touche une source d'un autre fournisseur -> 409
    };

    struct ApplyResult {
        Apply   status;
        qint64  revision   = 0;   // revision desormais appliquee pour ce fournisseur
        QString generation;       // generation appliquee
        QString message;          // detail lisible (log / corps de reponse)
    };

    // Applique un manifeste deja parse/valide (voir Manifest::parse pour le 422).
    ApplyResult apply(const Manifest& manifest);

    // Etat (generation, revision) applique pour un fournisseur. found=false si
    // ce fournisseur n'a jamais rien pousse (-> 404 sur /manifest/state).
    bool stateOf(const QString& providerInstance, QString& generation, qint64& revision) const;

    // --- Acces aux sources ---------------------------------------------------
    Source*       find(const QString& sourceId);
    const Source* find(const QString& sourceId) const;
    int           count() const { return m_sources.size(); }
    QList<Source*> all();               // iteration (ordre non garanti)
    bool           remove(const QString& sourceId);   // suppression definitive

    // --- Vues / metriques ----------------------------------------------------
    QJsonArray  sourcesJson() const;               // GET /sources
    QJsonObject metrics() const;                    // resume pour /status

private:
    struct ProviderState {
        QString generation;
        qint64  revision = -1;    // -1 = aucun manifeste applique
    };

    QHash<QString, Source>        m_sources;         // source_id -> Source
    QHash<QString, ProviderState> m_providers;       // instance  -> etat applique
};

} // namespace morfcollector
