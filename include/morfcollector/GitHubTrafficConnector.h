/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include "morfcollector/IConnector.h"

// -----------------------------------------------------------------------------
// GitHubTrafficConnector : connecteur `github-traffic` (contrat github-traffic/1).
//
// Recupere un instantane brut des metriques GitHub d'un depot public dont le
// proprietaire a autorise l'acces Traffic (fine-grained PAT, Administration
// lecture seule). morfCollector n'INTERPRETE rien : il conserve la reponse
// datee. L'historisation (upsert quotidien, deltas de telechargements) appartient
// a morfAnalytics.
//
// params :
//   "owner"      : compte GitHub (obligatoire)
//   "repository" : nom du depot (obligatoire)
// credentials (coffre) :
//   "token"      : PAT ; JAMAIS dans le manifeste, jamais dans l'objet conserve
//
// Un objet par jour UTC : github-traffic-YYYY-MM-DD.json. GitHub ne garde que
// 14 jours de trafic detaille ; un arret plus long laisse un trou, jamais une
// reconstruction inventee.
// -----------------------------------------------------------------------------
namespace morfcollector {

class GitHubTrafficConnector : public IConnector {
public:
    QString name() const override       { return QStringLiteral("github-traffic"); }
    int     version() const override    { return 1; }
    QString capability() const override { return QStringLiteral("github-traffic"); }

    bool    validate(const QJsonObject& params, QString& error) const override;
    Outcome list(const QJsonObject& params, const QJsonObject& credentials,
                 QVector<RemoteItem>& out, QString& error) override;
    Outcome fetch(const QJsonObject& params, const QJsonObject& credentials,
                  const QString& name, QByteArray& out, QString& error) override;
};

} // namespace morfcollector
