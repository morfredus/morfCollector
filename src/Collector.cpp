/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "morfcollector/Collector.h"
#include "morfcollector/Manifest.h"
#include "morfcollector/IConnector.h"
#include "morfcollector/FsConnector.h"
#ifdef MORFCOLLECTOR_HAVE_SFTP
#  include "morfcollector/SftpConnector.h"
#endif

#include <QTimer>
#include <QDateTime>
#include <QTime>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include <QSet>
#include <utility>

namespace morfcollector {

namespace {
constexpr int kSchedulerTickMs = 30000;   // cadence de verification des echeances

OperationalState opStateFor(IConnector::Outcome o) {
    switch (o) {
        case IConnector::Outcome::AuthFailed:  return OperationalState::AuthFailed;
        case IConnector::Outcome::Unreachable: return OperationalState::Unreachable;
        case IConnector::Outcome::Error:       return OperationalState::Error;
        case IConnector::Outcome::Ok:          return OperationalState::Idle;
    }
    return OperationalState::Error;
}

// Un objet recupere par le thread de travail, en attente d'ecriture (le commit
// dans l'ObjectStore se fait sur le thread principal).
struct Fetched {
    QString    name;
    QByteArray bytes;
    QString    period;
};

// Resultat complet d'un travail de collecte, renvoye au thread principal.
struct CollectResult {
    IConnector::Outcome outcome = IConnector::Outcome::Error;
    QString             error;
    QVector<Fetched>    items;
};

// Travail EXECUTE SUR LE THREAD DE TRAVAIL. Ne touche NI l'ObjectStore NI les
// Source : uniquement le connecteur (stateless) et des donnees copiees. Le
// connecteur est stateless, donc appelable en parallele pour des sources
// differentes.
CollectResult runJob(IConnector* connector, const QJsonObject& params,
                     const QJsonObject& creds, const QSet<QString>& known) {
    CollectResult r;
    QVector<IConnector::RemoteItem> items;
    QString error;

    r.outcome = connector->list(params, creds, items, error);
    if (r.outcome != IConnector::Outcome::Ok) { r.error = error; return r; }

    for (const IConnector::RemoteItem& item : items) {
        if (known.contains(item.name))
            continue;   // deduplication : deja conserve
        QByteArray bytes;
        const IConnector::Outcome fo = connector->fetch(params, creds, item.name, bytes, error);
        if (fo != IConnector::Outcome::Ok) { r.outcome = fo; r.error = error; return r; }
        r.items.push_back({ item.name, bytes, item.period });
    }
    r.outcome = IConnector::Outcome::Ok;
    return r;
}
} // namespace

Collector::Collector(QString storageRoot, QObject* parent)
    : QObject(parent),
      m_store(storageRoot),
      m_vault(storageRoot) {
    m_store.init();
    m_vault.init();

    // Plan de donnees : enregistrement des connecteurs. Chacun declare sa
    // capacite, surfacee dans /status (CONTRAT.md §2, §7.2).
    registerConnector(new FsConnector());
#ifdef MORFCOLLECTOR_HAVE_SFTP
    registerConnector(new SftpConnector());
#endif

    // Ordonnanceur : declenche les collectes planifiees dues, meme fournisseur
    // ferme (autonomie, CONTRAT.md §4.5).
    m_scheduler = new QTimer(this);
    m_scheduler->setInterval(kSchedulerTickMs);
    connect(m_scheduler, &QTimer::timeout, this, &Collector::onSchedulerTick);
    m_scheduler->start();
}

Collector::~Collector() {
    for (IConnector* c : m_connectors)
        delete c;
}

void Collector::registerConnector(IConnector* c) {
    if (c)
        m_connectors.insert(c->name(), c);
}

// --- Manifeste ---------------------------------------------------------------

void Collector::applyManifest(const QJsonObject& root, int& httpCode, QJsonObject& body) {
    Manifest manifest;
    QString  error;
    if (!Manifest::parse(root, manifest, error)) {
        httpCode = 422;
        body = QJsonObject{ {"error", "manifeste non applicable"}, {"detail", error} };
        return;
    }

    const SourceRegistry::ApplyResult r = m_sources.apply(manifest);
    switch (r.status) {
        case SourceRegistry::Apply::Applied:
            httpCode = 200;
            body = QJsonObject{
                {"status", "applied"},
                {"provider", manifest.provider.instance},
                {"generation", r.generation},
                {"revision", static_cast<double>(r.revision)},
                {"detail", r.message},
            };
            break;
        case SourceRegistry::Apply::IgnoredStale:
            httpCode = 409;
            body = QJsonObject{
                {"status", "ignored_stale"},
                {"applied_revision", static_cast<double>(r.revision)},
                {"detail", r.message},
            };
            break;
        case SourceRegistry::Apply::RejectedOwnership:
            httpCode = 409;
            body = QJsonObject{ {"status", "rejected_ownership"}, {"detail", r.message} };
            break;
    }
}

bool Collector::manifestState(const QString& providerInstance, QJsonObject& out) const {
    QString generation;
    qint64  revision = 0;
    if (!m_sources.stateOf(providerInstance, generation, revision))
        return false;
    out = QJsonObject{
        {"provider", providerInstance},
        {"generation", generation},
        {"revision", static_cast<double>(revision)},
    };
    return true;
}

// --- Lecture -----------------------------------------------------------------

QJsonObject Collector::enrich(const Source* s, bool detail) const {
    QJsonObject o = detail ? s->detailJson() : s->summaryJson();
    o["objects"] = static_cast<double>(m_store.objectCountOf(s->id()));
    o["bytes"]   = static_cast<double>(m_store.bytesOf(s->id()));
    return o;
}

QJsonObject Collector::sourcesEnvelope() const {
    QJsonArray arr;
    for (Source* s : const_cast<SourceRegistry&>(m_sources).all())
        arr.append(enrich(s, false));
    return QJsonObject{
        {"sources", arr},
        {"count", m_sources.count()},
        {"ts", static_cast<double>(QDateTime::currentSecsSinceEpoch())},
    };
}

bool Collector::sourceDetail(const QString& id, QJsonObject& out) const {
    const Source* s = m_sources.find(id);
    if (!s)
        return false;
    out = enrich(s, true);
    return true;
}

bool Collector::periods(const QString& id, QJsonArray& out) const {
    if (!m_sources.find(id))
        return false;
    out = QJsonArray{};
    for (const QString& p : m_store.periodsOf(id))
        out.append(p);
    return true;
}

bool Collector::objects(const QString& id, QJsonArray& out) const {
    if (!m_sources.find(id))
        return false;
    out = QJsonArray{};
    for (const CollectedObject& o : m_store.objectsOf(id))
        out.append(o.toJson());
    return true;
}

bool Collector::readObject(const QString& objectId, QByteArray& out, QString& originalName) const {
    return m_store.readObject(objectId, out, originalName);
}

// --- Pipeline de collecte (asynchrone) ---------------------------------------

void Collector::startCollection(Source* s) {
    const QString id = s->id();
    if (m_inFlight.contains(id))
        return;   // une collecte de cette source est deja en cours

    IConnector* connector = m_connectors.value(s->connectorName(), nullptr);
    if (!connector) {
        s->setOperationalState(OperationalState::Error);
        s->setLastError(QStringLiteral("connecteur inconnu : %1").arg(s->connectorName()));
        return;
    }

    // Validation rapide sur le thread principal (pas d'E/S).
    const QJsonObject params = s->params();
    QString error;
    if (!connector->validate(params, error)) {
        s->setOperationalState(OperationalState::Error);
        s->setLastError(error);
        return;
    }

    // Instantane transmis au thread de travail (aucune reference partagee).
    const QJsonObject creds     = m_vault.get(s->credentialsRef());
    const QJsonObject retention = s->retention();
    QSet<QString> known;
    for (const CollectedObject& o : m_store.objectsOf(id))
        known.insert(o.originalName);

    s->setOperationalState(OperationalState::Collecting);
    m_inFlight.insert(id);

    auto* watcher = new QFutureWatcher<CollectResult>(this);
    connect(watcher, &QFutureWatcherBase::finished, this,
            [this, watcher, id, retention]() {
        const CollectResult r = watcher->result();
        watcher->deleteLater();
        m_inFlight.remove(id);

        // La source a pu disparaitre (retrait + suppression) pendant le travail.
        Source* s2 = m_sources.find(id);
        if (!s2)
            return;

        if (r.outcome != IConnector::Outcome::Ok) {
            s2->setOperationalState(opStateFor(r.outcome));
            s2->setLastError(r.error);
            return;
        }

        // Commit sur le thread principal : ecriture dans l'ObjectStore.
        for (const Fetched& f : r.items) {
            bool ok = false;
            m_store.put(id, f.name, f.bytes, f.period, ok);
            if (!ok) {
                s2->setOperationalState(OperationalState::Error);
                s2->setLastError(QStringLiteral("ecriture impossible : %1").arg(f.name));
                return;
            }
        }
        m_store.applyRetention(id, retention);
        s2->setLastCollectTs(QDateTime::currentSecsSinceEpoch());
        s2->clearLastError();
        s2->setOperationalState(OperationalState::Waiting);
    });

    watcher->setFuture(QtConcurrent::run(runJob, connector, params, creds, known));
}

void Collector::onSchedulerTick() {
    const QDateTime nowDt = QDateTime::currentDateTime();
    const qint64    now   = nowDt.toSecsSinceEpoch();
    for (Source* s : m_sources.all()) {
        if (s->adminState() != AdminState::Active)
            continue;

        // Priorite au rendez-vous quotidien "HH:MM" (heure locale) s'il est
        // declare. Collecte une fois par jour, a l'heure cible ou apres (rattrapage
        // si la machine etait eteinte a l'heure dite).
        const QString dailyAt = s->scheduleDailyAt();
        if (!dailyAt.isEmpty()) {
            const QTime t = QTime::fromString(dailyAt, QStringLiteral("HH:mm"));
            if (!t.isValid())
                continue;
            const QDateTime target(nowDt.date(), t);
            if (nowDt >= target && s->lastCollectTs() < target.toSecsSinceEpoch())
                startCollection(s);
            continue;
        }

        // Sinon, intervalle simple.
        const int minutes = s->scheduleMinutes();
        if (minutes > 0 && now - s->lastCollectTs() >= static_cast<qint64>(minutes) * 60)
            startCollection(s);
    }
}

// --- Administration ----------------------------------------------------------

bool Collector::suspend(const QString& id) {
    Source* s = m_sources.find(id);
    if (!s)
        return false;
    s->operatorSuspend();
    return true;
}

bool Collector::resume(const QString& id) {
    Source* s = m_sources.find(id);
    if (!s)
        return false;
    s->operatorResume();
    return true;
}

Collector::Op Collector::collectNow(const QString& id, QJsonObject& out) {
    Source* s = m_sources.find(id);
    if (!s)
        return Op::NotFound;
    if (s->adminState() != AdminState::Active) {
        out = QJsonObject{ {"error", "source non active"},
                           {"admin_state", QString::fromLatin1(toString(s->adminState()))} };
        return Op::Conflict;
    }
    const bool already = m_inFlight.contains(id);
    startCollection(s);   // asynchrone : le resultat vit dans l'etat de la source
    out = QJsonObject{
        {"status", already ? "already_running" : "accepted"},
        {"source_id", id},
        {"detail", "collecte lancee ; suivre l'etat via GET /sources/{id}"},
    };
    return Op::Done;   // -> 202 Accepted
}

bool Collector::deleteObject(const QString& objectId) {
    return m_store.removeObject(objectId);
}

Collector::Op Collector::deleteSourceObjects(const QString& id, int& removed) {
    const bool known = (m_sources.find(id) != nullptr);
    const bool hasObjects = (m_store.objectCountOf(id) > 0);
    if (!known && !hasObjects)
        return Op::NotFound;
    removed = m_store.removeAll(id);
    return Op::Done;
}

Collector::Op Collector::deleteSource(const QString& id) {
    Source* s = m_sources.find(id);
    if (!s)
        return Op::NotFound;
    if (s->adminState() != AdminState::Retired)
        return Op::Conflict;   // seule une source RETIRED se supprime (CONTRAT.md §6.3)
    m_store.removeAll(id);
    m_sources.remove(id);
    return Op::Done;
}

// --- Coffre ------------------------------------------------------------------

bool Collector::storeCredentials(const QString& ref, const QJsonObject& secret) {
    if (ref.isEmpty() || secret.isEmpty())
        return false;
    // Remise unique dans le coffre CHIFFRE au repos (CONTRAT.md §1.5). Jamais
    // journalise, jamais renvoye par aucune route.
    return m_vault.put(ref, secret);
}

// --- Capacites / metriques ---------------------------------------------------

QStringList Collector::capabilities() const {
    QStringList caps = m_connectors.keys();        // connecteurs reellement disponibles
    caps << QStringLiteral("delete") << QStringLiteral("retention");
    caps.sort();
    return caps;
}

QJsonObject Collector::metrics() const {
    QJsonObject m = m_sources.metrics();
    m["objects"]          = static_cast<double>(m_store.objectCount());
    m["bytes"]            = static_cast<double>(m_store.totalBytes());
    m["last_collect_ts"]  = static_cast<double>(m_store.lastCollectTs());
    m["credentials_refs"] = m_vault.size();
    return m;
}

QString Collector::state() const {
    return QStringLiteral("ok");
}

} // namespace morfcollector
