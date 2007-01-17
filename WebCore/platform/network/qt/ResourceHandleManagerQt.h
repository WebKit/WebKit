/*
 * Copyright (C) 2006 Enrico Ros <enrico.ros@m31engineering.it>
 * Copyright (C) 2006 Trolltech ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ResourceHandleManagerQt_H
#define ResourceHandleManagerQt_H

#include <QHash>
#include <QHttp>
#include <QBuffer>
#include <QObject>
#include <QThread>
#include <QString>
#include <QEvent>
#include <QUrl>

namespace WebCore {

class LoaderThread;
class ResourceHandle;
class NetworkLoader;

struct HostInfo {
    HostInfo() {}
    HostInfo(const KURL& url);
    QString protocol;
    QString host;
    int port;
    bool isLocalFile() { return protocol.isEmpty() || protocol == QLatin1String("file"); }
};
    
class RequestQt
{
public:
    RequestQt(ResourceHandle*, FrameQtClient*);
    void setURL(const KURL &url);
    // not thread safe, don't use in other threads
    KURL url;

    QUrl qurl;
    FrameQtClient* client;
    ResourceHandle* resource;

    // to be used by other threads
    HostInfo hostInfo;
    QByteArray postData;
    QHttpRequestHeader request;
    QHttpResponseHeader response;
    bool redirected;
    bool cancelled;
};

/**
 * @class ResourceHandleManager
 * @short Download Controller: handle ResourceHandles using a pool of threads
 */
class ResourceHandleManager : public QObject {
    Q_OBJECT
public:
    static ResourceHandleManager* self();

    void add(ResourceHandle*, FrameQtClient*);
    void add(RequestQt* request);
    void cancel(ResourceHandle*);

public slots:
    void receivedResponse(RequestQt*);
    void receivedData(RequestQt*, const QByteArray& data);
    void receivedFinished(RequestQt*, int errorCode);
signals:
    void networkRequest(RequestQt*);
    void networkCancel(RequestQt*);
    void fileRequest(RequestQt*);

private:
    ResourceHandleManager();
    ~ResourceHandleManager();

    LoaderThread *m_networkLoader;
    LoaderThread *m_fileLoader;

    QHash<ResourceHandle *, RequestQt *> pendingRequests;
};


class LoaderThread : public QThread {
    Q_OBJECT
public:
    enum Type {
        Network,
        File
    };
    LoaderThread(ResourceHandleManager *manager, Type type);

    void waitForSetup() { while (!m_setup); }
protected:
    void run();
private:
    Type m_type;
    QObject* m_loader;
    ResourceHandleManager* m_manager;
    volatile bool m_setup;
};

class FileLoader : public QObject {
    Q_OBJECT
public:
    FileLoader();

public slots:
    void request(RequestQt*);

signals:
    void receivedResponse(RequestQt* resource);
    void receivedData(RequestQt* resource, const QByteArray &data);
    void receivedFinished(RequestQt* resource, int errorCode);

private:
    void parseDataUrl(RequestQt* request);
    void sendData(RequestQt* request, int statusCode, const QByteArray &data);
};


class WebCoreHttp : public QObject
{
    Q_OBJECT
public:
    WebCoreHttp(NetworkLoader* parent, const HostInfo&);
    ~WebCoreHttp();

    void request(RequestQt* resource);
    void cancel(RequestQt*);

signals:
    void connectionClosed(const HostInfo &);

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
    NetworkLoader* m_loader;
    QList<RequestQt*> m_pendingRequests;
    struct HttpConnection {
        QHttp *http;
        RequestQt *current;
    };
    HttpConnection connection[2];
    bool m_inCancel;
};

class NetworkLoader : public QObject {
    Q_OBJECT
public:
    NetworkLoader();
    ~NetworkLoader();


public slots:
    void request(RequestQt*);
    void cancel(RequestQt*);
    void connectionClosed(const HostInfo &);

signals:
    void receivedResponse(RequestQt* resource);
    void receivedData(RequestQt* resource, const QByteArray &data);
    void receivedFinished(RequestQt* resource, int errorCode);
private:
    friend class WebCoreHttp;
    QHash<HostInfo, WebCoreHttp *> m_hostMapping;
};


}

#endif
