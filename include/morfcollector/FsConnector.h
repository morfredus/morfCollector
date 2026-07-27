/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include "morfcollector/IConnector.h"

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
// -----------------------------------------------------------------------------
namespace morfcollector {

class FsConnector : public IConnector {
public:
    QString name() const override       { return QStringLiteral("fs"); }
    int     version() const override    { return 1; }
    QString capability() const override { return QStringLiteral("fs"); }

    bool    validate(const QJsonObject& params, QString& error) const override;
    Outcome list(const QJsonObject& params, const QJsonObject& credentials,
                 QVector<RemoteItem>& out, QString& error) override;
    Outcome fetch(const QJsonObject& params, const QJsonObject& credentials,
                  const QString& name, QByteArray& out, QString& error) override;
};

} // namespace morfcollector
