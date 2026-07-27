/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "morfcollector/Manifest.h"

#include <QJsonArray>
#include <QJsonValue>

namespace morfcollector {

bool Manifest::parse(const QJsonObject& root, Manifest& out, QString& error) {
    out = Manifest{};

    // --- proto : prefixe morfcollect/ obligatoire (CONTRAT.md §1.2) ----------
    out.proto = root.value(QStringLiteral("proto")).toString();
    if (!out.proto.startsWith(QStringLiteral("morfcollect/"))) {
        error = QStringLiteral("proto absent ou non 'morfcollect/*' : '%1'").arg(out.proto);
        return false;
    }

    // --- provider : instance requise (domaine de propriete, CONTRAT.md §4.4) -
    const QJsonObject provider = root.value(QStringLiteral("provider")).toObject();
    out.provider.app      = provider.value(QStringLiteral("app")).toString();
    out.provider.instance = provider.value(QStringLiteral("instance")).toString();
    if (out.provider.instance.isEmpty()) {
        error = QStringLiteral("provider.instance requis");
        return false;
    }

    // --- generation + revision (CONTRAT.md §1.4) -----------------------------
    out.generation = root.value(QStringLiteral("manifest_generation")).toString();
    if (out.generation.isEmpty()) {
        error = QStringLiteral("manifest_generation requis");
        return false;
    }
    if (!root.contains(QStringLiteral("revision"))) {
        error = QStringLiteral("revision requise");
        return false;
    }
    out.revision = static_cast<qint64>(root.value(QStringLiteral("revision")).toDouble());

    // --- sources -------------------------------------------------------------
    const QJsonArray sources = root.value(QStringLiteral("sources")).toArray();
    for (const QJsonValue& v : sources) {
        const QJsonObject o = v.toObject();

        ManifestSource s;
        s.sourceId = o.value(QStringLiteral("source_id")).toString();
        if (s.sourceId.isEmpty()) {
            error = QStringLiteral("une source sans source_id");
            return false;
        }

        const QJsonObject connector = o.value(QStringLiteral("connector")).toObject();
        s.connector.name = connector.value(QStringLiteral("name")).toString();
        if (s.connector.name.isEmpty()) {
            error = QStringLiteral("source %1 : connector.name requis").arg(s.sourceId);
            return false;
        }
        s.connector.version = connector.value(QStringLiteral("version")).toInt(1);

        s.label          = o.value(QStringLiteral("label")).toString(s.sourceId);
        s.enabled        = o.value(QStringLiteral("enabled")).toBool(true);
        s.params         = o.value(QStringLiteral("params")).toObject();
        s.credentialsRef = o.value(QStringLiteral("credentials_ref")).toString();
        s.schedule       = o.value(QStringLiteral("schedule")).toObject();
        s.retention      = o.value(QStringLiteral("retention")).toObject();

        out.sources.push_back(s);
    }

    return true;
}

} // namespace morfcollector
