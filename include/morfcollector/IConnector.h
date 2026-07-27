/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QString>
#include <QVector>
#include <QByteArray>
#include <QJsonObject>

// -----------------------------------------------------------------------------
// IConnector : le contrat de connecteur gele (CONTRAT.md §2).
//
// Le coeur generique ne connait d'un connecteur que ses CINQ facettes : nom,
// version, capacite, validation, et les deux operations de transport (list /
// fetch). params et credentials sont OPAQUES au coeur : seul le connecteur les
// interprete. C'est ce qui garantit que morfCollector n'embarque jamais la
// logique metier d'un client.
//
// Note de fil d'execution : cette premiere version est SYNCHRONE. Elle convient a
// un connecteur local (`fs`). Un connecteur RESEAU (sftp, http...) devra deporter
// list/fetch sur un thread de travail pour ne pas bloquer le serveur HTTP ; la
// facade restera la meme.
// -----------------------------------------------------------------------------
namespace morfcollector {

class IConnector {
public:
    virtual ~IConnector() = default;

    // --- Identite (CONTRAT.md §2) -------------------------------------------
    virtual QString name() const = 0;        // "fs", "sftp", ...
    virtual int     version() const = 0;     // version du sous-contrat du connecteur
    virtual QString capability() const = 0;  // identifiant stable annonce dans /status

    // --- Validation ----------------------------------------------------------
    // Verifie que `params` est exploitable AVANT toute collecte. false + `error`
    // sinon (la source basculera alors en etat operationnel ERROR).
    virtual bool validate(const QJsonObject& params, QString& error) const = 0;

    // --- Transport -----------------------------------------------------------
    // Une ressource distante enumeree. `period` est un indice fourni par le
    // connecteur (deduit du nom), jamais du contenu (CONTRAT.md §5.3).
    struct RemoteItem {
        QString name;         // nom d'origine chez la source (ex. "site.log-20260710.gz")
        qint64  size = -1;    // taille si connue, -1 sinon
        QString period;       // "YYYY-MM-DD" si deductible, vide sinon
    };

    // Issue d'une operation, mappee sur l'etat operationnel de la source.
    enum class Outcome { Ok, AuthFailed, Unreachable, Error };

    // Enumere les ressources disponibles a la source.
    virtual Outcome list(const QJsonObject& params, const QJsonObject& credentials,
                         QVector<RemoteItem>& out, QString& error) = 0;

    // Recupere le contenu d'UNE ressource, tel quel (aucune transformation).
    virtual Outcome fetch(const QJsonObject& params, const QJsonObject& credentials,
                          const QString& name, QByteArray& out, QString& error) = 0;
};

} // namespace morfcollector
