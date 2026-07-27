/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QString>
#include <QVector>
#include <QJsonObject>

// -----------------------------------------------------------------------------
// Manifest.h : le message d'entree du contrat morfcollect/1 (CONTRAT.md §1).
//
// Un manifeste est le SEUL moyen par lequel un fournisseur decrit ce qu'il faut
// collecter. Ces structures sont un miroir passif du JSON : aucune logique de
// reconciliation ici (voir SourceRegistry), seulement le parsing et la
// validation de forme (proto, champs requis).
// -----------------------------------------------------------------------------
namespace morfcollector {

// Reference de connecteur (CONTRAT.md §2) : le coeur n'en connait que nom+version.
struct ConnectorRef {
    QString name;      // "sftp", "http", ...
    int     version = 1;
};

// Une source declaree dans un manifeste (etat desire).
struct ManifestSource {
    QString        sourceId;         // UUID stable, identite (CONTRAT.md §1.3)
    QString        label;            // nom d'affichage, cosmetique
    bool           enabled = true;   // false => suspension cote fournisseur
    ConnectorRef   connector;
    QJsonObject    params;           // opaques au coeur, propres au connecteur
    QString        credentialsRef;   // reference vers le coffre (CONTRAT.md §1.5)
    QJsonObject    schedule;         // cadence souhaitee
    QJsonObject    retention;        // politique de conservation (CONTRAT.md §5.4)
};

// Identite du fournisseur (CONTRAT.md §1.2). `instance` = domaine de propriete.
struct ProviderRef {
    QString app;
    QString instance;
};

// Un manifeste complet.
struct Manifest {
    QString                 proto;               // "morfcollect/1"
    ProviderRef             provider;
    QString                 generation;          // UUID de lignee (CONTRAT.md §1.4)
    qint64                  revision = 0;        // monotone dans une generation
    QVector<ManifestSource> sources;

    // Parse et valide la FORME d'un manifeste. Renvoie false et renseigne `error`
    // si le document n'est pas applicable (proto inconnu, champ requis absent) ->
    // l'appelant repond 422. Ne juge PAS de la fraicheur (revision) : c'est le
    // role de SourceRegistry (409 le cas echeant).
    static bool parse(const QJsonObject& root, Manifest& out, QString& error);
};

// Prefixe de protocole attendu (le suffixe de version peut evoluer additivement).
constexpr const char* kManifestProto = "morfcollect/1";

} // namespace morfcollector
