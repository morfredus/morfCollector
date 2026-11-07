/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "morfcollector/ObjectStore.h"
#include "morfcollector/Guards.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QSaveFile>
#include <QJsonParseError>
#include <QDebug>

#include <algorithm>
#include <utility>

namespace morfcollector {

namespace {
// Nom de fichier sur disque, derive d'un nom d'origine potentiellement hostile.
QString safeName(const QString& name) { return sanitizeFileName(name); }
} // namespace

ObjectStore::ObjectStore(QString root) : m_root(std::move(root)) {}

QString ObjectStore::sourceDir(const QString& sourceId) const {
    return QDir(m_root).filePath(QStringLiteral("objects/") + safeName(sourceId));
}

bool ObjectStore::init() {
    if (m_root.isEmpty())
        return false;
    QDir().mkpath(QDir(m_root).filePath(QStringLiteral("objects")));
    m_ready = loadIndex();
    if (!m_ready)
        qWarning().noquote() << "morfCollector : index des objets inutilisable :" << m_error;
    return m_ready;
}

CollectedObject ObjectStore::put(const QString& sourceId, const QString& originalName,
                                 const QByteArray& bytes, const QString& period, bool& ok) {
    if (!m_ready) { ok = false; return CollectedObject(); }   // index non charge : ne rien ecraser
    // Upsert par (source, nom d'origine) : chez la source, un fichier NOMME est UNE
    // entite dont on conserve le DERNIER etat. Les .gz surveilles sont append-only
    // (un log mensuel qui se complete au fil de l'eau) : recollecter un nom deja
    // connu doit REMPLACER l'objet, jamais en creer un second - sinon on empilerait
    // une copie par nuit (duplication). On reutilise l'object_id existant : la
    // reference reste stable pour le client qui a deja telecharge cet objet.
    QString reuseId;
    for (const CollectedObject& e : m_objects) {
        if (e.sourceId == sourceId && e.originalName == originalName) {
            reuseId = e.objectId;
            break;
        }
    }

    CollectedObject o;
    o.objectId     = reuseId.isEmpty()
        ? QUuid::createUuid().toString(QUuid::WithoutBraces) : reuseId;
    o.sourceId     = sourceId;
    o.originalName = originalName;
    o.size         = bytes.size();
    o.hash         = QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    o.collectedAt  = QDateTime::currentSecsSinceEpoch();
    o.period       = period;

    const QString dir = sourceDir(sourceId);
    QDir().mkpath(dir);
    // Le chemin derive de l'object_id : reutiliser l'id => on reecrit le meme
    // fichier (Truncate), l'ancien contenu est remplace sans laisser d'orphelin.
    o.storedPath = QDir(dir).filePath(o.objectId + QStringLiteral("__") + safeName(originalName));

    // QSaveFile : si l'ecriture echoue (disque plein), l'ancien etat de cet objet
    // reste intact au lieu d'etre tronque.
    QSaveFile f(o.storedPath);
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit()) {
        ok = false;
        return o;
    }

    m_objects.insert(o.objectId, o);
    ok = writeIndex();
    return o;
}

QVector<CollectedObject> ObjectStore::objectsOf(const QString& sourceId) const {
    QVector<CollectedObject> v;
    for (const CollectedObject& o : m_objects)
        if (o.sourceId == sourceId)
            v.push_back(o);
    std::sort(v.begin(), v.end(), [](const CollectedObject& a, const CollectedObject& b) {
        return a.collectedAt < b.collectedAt;
    });
    return v;
}

QStringList ObjectStore::periodsOf(const QString& sourceId) const {
    QSet<QString> set;
    for (const CollectedObject& o : m_objects)
        if (o.sourceId == sourceId && !o.period.isEmpty())
            set.insert(o.period);
    QStringList l = set.values();
    l.sort();
    return l;
}

