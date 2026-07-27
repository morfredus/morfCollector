/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "morfcollector/FsConnector.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace morfcollector {

namespace {
// Deduit une periode "YYYY-MM-DD" du nom, si un marqueur de date s'y trouve.
// Ne lit JAMAIS le contenu (CONTRAT.md §5.3). Vide si rien de fiable.
QString periodFromName(const QString& name) {
    static const QRegularExpression re(QStringLiteral("(20\\d{2})[-_]?(\\d{2})[-_]?(\\d{2})"));
    const QRegularExpressionMatch m = re.match(name);
    if (!m.hasMatch())
        return QString();
    const int mm = m.captured(2).toInt();
    const int dd = m.captured(3).toInt();
    if (mm < 1 || mm > 12 || dd < 1 || dd > 31)
        return QString();
    return QStringLiteral("%1-%2-%3").arg(m.captured(1), m.captured(2), m.captured(3));
}
} // namespace

bool FsConnector::validate(const QJsonObject& params, QString& error) const {
    const QString dir = params.value(QStringLiteral("dir")).toString();
    if (dir.isEmpty()) {
        error = QStringLiteral("params.dir requis pour le connecteur 'fs'");
        return false;
    }
    return true;
}

IConnector::Outcome FsConnector::list(const QJsonObject& params, const QJsonObject& /*credentials*/,
                                      QVector<RemoteItem>& out, QString& error) {
    const QString dirPath = params.value(QStringLiteral("dir")).toString();
    const QString match   = params.value(QStringLiteral("match")).toString();

    QDir dir(dirPath);
    if (!dir.exists()) {
        error = QStringLiteral("dossier introuvable : %1").arg(dirPath);
        return Outcome::Unreachable;
    }

    const QFileInfoList entries = dir.entryInfoList(QDir::Files, QDir::Name);
    for (const QFileInfo& fi : entries) {
        const QString n = fi.fileName();
        if (!match.isEmpty() && !n.contains(match, Qt::CaseInsensitive))
            continue;
        RemoteItem item;
        item.name   = n;
        item.size   = fi.size();
        item.period = periodFromName(n);
        out.push_back(item);
    }
    return Outcome::Ok;
}

IConnector::Outcome FsConnector::fetch(const QJsonObject& params, const QJsonObject& /*credentials*/,
                                       const QString& name, QByteArray& out, QString& error) {
    const QString dirPath = params.value(QStringLiteral("dir")).toString();

    // Securite : on ne recupere qu'un fichier du dossier declare, jamais un
    // chemin remonte par "..".
    const QFileInfo fi(QDir(dirPath), name);
    const QString canonicalDir = QDir(dirPath).canonicalPath();
    if (fi.fileName() != name || !fi.absoluteFilePath().startsWith(canonicalDir)) {
        error = QStringLiteral("nom de ressource invalide : %1").arg(name);
        return Outcome::Error;
    }

    QFile f(fi.absoluteFilePath());
    if (!f.exists()) {
        error = QStringLiteral("ressource disparue : %1").arg(name);
        return Outcome::Error;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("lecture impossible : %1").arg(name);
        return Outcome::Error;
    }
    out = f.readAll();
    return Outcome::Ok;
}

} // namespace morfcollector
