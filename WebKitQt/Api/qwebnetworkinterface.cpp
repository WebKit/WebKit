/*
  Copyright (C) 2006 Enrico Ros <enrico.ros@m31engineering.it>
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
#include <qglobal.h>
#include "qwebnetworkinterface.h"
#include "qwebnetworkinterface_p.h"
#include "qwebobjectpluginconnector.h"
#include <qdebug.h>
#include <qfile.h>
#include <qurl.h>

#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "MimeTypeRegistry.h"
#include "CookieJar.h"

#define notImplemented() qDebug("FIXME: UNIMPLEMENTED: %s:%d (%s)", __FILE__, __LINE__, __FUNCTION__)

#if 0
#define DEBUG qDebug
#else
#define DEBUG if (1) {} else qDebug
#endif

static QWebNetworkInterface *default_interface = 0;
static QWebNetworkManager *manager = 0;

using namespace WebCore;

uint qHash(const HostInfo &info)
{
    return qHash(info.host) + info.port;
}

static bool operator==(const HostInfo &i1, const HostInfo &i2)
{
    return i1.port == i2.port && i1.host == i2.host;
}

void QWebNetworkRequestPrivate::init(const WebCore::ResourceRequest &resourceRequest)
{
    KURL url = resourceRequest.url();
    QUrl qurl = QString(url.url());
    init(resourceRequest.httpMethod(), qurl, &resourceRequest);
}

void QWebNetworkRequestPrivate::init(const QString &method, const QUrl &url, const WebCore::ResourceRequest *resourceRequest)
{
    httpHeader = QHttpRequestHeader(method, url.toEncoded(QUrl::RemoveScheme|QUrl::RemoveAuthority));
    httpHeader.setValue(QLatin1String("User-Agent"),
                         QLatin1String("Mozilla/5.0 (PC; U; Intel; Linux; en) AppleWebKit/420+ (KHTML, like Gecko)"));
    httpHeader.setValue(QLatin1String("Connection"), QLatin1String("Keep-Alive"));
    setURL(url);

    if (resourceRequest) {
        const QString scheme = url.scheme().toLower();
        if (scheme == QLatin1String("http") || scheme == QLatin1String("https")) {
            QString cookies = WebCore::cookies(resourceRequest->url());
            if (!cookies.isEmpty())
                httpHeader.setValue(QLatin1String("Cookie"), cookies);
        }


        const HTTPHeaderMap& loaderHeaders = resourceRequest->httpHeaderFields();
        HTTPHeaderMap::const_iterator end = loaderHeaders.end();
        for (HTTPHeaderMap::const_iterator it = loaderHeaders.begin(); it != end; ++it)
            httpHeader.setValue(it->first, it->second);

        // handle and perform a 'POST' request
        if (method == "POST") {
            DeprecatedString pd = resourceRequest->httpBody()->flattenToString().deprecatedString();
            postData = QByteArray(pd.ascii(), pd.length());
            httpHeader.setValue(QLatin1String("content-length"), QString::number(postData.size()));
        }
    }
}

void QWebNetworkRequestPrivate::setURL(const QUrl &u)
{
    url = u;
    int port = url.port();
    if (port > 0 && port != 80)
        httpHeader.setValue(QLatin1String("Host"), url.host() + QLatin1Char(':') + QString::number(port));
    else
        httpHeader.setValue(QLatin1String("Host"), url.host());
}

QWebNetworkRequest::QWebNetworkRequest()
    : d(new QWebNetworkRequestPrivate)
{
}

QWebNetworkRequest::QWebNetworkRequest(const QWebNetworkRequest &other)
    : d(new QWebNetworkRequestPrivate(*other.d))
{
}

QWebNetworkRequest &QWebNetworkRequest::operator=(const QWebNetworkRequest &other)
{
    *d = *other.d;
    return *this;
}

QWebNetworkRequest::QWebNetworkRequest(QWebNetworkRequestPrivate *priv)
    : d(new QWebNetworkRequestPrivate(*priv))
{
}

QWebNetworkRequest::~QWebNetworkRequest()
{
    delete d;
}

QUrl QWebNetworkRequest::url() const
{
    return d->url;
}

void QWebNetworkRequest::setUrl(const QUrl &url)
{
    d->setURL(url);
}

QHttpRequestHeader QWebNetworkRequest::httpHeader() const
{
    return d->httpHeader;
}

void QWebNetworkRequest::setHttpHeader(const QHttpRequestHeader &header) const
{
    d->httpHeader = header;
}

QByteArray QWebNetworkRequest::postData() const
{
    return d->postData;
}

void QWebNetworkRequest::setPostData(const QByteArray &data)
{
    d->postData = data;
}

/*!
  \class QWebNetworkJob

  The QWebNetworkJob class represents a network job, that needs to be
  processed by the QWebNetworkInterface.

  This class is only required when implementing a new network layer (or
  support for a special protocol) using QWebNetworkInterface.

  QWebNetworkJob objects are created and owned by the QtWebKit library.
  Most of it's properties are read-only.

  The job is reference counted. This can be used to ensure that the job doesn't
  get deleted while it's still stored in some data structure.
*/

