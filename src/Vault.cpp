/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "morfcollector/Vault.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QDebug>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <utility>

#ifdef Q_OS_UNIX
#  include <sys/stat.h>
#endif

namespace morfcollector {

namespace {
constexpr int kKeyLen   = 32;   // AES-256
constexpr int kNonceLen = 12;   // GCM standard
constexpr int kTagLen   = 16;   // GCM tag
} // namespace

Vault::Vault(QString root) : m_root(std::move(root)) {}

bool Vault::init() {
    if (m_root.isEmpty())
        return false;
    QDir().mkpath(m_root);
    if (!loadKey())
        return false;
    // Un coffre ABSENT demarre vide ; un coffre PRESENT mais indechiffrable (cle
    // changee, fichier corrompu) ne doit JAMAIS repartir vide : le prochain put()
    // l'ecraserait et detruirait les secrets restants. On garde une copie .bak et on
    // reste indisponible (m_ready=false => 503 explicite sur POST /credentials).
    if (!load()) {
        const QString enc = QDir(m_root).filePath(QStringLiteral("vault.enc"));
        const QString bak = enc + QStringLiteral(".bak");
        if (!QFile::exists(bak))
            QFile::copy(enc, bak);
        m_secrets.clear();
        qWarning().noquote() << "morfCollector : coffre illisible, service coffre desactive"
                                " (copie conservee dans" << bak << ")";
        return false;
    }
    m_ready = true;
    return true;
}

// --- Cle ---------------------------------------------------------------------

bool Vault::loadKey() {
    const QString keyPath = QDir(m_root).filePath(QStringLiteral("vault.key"));
    QFile kf(keyPath);
    if (kf.exists()) {
        if (!kf.open(QIODevice::ReadOnly))
            return false;
        m_key = kf.readAll();
        return m_key.size() == kKeyLen;
    }
    // Coffre present sans sa cle : en generer une neuve rendrait le coffre
    // definitivement indechiffrable. Refuser plutot que de masquer la perte.
    if (QFile::exists(QDir(m_root).filePath(QStringLiteral("vault.enc")))) {
        qWarning() << "morfCollector : vault.enc present mais vault.key absente";
        return false;
    }
    // Premiere fois : generer une cle aleatoire et la conserver, permissions
    // restreintes au proprietaire.
    m_key.resize(kKeyLen);
    if (RAND_bytes(reinterpret_cast<unsigned char*>(m_key.data()), kKeyLen) != 1)
        return false;
#ifdef Q_OS_UNIX
    const mode_t oldMask = umask(077);   // aucun instant ou la cle serait lisible par d'autres
#endif
    // QSaveFile : ecriture dans un fichier temporaire (0600) puis renommage ; les
    // permissions sont posees AVANT commit, la cle n'est jamais visible en 0644.
    QSaveFile sf(keyPath);
    bool ok = sf.open(QIODevice::WriteOnly)
        && sf.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        && sf.write(m_key) == m_key.size()
        && sf.commit();
#ifdef Q_OS_UNIX
    umask(oldMask);
#endif
    return ok;
}

// --- Chiffrement AES-256-GCM -------------------------------------------------

bool Vault::encrypt(const QByteArray& plain, QByteArray& out) const {
    QByteArray nonce(kNonceLen, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(nonce.data()), kNonceLen) != 1)
        return false;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return false;

    bool ok = false;
    QByteArray cipher(plain.size(), '\0');
    QByteArray tag(kTagLen, '\0');
    int len = 0, total = 0;

    do {
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceLen, nullptr) != 1) break;
        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                reinterpret_cast<const unsigned char*>(m_key.constData()),
                reinterpret_cast<const unsigned char*>(nonce.constData())) != 1) break;
        if (EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(cipher.data()), &len,
                reinterpret_cast<const unsigned char*>(plain.constData()), plain.size()) != 1) break;
        total = len;
        if (EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(cipher.data()) + len, &len) != 1) break;
        total += len;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagLen,
                reinterpret_cast<unsigned char*>(tag.data())) != 1) break;
        ok = true;
    } while (false);

    EVP_CIPHER_CTX_free(ctx);
    if (!ok)
        return false;

    cipher.resize(total);
    out = nonce + tag + cipher;   // format : nonce(12) | tag(16) | ciphertext
    return true;
}

bool Vault::decrypt(const QByteArray& blob, QByteArray& out) const {
    if (blob.size() < kNonceLen + kTagLen)
        return false;
    const QByteArray nonce  = blob.left(kNonceLen);
    const QByteArray tag    = blob.mid(kNonceLen, kTagLen);
    const QByteArray cipher = blob.mid(kNonceLen + kTagLen);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return false;

    bool ok = false;
    QByteArray plain(cipher.size(), '\0');
    int len = 0, total = 0;

    do {
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceLen, nullptr) != 1) break;
        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                reinterpret_cast<const unsigned char*>(m_key.constData()),
                reinterpret_cast<const unsigned char*>(nonce.constData())) != 1) break;
        if (EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(plain.data()), &len,
                reinterpret_cast<const unsigned char*>(cipher.constData()), cipher.size()) != 1) break;
        total = len;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagLen,
                const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(tag.constData()))) != 1) break;
        // Final verifie le tag : echoue si la cle est mauvaise ou les donnees alterees.
        if (EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(plain.data()) + len, &len) != 1) break;
        total += len;
        ok = true;
    } while (false);

    EVP_CIPHER_CTX_free(ctx);
    if (!ok)
        return false;

    plain.resize(total);
    out = plain;
    return true;
}

// --- Persistance -------------------------------------------------------------

bool Vault::load() {
    m_secrets.clear();
    QFile f(QDir(m_root).filePath(QStringLiteral("vault.enc")));
    if (!f.exists())
        return true;
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray blob = f.readAll();
    QByteArray plain;
    if (!decrypt(blob, plain))
        return false;   // cle changee ou coffre corrompu : l'appelant NE repart PAS de vide
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(plain, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    const QJsonObject root = doc.object();
    for (auto it = root.constBegin(); it != root.constEnd(); ++it)
        m_secrets.insert(it.key(), it.value().toObject());
    return true;
}

bool Vault::save() {
    if (!m_ready)
        return false;
    QJsonObject root;
    for (auto it = m_secrets.constBegin(); it != m_secrets.constEnd(); ++it)
        root.insert(it.key(), it.value());
    const QByteArray plain = QJsonDocument(root).toJson(QJsonDocument::Compact);
    QByteArray blob;
    if (!encrypt(plain, blob))
        return false;
    const QString path = QDir(m_root).filePath(QStringLiteral("vault.enc"));
    // QSaveFile : un disque plein ou un arret en cours d'ecriture laisse l'ancien
    // vault.enc intact (renommage atomique au commit). Permissions AVANT commit.
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)
        || !f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        || f.write(blob) != blob.size())
        return false;
    return f.commit();
}

// --- API interne -------------------------------------------------------------

bool Vault::put(const QString& ref, const QJsonObject& secret) {
    if (!m_ready)
        return false;
    m_secrets.insert(ref, secret);
    return save();
}

QJsonObject Vault::get(const QString& ref) const {
    return m_secrets.value(ref);
}

bool Vault::contains(const QString& ref) const {
    return m_secrets.contains(ref);
}

} // namespace morfcollector
