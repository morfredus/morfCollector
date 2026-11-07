/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "morfcollector/Guards.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace morfcollector {

namespace {
// Windows ignore la casse des chemins, Linux la respecte.
#ifdef Q_OS_WIN
constexpr Qt::CaseSensitivity kPathCase = Qt::CaseInsensitive;
#else
constexpr Qt::CaseSensitivity kPathCase = Qt::CaseSensitive;
#endif

QString withoutTrailingSlash(QString p) {
    while (p.size() > 1 && p.endsWith(QLatin1Char('/')))
        p.chop(1);
    return p;
}
} // namespace

bool isPathUnder(const QString& path, const QString& root) {
    const QString p = withoutTrailingSlash(path);
    const QString r = withoutTrailingSlash(root);
    if (r.isEmpty() || p.isEmpty())
        return false;
    if (p.compare(r, kPathCase) == 0)
        return true;
    // Separateur final : sans lui "/data2" passerait pour un enfant de "/data".
    const QString prefix = r.endsWith(QLatin1Char('/')) ? r : r + QLatin1Char('/');
    return p.startsWith(prefix, kPathCase);
}

QString validateFsDir(const QString& dir, const QStringList& allowedRoots,
                      const QStringList& forbidden, QString& error) {
    if (allowedRoots.isEmpty()) {
        error = QStringLiteral("connecteur 'fs' desactive : aucune racine autorisee "
                               "('fs_allowed_roots' absent de la configuration admin)");
        return QString();
    }
    if (dir.isEmpty()) {
        error = QStringLiteral("params.dir requis pour le connecteur 'fs'");
        return QString();
    }
    const QFileInfo fi(dir);
    // Un lien symbolique permettrait de sortir de la racine apres validation.
    if (fi.isSymLink()) {
        error = QStringLiteral("dossier refuse (lien symbolique) : %1").arg(dir);
        return QString();
    }
    const QString canon = QDir(dir).canonicalPath();   // vide si inexistant
    if (canon.isEmpty()) {
        error = QStringLiteral("dossier introuvable : %1").arg(dir);
        return QString();
    }
    for (const QString& f : forbidden) {
        const QString cf = QDir(f).canonicalPath();
        if (!cf.isEmpty() && isPathUnder(canon, cf)) {
            error = QStringLiteral("dossier refuse (coffre ou magasin du service) : %1").arg(dir);
            return QString();
        }
    }
    for (const QString& r : allowedRoots) {
        const QString cr = QDir(r).canonicalPath();
        if (!cr.isEmpty() && isPathUnder(canon, cr))
            return canon;
    }
    error = QStringLiteral("dossier hors des racines autorisees ('fs_allowed_roots') : %1").arg(dir);
    return QString();
}

bool isCrossOrigin(const QByteArray& origin, const QByteArray& hostHeader) {
    const QByteArray o = origin.trimmed();
    if (o.isEmpty())
        return false;   // client non-navigateur : pas d'Origin, pas de CSRF possible
    const int s = o.indexOf("://");
    const QByteArray authority = (s < 0) ? o : o.mid(s + 3);   // "null" => different
    return authority.toLower() != hostHeader.trimmed().toLower();
}

QString sanitizeFileName(const QString& name) {
    QString s;
    s.reserve(name.size());
    for (const QChar c : name) {
        if (c.isLetterOrNumber() || c == QLatin1Char('.') || c == QLatin1Char('-')
            || c == QLatin1Char('_'))
            s.append(c);
        else
            s.append(QLatin1Char('_'));
    }
    if (s.isEmpty())
        s = QStringLiteral("object");
    return s;
}

QByteArray contentDisposition(const QString& originalName) {
    // Forme ASCII sure + forme RFC 5987 pour le nom exact (encodee en %XX, donc
    // sans guillemet ni retour a la ligne injectable).
    return "attachment; filename=\"" + sanitizeFileName(originalName).toUtf8()
         + "\"; filename*=UTF-8''" + QUrl::toPercentEncoding(originalName);
}

bool hostKeyMatches(const QByteArray& rawSha256, const QString& expected) {
    if (rawSha256.size() != 32)
        return false;
    QString e = expected.trimmed();
    if (e.startsWith(QStringLiteral("SHA256:"), Qt::CaseInsensitive)) {
        // Format OpenSSH : base64 sans bourrage.
        const QByteArray b64 = e.mid(7).toLatin1();
        const QByteArray dec = QByteArray::fromBase64(b64,
            QByteArray::Base64Encoding | QByteArray::OmitTrailingEquals);
        return dec == rawSha256;
    }
    e.remove(QLatin1Char(':'));
    return e.size() == 64 && e.compare(QString::fromLatin1(rawSha256.toHex()), Qt::CaseInsensitive) == 0;
}

} // namespace morfcollector
