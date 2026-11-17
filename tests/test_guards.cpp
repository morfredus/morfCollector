/*
 * morfCollector - tests des garde-fous purs et de la durabilite du stockage.
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "morfcollector/Guards.h"
#include "morfcollector/ObjectStore.h"
#include "morfcollector/Vault.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>

using namespace morfcollector;

static int g_failed = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("ECHEC ligne %d : %s\n", __LINE__, #cond); ++g_failed; } } while (0)

static void touch(const QString& path, const QByteArray& data = "x") {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    // open() est [[nodiscard]] : sans fichier, les CHECK qui suivent echouent d'eux-memes.
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(data);
}

static void testPathUnder() {
    CHECK(isPathUnder("/data", "/data"));
    CHECK(isPathUnder("/data/a/b", "/data"));
    CHECK(isPathUnder("/data/a", "/data/"));
    CHECK(!isPathUnder("/data2", "/data"));     // separateur final obligatoire
    CHECK(!isPathUnder("/dat", "/data"));
    CHECK(isPathUnder("/x", "/"));
    CHECK(!isPathUnder("", "/data"));
}

static void testFsDir() {
    QTemporaryDir tmp;
    const QString base = tmp.path();
    QDir().mkpath(base + "/share/logs");
    QDir().mkpath(base + "/share2");
    QDir().mkpath(base + "/share/vault");
    QString err;

    // Pas de liste blanche => refus par defaut.
    CHECK(validateFsDir(base + "/share", {}, {}, err).isEmpty());
    CHECK(!err.isEmpty());

    const QStringList roots{ base + "/share" };
    CHECK(!validateFsDir(base + "/share/logs", roots, {}, err).isEmpty());
    CHECK(!validateFsDir(base + "/share", roots, {}, err).isEmpty());
    // Hors racine, dont le faux ami "share2" (meme prefixe sans separateur).
    CHECK(validateFsDir(base + "/share2", roots, {}, err).isEmpty());
    CHECK(validateFsDir(base, roots, {}, err).isEmpty());
    // Remontee par "..".
    CHECK(validateFsDir(base + "/share/../share2", roots, {}, err).isEmpty());
    // Inexistant.
    CHECK(validateFsDir(base + "/share/nope", roots, {}, err).isEmpty());
    // Coffre interdit meme sous une racine autorisee.
    CHECK(validateFsDir(base + "/share/vault", roots, { base + "/share/vault" }, err).isEmpty());
    CHECK(!validateFsDir(base + "/share/logs", roots, { base + "/share/vault" }, err).isEmpty());

#ifndef Q_OS_WIN   // sous Windows QFile::link cree un .lnk, pas un vrai lien symbolique
    if (QFile::link(base + "/share2", base + "/share/lien")) {
        CHECK(validateFsDir(base + "/share/lien", roots, {}, err).isEmpty());
    }
#endif
}

static void testOrigin() {
    CHECK(!isCrossOrigin("", "192.168.1.5:8792"));                       // curl, SiteWatch
    CHECK(!isCrossOrigin("http://192.168.1.5:8792", "192.168.1.5:8792"));
    CHECK(!isCrossOrigin("HTTP://Pi4Fred:8792", "pi4fred:8792"));
    CHECK(isCrossOrigin("http://evil.example", "192.168.1.5:8792"));
    CHECK(isCrossOrigin("http://192.168.1.5:9999", "192.168.1.5:8792"));  // autre port
    CHECK(isCrossOrigin("null", "192.168.1.5:8792"));
}

static void testFileName() {
    CHECK(sanitizeFileName("a b\"c\r\nd.gz") == "a_b_c__d.gz");
    CHECK(sanitizeFileName("") == "object");
    const QByteArray cd = contentDisposition("x\"\r\nSet-Cookie: a=b");
    CHECK(!cd.contains('\r') && !cd.contains('\n'));
    CHECK(cd.startsWith("attachment; filename=\""));
}

static void testHostKey() {
    QByteArray raw(32, '\0');
    for (int i = 0; i < 32; ++i) raw[i] = char(i);
    const QString hex = QString::fromLatin1(raw.toHex());
    CHECK(hostKeyMatches(raw, hex));
    CHECK(hostKeyMatches(raw, hex.toUpper()));
    QString colon; for (int i = 0; i < 64; i += 2) colon += (i ? ":" : "") + hex.mid(i, 2);
    CHECK(hostKeyMatches(raw, colon));
    CHECK(hostKeyMatches(raw, "SHA256:" + QString::fromLatin1(raw.toBase64(QByteArray::OmitTrailingEquals))));
    QByteArray other = raw; other[5] = 0x7f;
    CHECK(!hostKeyMatches(other, hex));
    CHECK(!hostKeyMatches(raw, ""));
    CHECK(!hostKeyMatches(QByteArray(), hex));
}

static void testObjectStoreCorruptIndex() {
    QTemporaryDir tmp;
    {
        ObjectStore st(tmp.path());
        CHECK(st.init());
        bool ok = false;
        st.put("src", "site.log-20260101.gz", "contenu", "2026-01-01", ok);
        CHECK(ok);
    }
    touch(tmp.path() + "/index.json", "{ pas du json");
    ObjectStore st(tmp.path());
    CHECK(st.init());                              // reconstruit, jamais "vide"
    CHECK(st.objectCountOf("src") == 1);
    CHECK(QFile::exists(tmp.path() + "/index.json.corrupt"));
}

static void testVaultCorrupt() {
    QTemporaryDir tmp;
    {
        Vault v(tmp.path());
        CHECK(v.init());
        CHECK(v.put("ref", QJsonObject{ {"password", "s"} }));
    }
    Vault again(tmp.path());
    CHECK(again.init());
    CHECK(again.contains("ref"));
    // Corruption : le coffre ne repart PAS vide et reste indisponible.
    touch(tmp.path() + "/vault.enc", "garbage garbage garbage garbage garbage garbage");
    Vault broken(tmp.path());
    CHECK(!broken.init());
    CHECK(!broken.ready());
    CHECK(!broken.put("x", QJsonObject{ {"a", 1} }));
    CHECK(QFile::exists(tmp.path() + "/vault.enc.bak"));
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    testPathUnder();
    testFsDir();
    testOrigin();
    testFileName();
    testHostKey();
    testObjectStoreCorruptIndex();
    testVaultCorrupt();
    if (g_failed) { std::printf("%d verification(s) en echec\n", g_failed); return 1; }
    std::printf("OK\n");
    return 0;
}
