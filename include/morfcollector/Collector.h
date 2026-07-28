/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>

#include "morfbeacon/IMetricsProvider.h"
#include "morfcollector/SourceRegistry.h"
#include "morfcollector/ObjectStore.h"
#include "morfcollector/Vault.h"

class QTimer;

// -----------------------------------------------------------------------------
// Collector : le MOTEUR du service (plan de controle + plan de donnees).
//   - plan de controle : SourceRegistry (manifeste, etats) ;
//   - plan de donnees   : connecteurs (IConnector), pipeline de collecte,
//                         ObjectStore (conservation + index), retention ;
//   - ordonnanceur      : declenche les collectes planifiees, meme fournisseur
//                         ferme (autonomie, CONTRAT.md §4.5).
// Implemente morfbeacon::IMetricsProvider (resume pour /status et heartbeat).
// -----------------------------------------------------------------------------
namespace morfcollector {

class IConnector;

class Collector : public QObject, public morfbeacon::IMetricsProvider {
    Q_OBJECT
public:
    // dataRoot  : objets collectes + index (source de verite).
    // vaultRoot : coffre de secrets, distinct de dataRoot (la cle ne doit jamais
    //             voyager avec une copie des donnees).
    explicit Collector(QString dataRoot, QString vaultRoot, QObject* parent = nullptr);
    ~Collector() override;

    // --- Manifeste (CONTRAT.md §4) ------------------------------------------
    void applyManifest(const QJsonObject& root, int& httpCode, QJsonObject& body);
    bool manifestState(const QString& providerInstance, QJsonObject& out) const;

    // --- Lecture -------------------------------------------------------------
    QJsonObject sourcesEnvelope() const;
    bool        sourceDetail(const QString& id, QJsonObject& out) const;
    bool        periods(const QString& id, QJsonArray& out) const;
    bool        objects(const QString& id, QJsonArray& out) const;
    bool        readObject(const QString& objectId, QByteArray& out, QString& originalName) const;

    // --- Administration (CONTRAT.md §6.3) -----------------------------------
    enum class Op { NotFound, Done, Conflict };
    bool suspend(const QString& id);
    bool resume(const QString& id);
    Op   collectNow(const QString& id, QJsonObject& out);
    bool deleteObject(const QString& objectId);
    Op   deleteSourceObjects(const QString& id, int& removed);
    Op   deleteSource(const QString& id);

    // --- Coffre de secrets (CONTRAT.md §1.5) --------------------------------
    bool storeCredentials(const QString& ref, const QJsonObject& secret);

    // true si le coffre est operationnel. false = coffre indisponible (dossier
    // non accessible en ecriture) : aucune remise de secret ne peut aboutir.
    bool vaultReady() const { return m_vault.ready(); }

    // --- Capacites annoncees (CONTRAT.md §7.2) ------------------------------
    QStringList capabilities() const;

    // --- morfbeacon::IMetricsProvider ---------------------------------------
    QJsonObject metrics() const override;
    QString     state() const override;

private:
    void registerConnector(IConnector* c);   // prend possession
    // Lance une collecte ASYNCHRONE (thread de travail). No-op si une collecte de
    // cette source est deja en cours. Le commit (ecriture dans l'ObjectStore, mise
    // a jour de l'etat) se fait sur le thread principal, a la fin du travail.
    void startCollection(Source* s);
    void onSchedulerTick();                  // collectes planifiees dues
    QJsonObject enrich(const Source* s, bool detail) const;  // + stats du store

    SourceRegistry               m_sources;
    ObjectStore                  m_store;
    Vault                        m_vault;           // coffre chiffre des secrets
    QHash<QString, IConnector*>  m_connectors;      // name -> connecteur
    QSet<QString>                m_inFlight;         // sources en cours de collecte
    QTimer*                      m_scheduler = nullptr;
};

} // namespace morfcollector