/*!
  \internal
*/
QWebNetworkJob::QWebNetworkJob()
    : d(new QWebNetworkJobPrivate)
{
    d->ref = 1;
    d->redirected = false;
    d->interface = 0;
}

/*!
  \internal
*/
QWebNetworkJob::~QWebNetworkJob()
{
    delete d;
}

/*!
  The requested URL
*/
QUrl QWebNetworkJob::url() const
{
    return d->request.url;
}

/*!
  Post data associated with the job
*/
QByteArray QWebNetworkJob::postData() const
{
    return d->request.postData;
}

/*!
  The HTTP request header that should be used to download the job.
*/
QHttpRequestHeader QWebNetworkJob::httpHeader() const
{
    return d->request.httpHeader;
}

/*!
  The complete network request that should be used to download the job.
*/
QWebNetworkRequest QWebNetworkJob::request() const
{
    return QWebNetworkRequest(&d->request);
}

/*!
  The HTTP response header received from the network.
*/
QHttpResponseHeader QWebNetworkJob::response() const
{
    return d->response;
}

/*!
  Sets the HTTP reponse header. The response header has to be called before
  emitting QWebNetworkInterface::started.
*/
void QWebNetworkJob::setResponse(const QHttpResponseHeader &response)
{
    d->response = response;
}

/*!
  returns true if the job has been cancelled by the WebKit framework
*/
bool QWebNetworkJob::cancelled() const
{
    return !d->resourceHandle && !d->connector;
}

/*!
  reference the job.
*/
void QWebNetworkJob::ref()
{
    ++d->ref;
}

/*!
  derefence the job.

  If the reference count drops to 0 this method also deletes the job.

  Returns false if the reference count has dropped to 0.
*/
bool QWebNetworkJob::deref()
{
    if (!--d->ref) {
        delete this;
        return false;
    }
    return true;
}

/*!
   Returns the network interface that is associated with this job.
*/
QWebNetworkInterface *QWebNetworkJob::networkInterface() const
{
    return d->interface;
}

/*!
  \class QWebNetworkManager
  \internal
*/
QWebNetworkManager::QWebNetworkManager()
    : QObject(0)
{
}

QWebNetworkManager *QWebNetworkManager::self()
{
    // ensure everything's constructed and connected
    QWebNetworkInterface::defaultInterface();

    return manager;
}

bool QWebNetworkManager::add(ResourceHandle *handle, QWebNetworkInterface *interface)
{
    ASSERT(resource);

    if (!interface)
        interface = default_interface;

    ASSERT(interface);

    QWebNetworkJob *job = new QWebNetworkJob();
    handle->getInternal()->m_job = job;
    job->d->resourceHandle = handle;
    job->d->interface = interface;
    job->d->connector = 0;

    job->d->request.init(handle->request());

    if (handle->method() != "POST" && handle->method() != "GET") {
        // don't know what to do! (probably a request error!!)
        // but treat it like a 'GET' request
        qWarning("REQUEST: [%s]\n", qPrintable(job->d->request.httpHeader.toString()));
    }

    DEBUG() << "QWebNetworkManager::add:" <<  job->d->request.httpHeader.toString();

    interface->addJob(job);

    return true;
}

