/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QString>
#include <QJsonObject>

// -----------------------------------------------------------------------------
// CollectedObject : l'unite conservee (CONTRAT.md §5.1). Generique : un journal
// .gz aujourd'hui, un backup / PDF / CSV demain. Conserve TEL QUEL ; ces champs
// sont des METADONNEES d'index, jamais le contenu.
// -----------------------------------------------------------------------------
namespace morfcollector {

struct CollectedObject {
    QString objectId;       // UUID stable
    QString sourceId;       // source d'appartenance
    QString originalName;   // nom d'origine chez la source distante
    QString storedPath;     // chemin local du fichier conserve (jamais annonce)
    qint64  size = 0;
    QString hash;           // sha256 hex (integrite, deduplication)
    qint64  collectedAt = 0;// date de recuperation (epoch s)
    QString period;         // periode couverte ("YYYY-MM-DD"), fournie, pas inferee

    // Vue d'index (metadonnees non sensibles). N'expose PAS storedPath.
    QJsonObject toJson() const {
        return QJsonObject{
            {"object_id", objectId},
            {"source_id", sourceId},
            {"original_name", originalName},
            {"size", static_cast<double>(size)},
            {"hash", hash},
            {"collected_at", static_cast<double>(collectedAt)},
            {"period", period},
        };
    }
};

} // namespace morfcollector
