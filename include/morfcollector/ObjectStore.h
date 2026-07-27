/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <QHash>
#include <QByteArray>

#include "morfcollector/CollectedObject.h"

// -----------------------------------------------------------------------------
// ObjectStore : conservation des objets sur disque + index de metadonnees
// (CONTRAT.md §5). Conserve les originaux TELS QUELS ; l'index est persiste en
// JSON (le passage a SQLite reste un detail d'implementation libre).
//
// Disposition : <root>/objects/<source_id>/<object_id>__<nom>  et  <root>/index.json
//
// Note : la suppression (removeObject / removeAll) est le SEUL chemin qui detruit
// des octets ; elle n'est jamais declenchee par une reconciliation (CONTRAT.md
// §5.5). La retention ne purge que la copie locale (CONTRAT.md §5.4).
// -----------------------------------------------------------------------------
namespace morfcollector {

class ObjectStore {
public:
    explicit ObjectStore(QString root);

    bool init();   // cree l'arborescence et charge l'index

    // Deduplication : un nom d'origine deja conserve pour cette source est ignore
    // (on ne recollecte pas ce qu'on possede deja).
    bool hasName(const QString& sourceId, const QString& originalName) const;

    // Conserve `bytes` tel quel. Renvoie l'objet indexe ; `ok`=false en cas
    // d'echec d'ecriture.
    CollectedObject put(const QString& sourceId, const QString& originalName,
                        const QByteArray& bytes, const QString& period, bool& ok);

    // --- Lecture -------------------------------------------------------------
    QVector<CollectedObject> objectsOf(const QString& sourceId) const;
    QStringList              periodsOf(const QString& sourceId) const;
    bool  readObject(const QString& objectId, QByteArray& out, QString& originalName) const;
    bool  contains(const QString& objectId) const;

    // --- Suppression (action explicite uniquement) ---------------------------
    bool removeObject(const QString& objectId);
    int  removeAll(const QString& sourceId);

    // --- Retention (purge de la copie locale) --------------------------------
    // policy : { "mode": "keep_forever" | "keep_days" | "keep_size", ... }
    int applyRetention(const QString& sourceId, const QJsonObject& policy);

    // --- Totaux (pour /status) ----------------------------------------------
    qint64 objectCount() const { return m_objects.size(); }
    qint64 totalBytes() const;
    qint64 lastCollectTs() const;
    qint64 objectCountOf(const QString& sourceId) const;
    qint64 bytesOf(const QString& sourceId) const;

private:
    bool writeIndex() const;
    bool loadIndex();
    QString sourceDir(const QString& sourceId) const;

    QString                         m_root;
    QHash<QString, CollectedObject> m_objects;   // object_id -> objet
};

} // namespace morfcollector
