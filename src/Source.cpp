/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "morfcollector/Source.h"

namespace morfcollector {

Source::Source(const ManifestSource& def, const QString& providerInstance)
    : m_def(def),
      m_providerInstance(providerInstance),
      m_present(true),
      m_manifestEnabled(def.enabled) {}

int Source::scheduleMinutes() const {
    return m_def.schedule.value(QStringLiteral("every_minutes")).toInt(0);
}

void Source::applyDefinition(const ManifestSource& def) {
    // L'identite (source_id) ne change jamais ; on ne recopie que le reste.
    m_def             = def;
    m_present         = true;
    m_manifestEnabled = def.enabled;
    // m_operatorSuspended : deliberement inchange (persistance de l'override).
}

void Source::retire() {
    m_present     = false;
    m_operational = OperationalState::Idle;
}

void Source::unretire() {
    m_present = true;
}

AdminState Source::adminState() const {
    if (!m_present)
        return AdminState::Retired;
    if (!m_manifestEnabled || m_operatorSuspended)
        return AdminState::Suspended;
    return AdminState::Active;
}

QJsonObject Source::summaryJson() const {
    QJsonObject o;
    o["source_id"]   = m_def.sourceId;
    o["label"]       = m_def.label;
    o["admin_state"] = QString::fromLatin1(toString(adminState()));
    if (adminState() == AdminState::Active)
        o["operational_state"] = QString::fromLatin1(toString(m_operational));
    o["connector"]        = m_def.connector.name;
    o["last_collect_ts"]  = static_cast<double>(m_lastCollectTs);
    if (!m_lastError.isEmpty())
        o["last_error"] = m_lastError;
    return o;
}

QJsonObject Source::detailJson() const {
    QJsonObject o = summaryJson();

    // Le triplet de suspension, TOUJOURS expose pour bannir l'etat fantome
    // (CONTRAT.md §4.3.1).
    o["manifest_enabled"]  = m_manifestEnabled;
    o["operator_override"] = m_operatorSuspended ? QStringLiteral("suspended")
                                                 : QJsonValue(QJsonValue::Null);
    o["effective_state"]   = QString::fromLatin1(toString(adminState()));

    o["provider_instance"] = m_providerInstance;

    QJsonObject connector;
    connector["name"]    = m_def.connector.name;
    connector["version"] = m_def.connector.version;
    o["connector_ref"]   = connector;

    // params non sensibles (coordonnees de la source) : lisibles sous confiance
    // LAN par l'API de detail, mais JAMAIS annonces (voir /status, CONTRAT.md §7.3).
    o["params"]    = m_def.params;
    o["schedule"]  = m_def.schedule;
    o["retention"] = m_def.retention;

    return o;
}

} // namespace morfcollector