void QWebNetworkManager::cancel(ResourceHandle *handle)
{
    QWebNetworkJob *job = handle->getInternal()->m_job;
    if (!job)
        return;
    job->d->resourceHandle = 0;
    job->d->connector = 0;
    job->d->interface->cancelJob(job);
    handle->getInternal()->m_job = 0;
}

void QWebNetworkManager::started(QWebNetworkJob *job)
{
    ResourceHandleClient* client = 0;
    if (job->d->resourceHandle) {
        client = job->d->resourceHandle->client();
        if (!client)
            return;
    } else if (!job->d->connector) {
        return;
    }

    DEBUG() << "ResourceHandleManager::receivedResponse:";
    DEBUG() << job->d->response.toString();

    QStringList cookies = job->d->response.allValues("Set-Cookie");
    KURL url(job->url().toString());
    foreach (QString c, cookies) {
        setCookies(url, url, c);
    }
    QString contentType = job->d->response.value("Content-Type");
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
    if (contentType.isEmpty()) {
        // let's try to guess from the extension
        QString extension = job->d->request.url.path();
        int index = extension.lastIndexOf(QLatin1Char('.'));
        if (index > 0) {
            extension = extension.mid(index + 1);
            contentType = MimeTypeRegistry::getMIMETypeForExtension(extension);
        }
    }
//     qDebug() << "Content-Type=" << contentType;
//     qDebug() << "Encoding=" << encoding;


    ResourceResponse response(url, contentType,
                              0 /* FIXME */,
                              encoding,
                              String() /* FIXME */);

    int statusCode = job->d->response.statusCode();
    response.setHTTPStatusCode(statusCode);
    /* Fill in the other fields */

    if (statusCode >= 300 && statusCode < 400) {
        // we're on a redirect page! if the 'Location:' field is valid, we redirect
        QString location = job->d->response.value("location");
        DEBUG() << "Redirection";
        if (!location.isEmpty()) {
            ResourceRequest newRequest = job->d->resourceHandle->request();
            newRequest.setURL(KURL(newRequest.url(), DeprecatedString(location)));
            if (client)
                client->willSendRequest(job->d->resourceHandle, newRequest, response);
            job->d->request.httpHeader.setRequest(job->d->request.httpHeader.method(), newRequest.url().path() + newRequest.url().query());
            job->d->request.setURL(QString(newRequest.url().url()));
            job->d->redirected = true;
            return;
        }
    }

    if (client)
        client->didReceiveResponse(job->d->resourceHandle, response);
    if (job->d->connector)
        emit job->d->connector->started(job);
    
}

void QWebNetworkManager::data(QWebNetworkJob *job, const QByteArray &data)
{
    ResourceHandleClient* client = 0;
    if (job->d->resourceHandle) {
        client = job->d->resourceHandle->client();
        if (!client)
            return;
    } else if (!job->d->connector) {
        return;
    }

    if (job->d->redirected)
        return; // don't emit the "Document has moved here" type of HTML

    DEBUG() << "receivedData" << job->d->request.url.path();
    if (client)
        client->didReceiveData(job->d->resourceHandle, data.constData(), data.length(), data.length() /*FixMe*/);
    if (job->d->connector)
        emit job->d->connector->data(job, data);
    
}

