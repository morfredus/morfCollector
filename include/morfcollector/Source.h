/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QString>
#include <QJsonObject>

#include "morfcollector/Domain.h"
#include "morfcollector/Manifest.h"

// -----------------------------------------------------------------------------
// Source : l'etat RUNTIME d'une source cote morfCollector (CONTRAT.md §3, §4.3.1).
//
// Elle combine :
//   - sa declaration issue du manifeste (ManifestSource) ;
//   - le triplet de suspension conserve separement :
//       * m_manifestEnabled   : ce que declare le dernier manifeste ;
//       * m_operatorSuspended  : decision operateur explicite (suspend/resume) ;
//       * l'etat effectif, DERIVE des deux ci-dessus + presence ;
//   - son etat operationnel, l'horodatage de derniere collecte et la derniere
//     erreur.
//
// Les COMPTEURS d'objets (nombre, octets) ne sont pas stockes ici : l'ObjectStore
// en est la seule source de verite. Le Collector les injecte dans les vues JSON.
// -----------------------------------------------------------------------------
namespace morfcollector {

class Source {
public:
    Source() = default;
    explicit Source(const ManifestSource& def, const QString& providerInstance);

    // --- Identite / propriete / definition ----------------------------------
    QString id() const               { return m_def.sourceId; }
    QString providerInstance() const { return m_providerInstance; }
    const ManifestSource& definition() const { return m_def; }
    QString     connectorName() const { return m_def.connector.name; }
    QJsonObject params() const        { return m_def.params; }
    QString     credentialsRef() const { return m_def.credentialsRef; }
    QJsonObject retention() const     { return m_def.retention; }
    int         scheduleMinutes() const;   // schedule.every_minutes (0 si absent)
    // schedule.daily_at ("HH:MM", heure locale) : collecte une fois par jour.
    // Vide si absent (on retombe alors sur every_minutes).
    QString     scheduleDailyAt() const { return m_def.schedule.value(QStringLiteral("daily_at")).toString(); }

    // --- Reconciliation ------------------------------------------------------
    void applyDefinition(const ManifestSource& def);  // ne touche PAS l'override
    void retire();
    void unretire();

    // --- Override operateur (admin) -----------------------------------------
    void operatorSuspend() { m_operatorSuspended = true; }
    void operatorResume()  { m_operatorSuspended = false; }
    bool operatorSuspended() const { return m_operatorSuspended; }

    // --- Etats ---------------------------------------------------------------
    bool             present() const { return m_present; }
    AdminState       adminState() const;
    OperationalState operationalState() const { return m_operational; }
    void             setOperationalState(OperationalState s) { m_operational = s; }

    // --- Suivi de collecte ---------------------------------------------------
    qint64 lastCollectTs() const { return m_lastCollectTs; }
    void   setLastCollectTs(qint64 ts) { m_lastCollectTs = ts; }
    void   setLastError(const QString& e) { m_lastError = e; }
    void   clearLastError() { m_lastError.clear(); }

    // --- Vues JSON (etat seul ; compteurs injectes par le Collector) ---------
    QJsonObject summaryJson() const;  // GET /sources (liste)
    QJsonObject detailJson() const;   // GET /sources/{id} (dont le triplet)

private:
    ManifestSource   m_def;
    QString          m_providerInstance;

    bool             m_present           = true;
    bool             m_manifestEnabled   = true;
    bool             m_operatorSuspended = false;

    OperationalState m_operational   = OperationalState::Idle;
    qint64           m_lastCollectTs = 0;   // 0 = jamais
    QString          m_lastError;
};

} // namespace morfcollector