bool ObjectStore::readObject(const QString& objectId, QByteArray& out, QString& originalName) const {
    auto it = m_objects.constFind(objectId);
    if (it == m_objects.constEnd())
        return false;
    QFile f(it->storedPath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    out = f.readAll();
    originalName = it->originalName;
    return true;
}

bool ObjectStore::contains(const QString& objectId) const {
    return m_objects.contains(objectId);
}

bool ObjectStore::removeObject(const QString& objectId) {
    auto it = m_objects.constFind(objectId);
    if (it == m_objects.constEnd())
        return false;
    QFile::remove(it->storedPath);
    m_objects.erase(it);
    writeIndex();
    return true;
}

int ObjectStore::removeAll(const QString& sourceId) {
    const QVector<CollectedObject> victims = objectsOf(sourceId);
    for (const CollectedObject& o : victims) {
        QFile::remove(o.storedPath);
        m_objects.remove(o.objectId);
    }
    if (!victims.isEmpty())
        writeIndex();
    return victims.size();
}

int ObjectStore::applyRetention(const QString& sourceId, const QJsonObject& policy) {
    const QString mode = policy.value(QStringLiteral("mode")).toString(QStringLiteral("keep_forever"));
    if (mode == QLatin1String("keep_forever") || mode.isEmpty())
        return 0;

    QVector<CollectedObject> objs = objectsOf(sourceId);   // tries par collectedAt croissant
    int removed = 0;

    if (mode == QLatin1String("keep_days")) {
        const int days = policy.value(QStringLiteral("days")).toInt(0);
        if (days <= 0)
            return 0;
        const qint64 cutoff = QDateTime::currentSecsSinceEpoch() - static_cast<qint64>(days) * 86400;
        for (const CollectedObject& o : objs) {
            // On se base sur la periode couverte si connue, sinon la date de collecte.
            const qint64 ref = o.collectedAt;
            if (ref < cutoff) {
                QFile::remove(o.storedPath);
                m_objects.remove(o.objectId);
                ++removed;
            }
        }
    } else if (mode == QLatin1String("keep_size")) {
        const qint64 maxBytes = static_cast<qint64>(policy.value(QStringLiteral("max_bytes")).toDouble(0));
        if (maxBytes <= 0)
            return 0;
        qint64 total = bytesOf(sourceId);
        // Purge les plus anciens jusqu'a repasser sous le plafond.
        for (const CollectedObject& o : objs) {
            if (total <= maxBytes)
                break;
            QFile::remove(o.storedPath);
            m_objects.remove(o.objectId);
            total -= o.size;
            ++removed;
        }
    }

    if (removed > 0)
        writeIndex();
    return removed;
}

qint64 ObjectStore::totalBytes() const {
    qint64 t = 0;
    for (const CollectedObject& o : m_objects)
        t += o.size;
    return t;
}

qint64 ObjectStore::lastCollectTs() const {
    qint64 last = 0;
    for (const CollectedObject& o : m_objects)
        last = std::max(last, o.collectedAt);
    return last;
}

qint64 ObjectStore::objectCountOf(const QString& sourceId) const {
    qint64 n = 0;
    for (const CollectedObject& o : m_objects)
        if (o.sourceId == sourceId)
            ++n;
    return n;
}

qint64 ObjectStore::bytesOf(const QString& sourceId) const {
    qint64 t = 0;
    for (const CollectedObject& o : m_objects)
        if (o.sourceId == sourceId)
            t += o.size;
    return t;
}

// --- Persistance de l'index --------------------------------------------------

bool ObjectStore::writeIndex() const {
    QJsonArray arr;
    for (const CollectedObject& o : m_objects) {
        QJsonObject j = o.toJson();
        j["stored_path"] = o.storedPath;   // interne : present dans l'index, jamais dans l'API
        arr.append(j);
    }
    const QByteArray data = QJsonDocument(arr).toJson(QJsonDocument::Indented);
    // QSaveFile : l'index n'est jamais vu tronque, meme en cas de coupure.
    QSaveFile f(QDir(m_root).filePath(QStringLiteral("index.json")));
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size())
        return false;
    return f.commit();
}

bool ObjectStore::loadIndex() {
    m_objects.clear();
    QFile f(QDir(m_root).filePath(QStringLiteral("index.json")));
    if (!f.exists())
        return true;   // rien a charger : index vide, pas une erreur
    if (!f.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("index.json illisible : %1").arg(f.errorString());
        return false;
    }
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isArray()) {
        m_error = QStringLiteral("index.json invalide (%1, offset %2)")
                      .arg(pe.errorString()).arg(pe.offset);
        // Garder la preuve, puis reconstruire plutot que repartir d'un index vide.
        const QString path = f.fileName();
        f.close();
        QFile::remove(path + QStringLiteral(".corrupt"));
        QFile::rename(path, path + QStringLiteral(".corrupt"));
        qWarning().noquote() << "morfCollector :" << m_error << "- reconstruction depuis les fichiers";
        return rebuildIndex();
    }
    for (const QJsonValue& v : doc.array()) {
        const QJsonObject j = v.toObject();
        CollectedObject o;
        o.objectId     = j.value("object_id").toString();
        o.sourceId     = j.value("source_id").toString();
        o.originalName = j.value("original_name").toString();
        o.storedPath   = j.value("stored_path").toString();
        o.size         = static_cast<qint64>(j.value("size").toDouble());
        o.hash         = j.value("hash").toString();
        o.collectedAt  = static_cast<qint64>(j.value("collected_at").toDouble());
        o.period       = j.value("period").toString();
        if (!o.objectId.isEmpty())
            m_objects.insert(o.objectId, o);
    }
    return true;
}

// Reconstruction best-effort : le nom d'origine n'est connu que sous sa forme
// assainie (les caracteres exotiques sont deja perdus dans le nom de fichier), la
// periode n'est pas recalculee. Mieux qu'un index vide qui orphelinerait tout.
bool ObjectStore::rebuildIndex() {
    m_objects.clear();
    const QDir objects(QDir(m_root).filePath(QStringLiteral("objects")));
    for (const QFileInfo& sd : objects.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QFileInfoList files = QDir(sd.absoluteFilePath()).entryInfoList(QDir::Files);
        for (const QFileInfo& fi : files) {
            const int sep = fi.fileName().indexOf(QStringLiteral("__"));
            if (sep <= 0)
                continue;
            QFile f(fi.absoluteFilePath());
            if (!f.open(QIODevice::ReadOnly))
                continue;
            CollectedObject o;
            o.objectId     = fi.fileName().left(sep);
            o.sourceId     = sd.fileName();
            o.originalName = fi.fileName().mid(sep + 2);
            o.storedPath   = fi.absoluteFilePath();
            o.size         = fi.size();
            o.hash         = QString::fromLatin1(
                QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex());
            o.collectedAt  = fi.lastModified().toSecsSinceEpoch();
            m_objects.insert(o.objectId, o);
        }
    }
    if (!writeIndex()) {
        m_error = QStringLiteral("reconstruction de l'index impossible (ecriture)");
        return false;
    }
    return true;
}

} // namespace morfcollector
