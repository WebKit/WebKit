/*
  Copyright (C) 2007 Trolltech ASA
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.
  
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.
  
  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
  
  This class provides all functionality needed for loading images, style sheets and html
  pages from the web. It has a memory cache for these objects.
*/
#ifndef QWEBNETWORKINTERFACE_P_H
#define QWEBNETWORKINTERFACE_P_H

#include "qwebnetworkinterface.h"
#include <qthread.h>

namespace WebCore {
    struct HostInfo;
    class ResourceRequest;
};
uint qHash(const WebCore::HostInfo &info);
#include <qhash.h>

namespace WebCore {
    class ResourceHandle;
}

struct QWebNetworkRequest
{
    QUrl url;
    QHttpRequestHeader httpHeader;
    QByteArray postData;

    void init(const WebCore::ResourceRequest &resourceRequest);
    void init(const QString &method, const QUrl &url, const WebCore::ResourceRequest *resourceRequest = 0);
    void setURL(const QUrl &u);
};

class QWebNetworkJobPrivate : public QWebNetworkRequest
{
public:
    QWebNetworkJobPrivate()
        : ref(1)
        , resourceHandle(0)
        , redirected(false)
        , interface(0)
        , connector(0)
        {}
    int ref;

    QHttpResponseHeader response;

    WebCore::ResourceHandle *resourceHandle;
    bool redirected;

    QWebNetworkInterface *interface;
    QWebObjectPluginConnector *connector;
};


class QWebNetworkManager : public QObject
{
    Q_OBJECT
public:
    static QWebNetworkManager *self();

    bool add(WebCore::ResourceHandle *resourceHandle, QWebNetworkInterface *interface);
    void cancel(WebCore::ResourceHandle *resourceHandle);

    void addHttpJob(QWebNetworkJob *job);
    void cancelHttpJob(QWebNetworkJob *job);

public slots:
    void started(QWebNetworkJob *);
    void data(QWebNetworkJob *, const QByteArray &data);
    void finished(QWebNetworkJob *, int errorCode);

signals:
    void fileRequest(QWebNetworkJob*);

private slots:
    void httpConnectionClosed(const WebCore::HostInfo &);

private:
    friend class QWebNetworkInterface;
    QWebNetworkManager();
    QHash<WebCore::HostInfo, WebCore::WebCoreHttp *> m_hostMapping;
};


namespace WebCore {

    class NetworkLoader;

    struct HostInfo {
        HostInfo() {}
        HostInfo(const QUrl& url);
        QString protocol;
        QString host;
        int port;
    };

    class WebCoreHttp : public QObject
    {
        Q_OBJECT
    public:
        WebCoreHttp(QObject *parent, const HostInfo&);
        ~WebCoreHttp();

        void request(QWebNetworkJob* resource);
        void cancel(QWebNetworkJob*);

    signals:
        void connectionClosed(const WebCore::HostInfo &);

    private slots:
        void onResponseHeaderReceived(const QHttpResponseHeader& resp);
        void onReadyRead();
        void onRequestFinished(int, bool);
        void onStateChanged(int);

        void scheduleNextRequest();

        int getConnection();

    public:
        HostInfo info;
    private:
        QList<QWebNetworkJob *> m_pendingRequests;
        struct HttpConnection {
            QHttp *http;
            QWebNetworkJob *current;
        };
        HttpConnection connection[2];
        bool m_inCancel;
    };

}

class QWebNetworkInterfacePrivate
{
public:
    void sendFileData(QWebNetworkJob* job, int statusCode, const QByteArray &data);
    void parseDataUrl(QWebNetworkJob* job);

    QWebNetworkInterface *q;
};

#endif
