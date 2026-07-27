/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include "morfcollector/IConnector.h"

// -----------------------------------------------------------------------------
// SftpConnector : connecteur `sftp` (SSH/SFTP via libssh2). Recupere des fichiers
// chez un hebergeur dont la retention est courte (cas o2switch de SiteWatch).
//
// Connecteur RESEAU : ses operations list/fetch sont BLOQUANTES. Le Collector les
// execute donc sur un thread de travail (jamais sur le thread du serveur HTTP).
// Le connecteur est STATELESS : chaque appel ouvre sa propre session, ce qui rend
// plusieurs collectes simultanees (sources differentes) sures.
//
// params :
//   "host"       : hote SSH (obligatoire)
//   "remote_dir" : dossier distant des fichiers (obligatoire)
//   "port"       : port SSH (facultatif, defaut 22)
//   "match"      : sous-chaine que le nom doit contenir (mode generique)
//   "site_key"   : nom de site pour le mode o2switch (prefixe avant le 1er point)
// credentials (coffre) :
//   "user"        : utilisateur SSH (obligatoire)
//   "password"    : mot de passe (facultatif)
//   "key_file"    : chemin d'une cle privee LOCALE a la machine de collecte (facultatif)
//   "key_pub"     : chemin de la cle publique (facultatif ; deduit sinon)
//   "private_key" : matiere de cle privee PEM en memoire (facultatif)
//   "passphrase"  : phrase de passe de la cle (facultatif)
//
// NON TESTABLE en local (reseau + hebergeur) : code adapte de SiteWatch/SftpClient,
// deja eprouve en production.
// -----------------------------------------------------------------------------
namespace morfcollector {

class SftpConnector : public IConnector {
public:
    SftpConnector();
    ~SftpConnector() override;

    QString name() const override       { return QStringLiteral("sftp"); }
    int     version() const override    { return 1; }
    QString capability() const override { return QStringLiteral("sftp"); }

    bool    validate(const QJsonObject& params, QString& error) const override;
    Outcome list(const QJsonObject& params, const QJsonObject& credentials,
                 QVector<RemoteItem>& out, QString& error) override;
    Outcome fetch(const QJsonObject& params, const QJsonObject& credentials,
                  const QString& name, QByteArray& out, QString& error) override;
};

} // namespace morfcollector