void QWebNetworkManager::finished(QWebNetworkJob *job, int errorCode)
{
    ResourceHandleClient* client = 0;
    if (job->d->resourceHandle) {
        client = job->d->resourceHandle->client();
        if (!client)
            return;
    } else if (!job->d->connector) {
        job->deref();
        return;
    }

    DEBUG() << "receivedFinished" << errorCode << job->url();

    if (job->d->redirected) {
        job->d->redirected = false;
        job->d->interface->addJob(job);
        return;
    }
    
    if (job->d->resourceHandle)
        job->d->resourceHandle->getInternal()->m_job = 0;

    if (client) {
        if (errorCode) {
            //FIXME: error setting error was removed from ResourceHandle
            client->didFail(job->d->resourceHandle,
                            ResourceError(job->d->request.url.host(), job->d->response.statusCode(),
                                          job->d->request.url.toString(), String()));
        } else {
            client->didFinishLoading(job->d->resourceHandle);
        }
    }

    if (job->d->connector)
        emit job->d->connector->finished(job, errorCode);
    
    DEBUG() << "receivedFinished done" << job->d->request.url;

    job->deref();
}

void QWebNetworkManager::addHttpJob(QWebNetworkJob *job)
{
    HostInfo hostInfo(job->url());
    WebCoreHttp *httpConnection = m_hostMapping.value(hostInfo);
    if (!httpConnection) {
        // #### fix custom ports
        DEBUG() << "   new connection to" << hostInfo.host << hostInfo.port;
        httpConnection = new WebCoreHttp(this, hostInfo);
        QObject::connect(httpConnection, SIGNAL(connectionClosed(const WebCore::HostInfo&)),
                         this, SLOT(httpConnectionClosed(const WebCore::HostInfo&)));

        m_hostMapping[hostInfo] = httpConnection;
    }
    httpConnection->request(job);
}

void QWebNetworkManager::cancelHttpJob(QWebNetworkJob *job)
{
    WebCoreHttp *httpConnection = m_hostMapping.value(job->url());
    if (httpConnection)
        httpConnection->cancel(job);
}

void QWebNetworkManager::httpConnectionClosed(const WebCore::HostInfo &info)
{
    WebCoreHttp *connection = m_hostMapping.take(info);
    delete connection;
}

void QWebNetworkInterfacePrivate::sendFileData(QWebNetworkJob* job, int statusCode, const QByteArray &data)
{
    int error = statusCode >= 400 ? 1 : 0;
    if (!job->cancelled()) {
        QHttpResponseHeader response;
        response.setStatusLine(statusCode);
        job->setResponse(response);
        emit q->started(job);
        if (!data.isEmpty())
            emit q->data(job, data);
    }
    emit q->finished(job, error);
}

void QWebNetworkInterfacePrivate::parseDataUrl(QWebNetworkJob* job)
{
    QByteArray data = job->url().toString().toLatin1();
    //qDebug() << "handling data url:" << data; 

    ASSERT(data.startsWith("data:"));

    // Here's the syntax of data URLs:
    // dataurl    := "data:" [ mediatype ] [ ";base64" ] "," data
    // mediatype  := [ type "/" subtype ] *( ";" parameter )
    // data       := *urlchar
    // parameter  := attribute "=" value
    QByteArray header;
    bool base64 = false;

    int index = data.indexOf(',');
    if (index != -1) {
        header = data.mid(5, index - 5);
        header = header.toLower();
        //qDebug() << "header=" << header;
        data = data.mid(index+1);
        //qDebug() << "data=" << data;

        if (header.endsWith(";base64")) {
            //qDebug() << "base64";
            base64 = true;
            header = header.left(header.length() - 7);
            //qDebug() << "mime=" << header;
        }        
    } else {
        data = QByteArray();
    }
    if (base64) {
        data = QByteArray::fromBase64(data);
    } else {
        data = QUrl::fromPercentEncoding(data).toLatin1();
    }

    if (header.isEmpty()) 
        header = "text/plain;charset=US-ASCII";
    int statusCode = data.isEmpty() ? 404 : 200;
    QHttpResponseHeader response;
    response.setContentType(header);
    response.setContentLength(data.size());
    job->setResponse(response);

    sendFileData(job, statusCode, data);
}

