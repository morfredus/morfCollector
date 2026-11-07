/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include "morfcollector/IConnector.h"

#include <QStringList>

#include <utility>

// -----------------------------------------------------------------------------
// FsConnector : connecteur `fs`, premier connecteur concret (CONTRAT.md §2).
//
// Collecte des fichiers depuis un DOSSIER LOCAL (par ex. un partage NAS/SMB
// monte). Il execute une mission definie par le manifeste ; il ne decide de rien
// et ne « scanne » pas a l'aveugle (principe executant). Entierement local, donc
// sans dependance externe et testable hors ligne : il prouve toute la chaine de
// collecte avant l'arrivee des connecteurs reseau (sftp...).
//
// params :
//   "dir"        : dossier a collecter (obligatoire)
//   "match"      : sous-chaine que le nom doit contenir (facultatif, insensible casse)
//   "match_mode" : "generic" (defaut). Reserve pour futures strategies.
// Aucun secret : credentials ignore.
//
// SECURITE : "dir" vient du manifeste (donc d'un client du LAN). Il n'est accepte
// que sous une racine de la liste blanche 'fs_allowed_roots' de la config ADMIN
// (lecture seule, jamais modifiable par l'API). Sans liste blanche, le connecteur
// refuse tout. Les liens symboliques et les dossiers du coffre/magasin sont refuses.
// -----------------------------------------------------------------------------
namespace morfcollector {

class FsConnector : public IConnector {
public:
    // allowedRoots : racines autorisees (config admin) ; forbidden : dossiers
    // interdits meme sous une racine (coffre, magasin d'objets).
    FsConnector(QStringList allowedRoots, QStringList forbidden)
        : m_roots(std::move(allowedRoots)), m_forbidden(std::move(forbidden)) {}

    QString name() const override       { return QStringLiteral("fs"); }
    int     version() const override    { return 1; }
    QString capability() const override { return QStringLiteral("fs"); }

    bool    validate(const QJsonObject& params, QString& error) const override;
    Outcome list(const QJsonObject& params, const QJsonObject& credentials,
                 QVector<RemoteItem>& out, QString& error) override;
    Outcome fetch(const QJsonObject& params, const QJsonObject& credentials,
                  const QString& name, QByteArray& out, QString& error) override;

private:
    QStringList m_roots;
    QStringList m_forbidden;
};

} // namespace morfcollector
