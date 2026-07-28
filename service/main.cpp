/*
 * morfCollector — demon de service
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Charge une configuration JSON (reglages machine), ouvre l'API HTTP et annonce
 * sa presence sur le LAN (morfBeacon, capacite "collection"). Les sources a
 * collecter n'arrivent PAS de la config : elles sont poussees par le fournisseur
 * via POST /manifest (contrat morfcollect/1, voir docs/fr/CONTRAT.md).
 */

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTextStream>

#include <morfcollector/Service.h>
#include <morfcollector/Version.h>

using morfcollector::ServiceConfig;

namespace {

QTextStream& out() { static QTextStream s(stdout); return s; }
QTextStream& err() { static QTextStream s(stderr); return s; }

QString findDefaultConfig() {
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir::current().filePath("morfcollector.json"),
        QDir(exeDir).filePath("morfcollector.json"),
        QDir(exeDir).filePath("config/morfcollector.json"),
#ifdef Q_OS_UNIX
        QStringLiteral("/etc/morfsystem/morfcollector/morfcollector.json"),
        QStringLiteral("/etc/morfcollector/morfcollector.json"),   // ancien emplacement (avant le regroupement sous /etc/morfsystem)
#endif
    };
    for (const QString& c : candidates)
        if (QFileInfo::exists(c))
            return c;
    return {};
}

bool loadConfig(const QString& path, ServiceConfig* outCfg, QString* error) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("impossible d'ouvrir %1 : %2").arg(path, f.errorString());
        return false;
    }
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        *error = QStringLiteral("JSON invalide dans %1 : %2").arg(path, pe.errorString());
        return false;
    }
    *outCfg = ServiceConfig::fromJson(doc.object());
    return true;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("morfCollector"));
    QCoreApplication::setApplicationVersion(morfcollector::version());

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("morfCollector — collecte et conservation locale de "
                       "ressources distantes temporaires (contrat morfcollect/1)."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption configOpt({"c", "config"},
        QStringLiteral("Fichier de configuration JSON (reglages machine)."),
        QStringLiteral("chemin"));
    parser.addOption(configOpt);
    parser.process(app);

    ServiceConfig config;
    QString configPath = parser.value(configOpt);
    if (configPath.isEmpty())
        configPath = findDefaultConfig();

    if (configPath.isEmpty()) {
        err() << "Aucune configuration trouvee : demarrage avec les valeurs par "
                 "defaut. Fournir --config au besoin.\n";
    } else {
        QString error;
        if (!loadConfig(configPath, &config, &error)) {
            err() << "Erreur de configuration : " << error << '\n';
            return 2;
        }
        out() << "Configuration chargee : " << configPath << '\n';
    }

    morfcollector::Service service(config);

    if (!service.start()) {
        err() << "Le serveur HTTP n'a pas pu ecouter sur le port "
              << config.httpPort << " (deja utilise ?).\n";
        return 3;
    }

    out() << "morfCollector v" << morfcollector::version() << " demarre : API http://"
          << config.bindAddress << ':' << service.httpPort()
          << "/  (POST /manifest ; GET /sources /status /healthz)\n";
    out().flush();

    return app.exec();
}
