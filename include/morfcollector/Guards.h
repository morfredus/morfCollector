/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>

// -----------------------------------------------------------------------------
// Garde-fous PURS (sans E/S reseau, testables par ctest) :
//   - validation du dossier d'un connecteur `fs` contre une liste blanche ;
//   - controle d'origine (anti-CSRF) des ecritures HTTP ;
//   - nom de fichier sur pour Content-Disposition et pour le disque ;
//   - comparaison d'empreinte de cle d'hote SSH.
// -----------------------------------------------------------------------------
namespace morfcollector {

// true si `path` est `root` ou se trouve SOUS `root`. Compare avec un separateur
// final : "/data2" n'est PAS sous "/data". Les deux chemins doivent deja etre
// canoniques (liens resolus).
bool isPathUnder(const QString& path, const QString& root);

// Valide le dossier `dir` d'une source `fs`. Retourne le chemin CANONIQUE a
// utiliser, ou une chaine vide + `error`. Refuse : liste blanche vide, dossier
// inexistant ou lien symbolique, dossier hors des racines autorisees, dossier
// situe dans/sur un des dossiers `forbidden` (coffre, magasin d'objets).
QString validateFsDir(const QString& dir, const QStringList& allowedRoots,
                      const QStringList& forbidden, QString& error);

// Anti-CSRF : true si la requete d'ecriture doit etre refusee. `origin` vide
// (client non-navigateur) => jamais refusee ; sinon le host:port de l'Origin doit
// egaler `hostHeader`.
bool isCrossOrigin(const QByteArray& origin, const QByteArray& hostHeader);

// Nom de fichier sans caractere dangereux (lettres, chiffres, . - _ uniquement).
QString sanitizeFileName(const QString& name);

// Valeur complete de l'en-tete Content-Disposition pour un telechargement.
QByteArray contentDisposition(const QString& originalName);

// Compare l'empreinte SHA-256 brute (32 octets) d'une cle d'hote a l'attendu, au
// format hex (avec ou sans ':') ou base64 "SHA256:..." d'OpenSSH.
bool hostKeyMatches(const QByteArray& rawSha256, const QString& expected);

} // namespace morfcollector
