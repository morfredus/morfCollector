/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QHash>
#include <QJsonObject>

// -----------------------------------------------------------------------------
// Vault : coffre local CHIFFRE des secrets d'acces (CONTRAT.md §1.5).
//
// Les secrets sont remis UNE fois (POST /credentials), stockes chiffres au repos,
// jamais relus par aucune route ni annonce. Chiffrement AES-256-GCM (OpenSSL),
// avec une cle propre a l'installation, generee au premier demarrage et
// conservee a cote du coffre avec des permissions restreintes.
//
// Modele de menace assume : la cle vit sur la meme machine que le coffre (aucune
// interaction utilisateur possible pour un service headless). Le chiffrement
// protege donc une COPIE du fichier de coffre (sauvegarde, disque exfiltre seul),
// pas un attaquant ayant deja tous les droits sur la machine. C'est le niveau
// realiste pour un service autonome sans TPM ni phrase de passe.
//
// Disposition : <root>/vault.enc (donnees chiffrees) + <root>/vault.key (cle),
// ou <root> est le dossier de CONFIGURATION (/etc/morfcollector), distinct du
// dossier de donnees. Ainsi copier ou sauvegarder les objets collectes n'emporte
// jamais la cle : le chiffrement au repos garde son sens (docs/FILESYSTEM.md).
// -----------------------------------------------------------------------------
namespace morfcollector {

class Vault {
public:
    explicit Vault(QString root);

    // Charge la cle (ou la genere) puis dechiffre le coffre existant. false si le
    // chiffrement n'est pas disponible ou si le coffre est illisible (cle changee,
    // fichier corrompu) -- dans ce cas le coffre demarre vide plutot que de planter.
    bool init();

    // Remise unique d'un secret. Re-chiffre et persiste immediatement.
    bool put(const QString& ref, const QJsonObject& secret);

    // Lecture INTERNE (pour la collecte). Jamais exposee par l'API.
    QJsonObject get(const QString& ref) const;
    bool        contains(const QString& ref) const;

    int         size() const { return m_secrets.size(); }
    QStringList refs() const { return m_secrets.keys(); }

private:
    bool loadKey();
    bool load();     // dechiffre vault.enc -> m_secrets
    bool save();     // m_secrets -> chiffre -> vault.enc

    bool encrypt(const QByteArray& plain, QByteArray& out) const;
    bool decrypt(const QByteArray& blob, QByteArray& out) const;

    QString                       m_root;
    QByteArray                    m_key;       // 32 octets
    QHash<QString, QJsonObject>   m_secrets;
    bool                          m_ready = false;
};

} // namespace morfcollector