/*!
  \class QWebNetworkInterface

  The QWebNetworkInterface class provides an abstraction layer for
  WebKit's network interface.  It allows to completely replace or
  extend the builtin network layer.

  QWebNetworkInterface contains two virtual methods, addJob and
  cancelJob that have to be reimplemented when implementing your own
  networking layer.

  QWebNetworkInterface can by default handle the http, https, file and
  data URI protocols.
  
*/

/*!
  Sets a new default interface that will be used by all of WebKit
  for downloading data from the internet.
*/
void QWebNetworkInterface::setDefaultInterface(QWebNetworkInterface *defaultInterface)
{
    if (default_interface == defaultInterface)
        return;
    if (default_interface)
        delete default_interface;
    default_interface = defaultInterface;
}

/*!
  Returns the default interface that will be used by WebKit. If no
  default interface has been set, QtWebkit will create an instance of
  QWebNetworkInterface to do the work.
*/
QWebNetworkInterface *QWebNetworkInterface::defaultInterface()
{
    if (!default_interface)
        setDefaultInterface(new QWebNetworkInterface);
    return default_interface;
}


/*!
  Constructs a QWebNetworkInterface object.
*/
QWebNetworkInterface::QWebNetworkInterface(QObject *parent)
    : QObject(parent)
{
    d = new QWebNetworkInterfacePrivate;
    d->q = this;

    if (!manager)
        manager = new QWebNetworkManager;

    QObject::connect(this, SIGNAL(started(QWebNetworkJob*)),
                     manager, SLOT(started(QWebNetworkJob*)), Qt::QueuedConnection);
    QObject::connect(this, SIGNAL(data(QWebNetworkJob*, const QByteArray &)),
                     manager, SLOT(data(QWebNetworkJob*, const QByteArray &)), Qt::QueuedConnection);
    QObject::connect(this, SIGNAL(finished(QWebNetworkJob*, int)),
                     manager, SLOT(finished(QWebNetworkJob*, int)), Qt::QueuedConnection);
}

/*!
  Destructs the QWebNetworkInterface object.
*/
QWebNetworkInterface::~QWebNetworkInterface()
{
    delete d;
}

/*!
  This virtual method gets called whenever QtWebkit needs to add a
  new job to download.

  The QWebNetworkInterface should process this job, by first emitting
  the started signal, then emitting data repeatedly as new data for
  the Job is available, and finally ending the job with emitting a
  finished signal.

  After the finished signal has been emitted, the QWebNetworkInterface
  is not allowed to access the job anymore.
*/
void QWebNetworkInterface::addJob(QWebNetworkJob *job)
{
    QString protocol = job->url().scheme();
    if (protocol == QLatin1String("http")) {
        QWebNetworkManager::self()->addHttpJob(job);
        return;
    }

    // "file", "data" and all unhandled stuff go through here
    //DEBUG() << "fileRequest";
    DEBUG() << "FileLoader::request" << job->url();

    if (job->cancelled()) {
        d->sendFileData(job, 400, QByteArray());
        return;
    }

    QUrl url = job->url();
    if (protocol == QLatin1String("data")) {
        d->parseDataUrl(job);
        return;
    }

    int statusCode = 200;
    QByteArray data;
    if (!(protocol.isEmpty() || protocol == QLatin1String("file"))) {
        statusCode = 404;
    } else if (job->postData().isEmpty()) {
        QFile f(url.path());
        DEBUG() << "opening" << QString(url.path());

        if (f.open(QIODevice::ReadOnly)) {
            QHttpResponseHeader response;
            response.setStatusLine(200);
            job->setResponse(response);
            data = f.readAll();
        } else {
            statusCode = 404;
        }
    } else {
        statusCode = 404;
    }
    d->sendFileData(job, statusCode, data);
}

/*!
  This virtual method gets called whenever QtWebkit needs to cancel a
  new job.

  The QWebNetworkInterface acknowledge the canceling of the job, by
  emitting the finished signal with an error code of 1. After emitting
  the finished signal, the interface should not access the job
  anymore.
*/
void QWebNetworkInterface::cancelJob(QWebNetworkJob *job)
{
    QString protocol = job->url().scheme();
    if (protocol == QLatin1String("http"))
        QWebNetworkManager::self()->cancelHttpJob(job);
}

