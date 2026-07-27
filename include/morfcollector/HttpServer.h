/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QObject>
#include <QElapsedTimer>
#include <QByteArray>
#include "morfcollector/ServiceConfig.h"

class QTcpServer;
class QTcpSocket;

namespace morfcollector {

class Collector;

// -----------------------------------------------------------------------------
// HttpServer : serveur HTTP/1.1 minimal servant l'API du contrat morfcollect/1
// (CONTRAT.md §6). GET + POST + DELETE, corps lu via Content-Length.
//
// Cadre (compatible morfBeacon) :
//   GET    /status                     detail : etat, metriques, collect, capabilities, api
//   GET    /healthz                    sonde de vie
// Configuration / lecture :
//   POST   /manifest                   recevoir un manifeste       (200/409/422)
//   GET    /manifest/state?instance=   generation + revision       (200/404)
//   POST   /credentials                remise unique de secrets    (204/400)
//   GET    /sources                    liste
//   GET    /sources/{id}               detail (triplet suspension)
//   GET    /sources/{id}/periods       periodes disponibles
//   GET    /sources/{id}/objects       objets collectes
//   GET    /objects/{object_id}        recuperer un original       (404 : a venir)
// Administration :
//   POST   /sources/{id}/collect       collecte immediate          (202)
//   POST   /sources/{id}/suspend       override operateur ON
//   POST   /sources/{id}/resume        override operateur OFF
//   DELETE /objects/{object_id}                                    (a venir)
//   DELETE /sources/{id}/objects                                   (a venir)
//   DELETE /sources/{id}                                           (a venir)
// -----------------------------------------------------------------------------
class HttpServer : public QObject {
    Q_OBJECT
public:
    HttpServer(ServiceConfig config, Collector* collector, QObject* parent = nullptr);
    ~HttpServer() override;

    bool start();
    void stop();
    bool isListening() const;
    quint16 port() const;

private:
    void onNewConnection();
    void onSocketReadyRead(QTcpSocket* sock);
    void handleRequest(QTcpSocket* sock, const QByteArray& method,
                       const QByteArray& rawPath, const QByteArray& body);

    // Aiguillage des routes. Renseigne code/reason et renvoie le corps JSON.
    QByteArray route(const QByteArray& method, const QString& path,
                     const QString& query, const QByteArray& body,
                     int& code, QByteArray& reason);

    QByteArray buildStatusJson() const;
    void reply(QTcpSocket* sock, int code, const QByteArray& reason, const QByteArray& body);

    ServiceConfig m_config;
    Collector*    m_collector;
    QTcpServer*   m_server;
    QElapsedTimer m_uptime;
};

} // namespace morfcollector
