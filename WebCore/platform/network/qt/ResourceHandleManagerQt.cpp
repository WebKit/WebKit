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

#include "config.h"

#include "CString.h"
#include "FrameQt.h"
#include "ResourceHandle.h"
#include "ResourceResponse.h"
#include "ResourceHandleManagerQt.h"
#include "ResourceHandleInternal.h"
#include "ResourceError.h"

#include <QCoreApplication>
#include <QHttpRequestHeader>
#include <QFile>
#include <QMap>
#include <qdebug.h>

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d (%s)\n", \
           __FILE__, __LINE__, __FUNCTION__); } while(0)

#if 0
#define DEBUG qDebug
#else
#define DEBUG if (1) {} else qDebug
#endif

namespace WebCore {

static ResourceHandleManager* s_self = 0;

ResourceHandleManager::ResourceHandleManager()
{
    m_fileLoader = new LoaderThread(this, LoaderThread::File);
    m_fileLoader->start();
    m_networkLoader = new LoaderThread(this, LoaderThread::Network);
    m_networkLoader->start();

    m_fileLoader->waitForSetup();
    m_networkLoader->waitForSetup();
}

ResourceHandleManager::~ResourceHandleManager()
{
    m_networkLoader->quit();
    m_fileLoader->quit();
}

ResourceHandleManager* ResourceHandleManager::self()
{
    if (!s_self)
        s_self = new ResourceHandleManager();

    return s_self;
}

RequestQt::RequestQt(ResourceHandle* res, FrameQtClient *c)
    : client(c), resource(res), url(res->url()), redirected(false), cancelled(false)
{
    request = QHttpRequestHeader(resource->method(), url.path() + url.query());
    request.setValue(QLatin1String("User-Agent"),
                           QLatin1String("Mozilla/5.0 (PC; U; Intel; Linux; en) AppleWebKit/420+ (KHTML, like Gecko)"));
    request.setValue(QLatin1String("Connection"), QLatin1String("Keep-Alive"));

    const HTTPHeaderMap& loaderHeaders = resource->requestHeaders();
    HTTPHeaderMap::const_iterator end = loaderHeaders.end();
    for (HTTPHeaderMap::const_iterator it = loaderHeaders.begin(); it != end; ++it)
        request.setValue(it->first, it->second);

    int port = url.port();
    if (port && port != 80)
        request.setValue(QLatin1String("Host"), url.host() + QLatin1Char(':') + QString::number(port));
    else
        request.setValue(QLatin1String("Host"), url.host());

    int id;
    // handle and perform a 'POST' request
    if (resource->method() == "POST") {
        request.setValue(QLatin1String("PropagateHttpHeader"), QLatin1String("true"));
        request.setValue(QLatin1String("content-type"), QLatin1String("Content-Type: application/x-www-form-urlencoded"));

        DeprecatedString pd = resource->postData()->flattenToString().deprecatedString();
        postData = QByteArray(pd.ascii(), pd.length());
        request.setValue(QLatin1String("content-length"), QString::number(postData.size()));
    } else if (resource->method() != "GET") {
        // or.. don't know what to do! (probably a request error!!)
        // but treat it like a 'GET' request
        notImplemented();
        qWarning("REQUEST: [%s]\n", qPrintable(request.toString()));
    }
//     DEBUG() << "RequestQt::RequestQt: http header:";
//     DEBUG() << request.toString();
}

void ResourceHandleManager::add(ResourceHandle* resource, FrameQtClient* client)
{
    ASSERT(resource);

    // check for (probably) broken requests
    if (resource->method() != "GET" && resource->method() != "POST") {
        notImplemented();
        return;
    }

    RequestQt* request = new RequestQt(resource, client);
    add(request);
}

void ResourceHandleManager::add(RequestQt* request)
{
    Q_ASSERT(!pendingRequests.value(request->resource));

    pendingRequests[request->resource] = request;

//     DEBUG() << "ResourceHandleManager::add";
    // check for not implemented protocols
    if (request->url.isLocalFile()) {
//         DEBUG() << "fileRequest";
        emit fileRequest(request);
        return;
    }
    String protocol = request->url.protocol();
    if (protocol == "http") {
//         DEBUG() << "networkRequest";
        emit networkRequest(request);
        return;
    }

    notImplemented();
    cancel(request->resource);
    return;
}

void ResourceHandleManager::cancel(ResourceHandle* resource)
{
    ResourceHandleClient* client = resource->client();
    if (!client)
        return;
    RequestQt *req = pendingRequests.value(resource);
    if (!req)
        return;

    DEBUG() << "ResourceHandleManager::cancel";
    client->didFail(resource, ResourceError());

    RequestQt* request = pendingRequests.take(resource);
    if (!request)
        return;
    request->cancelled = true;

    KURL url = request->url;
    if (url.isLocalFile()) {
        emit fileCancel(request);
    } else {
        String protocol = request->url.protocol();
        if (protocol == "http") {
            emit networkCancel(request);
        } else {
            delete request;
        }
    }

    return;
}


void ResourceHandleManager::receivedResponse(RequestQt* request)
{
    RequestQt *req = pendingRequests.value(request->resource);
    if (!req || request->cancelled)
        return;
    Q_ASSERT(req == request);
    DEBUG() << "ResourceHandleManager::receivedResponse:";
    DEBUG() << request->response.toString();

    ResourceHandleClient* client = request->resource->client();
    if (!client)
        return;

    QString contentType = request->response.value("Content-Type");
    QString encoding;
    int idx = contentType.indexOf(QLatin1Char(';'));
    if (idx > 0) {
        QString remainder = contentType.mid(idx + 1).toLower();
        contentType = contentType.left(idx).trimmed();

        idx = remainder.indexOf("charset");
        if (idx >= 0) {
            idx = remainder.indexOf(QLatin1Char('='), idx);
            if (idx >= 0)
                encoding = remainder.mid(idx + 1).trimmed();
        }
    }
//     qDebug() << "Content-Type=" << contentType;
//     qDebug() << "Encoding=" << encoding;


    ResourceResponse response(request->url, contentType,
                              0 /* FIXME */,
                              encoding,
                              String() /* FIXME */);

    int statusCode = request->response.statusCode();
    response.setHTTPStatusCode(statusCode);
    /* Fill in the other fields */

    if (statusCode >= 300 && statusCode < 400) {
        // we're on a redirect page! if the 'Location:' field is valid, we redirect
        QString location = request->response.value("location");
        DEBUG() << "Redirection";
        if (!location.isEmpty()) {
            ResourceRequest newRequest = request->resource->request();
            newRequest.setURL(DeprecatedString(location));
            client->willSendRequest(request->resource, newRequest, response);
            request->request.setRequest(request->request.method(), newRequest.url().path());
            request->url = newRequest.url();
            request->redirected = true;
            return;
        }
    }


    client->didReceiveResponse(request->resource, response);
}

void ResourceHandleManager::receivedData(RequestQt* request, const QByteArray& data)
{
    RequestQt *req = pendingRequests.value(request->resource);
    if (!req || request->cancelled || request->redirected)
        return;
    Q_ASSERT(req == request);

    ResourceHandleClient* client = request->resource->client();
    if (!client)
        return;

    DEBUG() << "receivedData" << data;
    client->didReceiveData(request->resource, data.constData(), data.length(), data.length() /*FixMe*/);
}

void ResourceHandleManager::receivedFinished(RequestQt* request, int errorCode)
{
    DEBUG() << "receivedFinished" << errorCode;
    RequestQt *req = pendingRequests.value(request->resource);
    if (!req)
        return;
    Q_ASSERT(req == request);

    pendingRequests.remove(request->resource);

    if (request->cancelled) {
        delete request;
        return;
    }

    if (request->redirected) {
        request->redirected = false;
        add(request);
        return;
    }

    ResourceHandleClient* client = request->resource->client();
    if (!client)
        return;

    if (errorCode) {
        //FIXME: error setting error was removed from ResourceHandle
        client->didFail(request->resource, ResourceError());
    } else {
        client->didFinishLoading(request->resource);
    }
    delete request;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
LoaderThread::LoaderThread(ResourceHandleManager *manager, Type type)
    : QThread(manager), m_type(type), m_loader(0), m_manager(manager), m_setup(false)
{
}

void LoaderThread::run()
{
    switch (m_type) {
    case Network:
        m_loader = new NetworkLoader;
        connect(m_manager, SIGNAL(networkRequest(RequestQt*)),
                m_loader, SLOT(request(RequestQt*)));
        connect(m_manager, SIGNAL(networkCancel(RequestQt*)),
                m_loader, SLOT(cancel(RequestQt*)));
        break;
    case File:
        m_loader = new FileLoader;
        connect(m_manager, SIGNAL(fileRequest(RequestQt*)),
                m_loader, SLOT(request(RequestQt*)));
        connect(m_manager, SIGNAL(fileCancel(RequestQt*)),
                m_loader, SLOT(cancel(RequestQt*)));
        break;
    }
    connect(m_loader, SIGNAL(receivedResponse(RequestQt*)),
            m_manager, SLOT(receivedResponse(RequestQt*)));
    connect(m_loader, SIGNAL(receivedData(RequestQt*, QByteArray)),
            m_manager, SLOT(receivedData(RequestQt*, QByteArray)));
    connect(m_loader, SIGNAL(receivedFinished(RequestQt*, int)),
            m_manager, SLOT(receivedFinished(RequestQt*, int)));
    DEBUG() << "calling exec";
    m_setup = true;
    exec();
    DEBUG() << "done exec";
    delete m_loader;
}

/////////////////////////////////////////////////////////////////////////////
FileLoader::FileLoader()
    : QObject(0)
{
    DEBUG() << "FileLoader::FileLoader";
}

void FileLoader::request(RequestQt* request)
{
    DEBUG() << "FileLoader::request";
    KURL url = request->url;
    Q_ASSERT(url.isLocalFile());
    int error = 0;

    if (request->postData.isEmpty()) {
        QFile f(QString(url.path()));
        DEBUG() << "opening" << QString(url.path());

        if (f.open(QIODevice::ReadOnly)) {
            emit receivedResponse(request);
        
            QByteArray data = f.readAll();
            emit receivedData(request, data);
        } else {
            error = 2;
        }
    } else {
        error = 1;
    }
    DEBUG() << "error" << error;
    emit receivedFinished(request, error);
}

void FileLoader::cancel(RequestQt* resource)
{
    emit receivedFinished(resource, 1);
}

/////////////////////////////////////////////////////////////////////////////
WebCoreHttp::WebCoreHttp(NetworkLoader* parent, const HostInfo &hi)
    : info(hi),
      m_loader(parent),
      m_inCancel(false)
{
    for (int i = 0; i < 2; ++i) {
        connection[i].http = new QHttp(info.host, info.port);
        connection[i].current = 0;
        connect(connection[i].http, SIGNAL(responseHeaderReceived(const QHttpResponseHeader&)),
                this, SLOT(onResponseHeaderReceived(const QHttpResponseHeader&)));
        connect(connection[i].http, SIGNAL(readyRead(const QHttpResponseHeader&)),
                this, SLOT(onReadyRead()));
        connect(connection[i].http, SIGNAL(requestFinished(int, bool)),
                this, SLOT(onRequestFinished(int, bool)));
        connect(connection[i].http, SIGNAL(stateChanged(int)),
                this, SLOT(onStateChanged(int)));
    }
}

WebCoreHttp::~WebCoreHttp()
{
}

void WebCoreHttp::request(RequestQt *req)
{
    DEBUG() << ">>>>>>>>>>>>>> WebCoreHttp::request";
    DEBUG() << req->request.toString() << "\n";
    m_pendingRequests.append(req);

    scheduleNextRequest();
}

void WebCoreHttp::scheduleNextRequest()
{
    int c = 0;
    for (; c < 2; ++c) {
        if (!connection[c].current)
            break;
    }
    if (c >= 2 || m_pendingRequests.isEmpty())
        return;

    RequestQt *req = m_pendingRequests.takeFirst();
    QHttp *http = connection[c].http;
    if (!req->postData.isEmpty())
        http->request(req->request, req->postData);
    else
        http->request(req->request);
    connection[c].current = req;

    DEBUG() << "WebCoreHttp::scheduleNextRequest: using connection" << c;
//     DEBUG() << req->request.toString();
}

int WebCoreHttp::getConnection()
{
    QObject *o = sender();
    int c;
    if (o == connection[0].http) {
        c = 0;
    } else {
        Q_ASSERT(o == connection[1].http);
        c = 1;
    }
    Q_ASSERT(connection[c].current);
    return c;
}

void WebCoreHttp::onResponseHeaderReceived(const QHttpResponseHeader &resp)
{
    int c = getConnection();
    RequestQt *req = connection[c].current;
    DEBUG() << "WebCoreHttp::slotResponseHeaderReceived connection=" << c;

    req->response = resp;

    emit m_loader->receivedResponse(req);
}

void WebCoreHttp::onReadyRead()
{
    int c = getConnection();
    RequestQt *req = connection[c].current;
    QHttp *http = connection[c].http;
    DEBUG() << "WebCoreHttp::slotReadyRead connection=" << c;

    QByteArray data;
    data.resize(http->bytesAvailable());
    http->read(data.data(), data.length());
    emit m_loader->receivedData(req, data);
}

void WebCoreHttp::onRequestFinished(int, bool error)
{
    int c = getConnection();
    RequestQt *req = connection[c].current;
    QHttp *http = connection[c].http;
    DEBUG() << "WebCoreHttp::slotFinished connection=" << c << error << req;

    if (error)
        DEBUG() << "   error: " << http->errorString();

    if (!error && http->bytesAvailable()) {
        QByteArray data;
        data.resize(http->bytesAvailable());
        http->read(data.data(), data.length());
        emit m_loader->receivedData(req, data);
    }
    emit m_loader->receivedFinished(req, error ? 1 : 0);

    connection[c].current = 0;
    scheduleNextRequest();
}

void WebCoreHttp::onStateChanged(int state)
{
    if (state == QHttp::Closing || state == QHttp::Unconnected) {
        if (!m_inCancel && m_pendingRequests.isEmpty()
            && !connection[0].current && !connection[1].current)
            emit connectionClosed(info);
    }
}

void WebCoreHttp::cancel(RequestQt* request)
{
    m_inCancel = true;
    for (int i = 0; i < 2; ++i)
        if (request == connection[i].current)
            connection[i].http->abort();
    m_pendingRequests.removeAll(request);
    m_inCancel = false;

    emit m_loader->receivedFinished(request, 1);

    if (m_pendingRequests.isEmpty()
        && !connection[0].current && !connection[1].current)
        emit connectionClosed(info);
}


static uint qHash(const HostInfo &info)
{
    return qHash(info.host) + info.port;
}

static bool operator==(const HostInfo &i1, const HostInfo &i2)
{
    return i1.port == i2.port && i1.host == i2.host;
}

HostInfo::HostInfo(const KURL& url)
    : host(url.host()), port(url.port())
{
    if (!port)
        port = 80;
}

NetworkLoader::NetworkLoader()
    : QObject(0)
{
}

NetworkLoader::~NetworkLoader()
{
}

void NetworkLoader::request(RequestQt* request)
{
    DEBUG() << "NetworkLoader::request";
    HostInfo info(request->url);
    WebCoreHttp *httpConnection = m_hostMapping.value(info);
    if (!httpConnection) {
        // #### fix custom ports
        DEBUG() << "   new connection to" << info.host << info.port;
        httpConnection = new WebCoreHttp(this, info);
        connect(httpConnection, SIGNAL(connectionClosed(const HostInfo&)),
                this, SLOT(connectionClosed(const HostInfo&)));

        m_hostMapping[info] = httpConnection;
    }
    httpConnection->request(request);
}
void NetworkLoader::connectionClosed(const HostInfo& info)
{
    DEBUG() << "Disconnected";
    WebCoreHttp *connection = m_hostMapping.take(info);
    delete connection;
}

void NetworkLoader::cancel(RequestQt* request)
{
    DEBUG() << "NetworkLoader::cancel";
    HostInfo info(request->url);
    WebCoreHttp *httpConnection = m_hostMapping.value(info);
    if (httpConnection)
        httpConnection->cancel(request);
}

} // namespace WebCore

#include "ResourceHandleManagerQt.moc"