/////////////////////////////////////////////////////////////////////////////
WebCoreHttp::WebCoreHttp(QObject* parent, const HostInfo &hi)
    : QObject(parent), info(hi),
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

void WebCoreHttp::request(QWebNetworkJob *job)
{
    DEBUG() << ">>>>>>>>>>>>>> WebCoreHttp::request";
    DEBUG() << job->httpHeader().toString() << "\n";
    m_pendingRequests.append(job);

    scheduleNextRequest();
}

void WebCoreHttp::scheduleNextRequest()
{
    int c = 0;
    for (; c < 2; ++c) {
        if (!connection[c].current)
            break;
    }
    if (c >= 2)
        return;

    QWebNetworkJob *job = 0;
    while (!job && !m_pendingRequests.isEmpty()) {
        job = m_pendingRequests.takeFirst();
        if (job->cancelled()) {
            emit job->networkInterface()->finished(job, 1);
            job = 0;
        }
    }
    if (!job)
        return;
    
    QHttp *http = connection[c].http;
    QByteArray postData = job->postData();
    if (!postData.isEmpty())
        http->request(job->httpHeader(), postData);
    else
        http->request(job->httpHeader());
    connection[c].current = job;

    DEBUG() << "WebCoreHttp::scheduleNextRequest: using connection" << c;
//     DEBUG() << job->request.toString();
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
    //Q_ASSERT(connection[c].current);
    return c;
}

void WebCoreHttp::onResponseHeaderReceived(const QHttpResponseHeader &resp)
{
    int c = getConnection();
    QWebNetworkJob *job = connection[c].current;
    DEBUG() << "WebCoreHttp::slotResponseHeaderReceived connection=" << c;

    job->setResponse(resp);

    emit job->networkInterface()->started(job);
}

void WebCoreHttp::onReadyRead()
{
    int c = getConnection();
    QWebNetworkJob *req = connection[c].current;
    QHttp *http = connection[c].http;
    DEBUG() << "WebCoreHttp::slotReadyRead connection=" << c;

    QByteArray data;
    data.resize(http->bytesAvailable());
    http->read(data.data(), data.length());
    emit req->networkInterface()->data(req, data);
}

void WebCoreHttp::onRequestFinished(int, bool error)
{
    int c = getConnection();
    QWebNetworkJob *req = connection[c].current;
    if (!req) {
        scheduleNextRequest();
        return;
    }
    QHttp *http = connection[c].http;
    DEBUG() << "WebCoreHttp::slotFinished connection=" << c << error << req;

    if (error)
        DEBUG() << "   error: " << http->errorString();

    if (!error && http->bytesAvailable()) {
        QByteArray data;
        data.resize(http->bytesAvailable());
        http->read(data.data(), data.length());
        emit req->networkInterface()->data(req, data);
    }
    emit req->networkInterface()->finished(req, error ? 1 : 0);

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

void WebCoreHttp::cancel(QWebNetworkJob* request)
{
    bool doEmit = true;
    m_inCancel = true;
    for (int i = 0; i < 2; ++i) {
        if (request == connection[i].current) {
            connection[i].http->abort();
            doEmit = false;
        }
    }
    if (!m_pendingRequests.removeAll(request))
        doEmit = false;
    m_inCancel = false;

    if (doEmit)
        emit request->networkInterface()->finished(request, 1);

    if (m_pendingRequests.isEmpty()
        && !connection[0].current && !connection[1].current)
        emit connectionClosed(info);
}

HostInfo::HostInfo(const QUrl& url)
    : protocol(url.scheme())
    , host(url.host())
    , port(url.port())
{
    if (port < 0) {
        if (protocol == QLatin1String("http"))
            port = 80;
        else if (protocol == QLatin1String("https"))
            port = 443;
    }
}

