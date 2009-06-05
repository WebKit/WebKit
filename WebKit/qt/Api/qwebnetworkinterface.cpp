/*
    Copyright (C) 2006 Enrico Ros <enrico.ros@m31engineering.it>
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2007 Staikos Computing Services Inc.  <info@staikos.net>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include <qglobal.h>
#if QT_VERSION < 0x040400
#include "qwebframe.h"
#include "qwebnetworkinterface.h"
#include "qwebnetworkinterface_p.h"
#include "qwebpage.h"
#include "qcookiejar.h"
#include <qdebug.h>
#include <qfile.h>
#include <qnetworkproxy.h>
#include <qurl.h>
#include <QAuthenticator>
#include <QCoreApplication>
#include <QSslError>

#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "MIMETypeRegistry.h"
#include "CookieJar.h"

#if 0
#define DEBUG qDebug
#else
#define DEBUG if (1) {} else qDebug
#endif

static QWebNetworkInterface *s_default_interface = 0;
static QWebNetworkManager *s_manager = 0;

using namespace WebCore;

uint qHash(const HostInfo &info)
{
    return qHash(info.host) + info.port;
}

static bool operator==(const HostInfo &i1, const HostInfo &i2)
{
    return i1.port == i2.port && i1.host == i2.host;
}

enum ParserState {
    State_Begin,
    State_FirstChar,
    State_SecondChar
};

/*
 * Decode URLs without doing any charset conversion.
 *
 * Most simple approach to do it without any lookahead.
 */
static QByteArray decodePercentEncoding(const QByteArray& input)
{
    int actualLength = 0;
    QByteArray tmpVal;
    QByteArray output;
    ParserState state = State_Begin;

    output.resize(input.length());
    tmpVal.resize(2);

    for (int i = 0; i < input.length(); ++i)
        if (state == State_Begin) {
            if (input.at(i) == '%') {
                state = State_FirstChar;
            } else {
                output[actualLength++] = input[i];
            }
        } else if (state == State_FirstChar) {
            state = State_SecondChar;
            tmpVal[0] = input[i];
        } else if (state == State_SecondChar) {
            state = State_Begin;
            tmpVal[1] = input[i];
            output[actualLength++] = tmpVal.toShort(0, 16);
        }

    output.resize(actualLength);
    return output;
}

void QWebNetworkRequestPrivate::init(const WebCore::ResourceRequest &resourceRequest)
{
    KURL url = resourceRequest.url();
    QUrl qurl = QString(url.string());
    init(resourceRequest.httpMethod(), qurl, &resourceRequest);
}

void QWebNetworkRequestPrivate::init(const QString &method, const QUrl &url, const WebCore::ResourceRequest *resourceRequest)
{
    httpHeader = QHttpRequestHeader(method, url.toString(QUrl::RemoveScheme|QUrl::RemoveAuthority));
    httpHeader.setValue(QLatin1String("Connection"), QLatin1String("Keep-Alive"));
    setURL(url);

    if (resourceRequest) {
        httpHeader.setValue(QLatin1String("User-Agent"), resourceRequest->httpUserAgent());
        const QString scheme = url.scheme().toLower();
        if (scheme == QLatin1String("http") || scheme == QLatin1String("https")) {
            QString cookies = QCookieJar::cookieJar()->cookies(resourceRequest->url());
            if (!cookies.isEmpty())
                httpHeader.setValue(QLatin1String("Cookie"), cookies);
        }


        const HTTPHeaderMap& loaderHeaders = resourceRequest->httpHeaderFields();
        HTTPHeaderMap::const_iterator end = loaderHeaders.end();
        for (HTTPHeaderMap::const_iterator it = loaderHeaders.begin(); it != end; ++it)
            httpHeader.setValue(it->first, it->second);

        // handle and perform a 'POST' request
        if (method == "POST") {
            Vector<char> data;
            resourceRequest->httpBody()->flatten(data);
            postData = QByteArray(data.data(), data.size());
            httpHeader.setValue(QLatin1String("content-length"), QString::number(postData.size()));
        }
    }
}

void QWebNetworkRequestPrivate::setURL(const QUrl &u)
{
    url = u;
    int port = url.port();
    const QString scheme = u.scheme();
    if (port > 0 && (port != 80 || scheme != "http") && (port != 443 || scheme != "https"))
        httpHeader.setValue(QLatin1String("Host"), url.host() + QLatin1Char(':') + QString::number(port));
    else
        httpHeader.setValue(QLatin1String("Host"), url.host());
}

/*!
  \class QWebNetworkRequest
  \internal

  The QWebNetworkRequest class represents a request for data from the network with all the
  necessary information needed for retrieval. This includes the url, extra HTTP header fields
  as well as data for a HTTP POST request.
*/

QWebNetworkRequest::QWebNetworkRequest()
    : d(new QWebNetworkRequestPrivate)
{
}

QWebNetworkRequest::QWebNetworkRequest(const QUrl &url, Method method, const QByteArray &postData)
    : d(new QWebNetworkRequestPrivate)
{
    d->init(method == Get ? "GET" : "POST", url);
    d->postData = postData;
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

/*!
  \internal
*/
QWebNetworkRequest::QWebNetworkRequest(const QWebNetworkRequestPrivate &priv)
    : d(new QWebNetworkRequestPrivate(priv))
{
}

/*!
  \internal
*/
QWebNetworkRequest::QWebNetworkRequest(const WebCore::ResourceRequest &request)
    : d(new QWebNetworkRequestPrivate)
{
    d->init(request);
}

QWebNetworkRequest::~QWebNetworkRequest()
{
    delete d;
}

/*!
  \internal
  The requested URL
*/
QUrl QWebNetworkRequest::url() const
{
    return d->url;
}

/*!
   \internal
   Sets the URL to request.

   Note that setting the URL also sets the "Host" field in the HTTP header.
*/
void QWebNetworkRequest::setUrl(const QUrl &url)
{
    d->setURL(url);
}

/*!
   \internal
   The http request header information.
*/
QHttpRequestHeader QWebNetworkRequest::httpHeader() const
{
    return d->httpHeader;
}

void QWebNetworkRequest::setHttpHeader(const QHttpRequestHeader &header) const
{
    d->httpHeader = header;
}

QString QWebNetworkRequest::httpHeaderField(const QString &key) const
{
    return d->httpHeader.value(key);
}

void QWebNetworkRequest::setHttpHeaderField(const QString &key, const QString &value)
{
    d->httpHeader.setValue(key, value);
}

/*!
    \internal
    Post data sent with HTTP POST requests.
*/
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
  \internal

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
}

/*!
  \internal
*/
QWebNetworkJob::~QWebNetworkJob()
{
    delete d;
    d = 0;
}

/*!
  \internal
  The requested URL
*/
QUrl QWebNetworkJob::url() const
{
    return d->request.url;
}

/*!
  \internal
  Post data associated with the job
*/
QByteArray QWebNetworkJob::postData() const
{
    return d->request.postData;
}

/*!
  \internal
  The HTTP request header that should be used to download the job.
*/
QHttpRequestHeader QWebNetworkJob::httpHeader() const
{
    return d->request.httpHeader;
}

/*!
  \internal
  The complete network request that should be used to download the job.
*/
QWebNetworkRequest QWebNetworkJob::request() const
{
    return QWebNetworkRequest(d->request);
}

/*!
  \internal
  The HTTP response header received from the network.
*/
QHttpResponseHeader QWebNetworkJob::response() const
{
    return d->response;
}

/*!
  \internal
  The last error of the Job.
*/
QString QWebNetworkJob::errorString() const
{
    return d->errorString;
}

/*!
  \internal
  Sets the HTTP reponse header. The response header has to be called before
  emitting QWebNetworkInterface::started.
*/
void QWebNetworkJob::setResponse(const QHttpResponseHeader &response)
{
    d->response = response;
}

void QWebNetworkJob::setErrorString(const QString& errorString)
{
    d->errorString = errorString;
}

/*!
  \internal
  returns true if the job has been cancelled by the WebKit framework
*/
bool QWebNetworkJob::cancelled() const
{
    return !d->resourceHandle;
}

/*!
  \internal
  reference the job.
*/
void QWebNetworkJob::ref()
{
    ++d->ref;
}

/*!
  \internal
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
   \internal
   Returns the network interface that is associated with this job.
*/
QWebNetworkInterface *QWebNetworkJob::networkInterface() const
{
    return d->interface;
}

/*!
   \internal
   Returns the network interface that is associated with this job.
*/
QWebFrame *QWebNetworkJob::frame() const
{
    if (d->resourceHandle) {
        ResourceHandleInternal *rhi = d->resourceHandle->getInternal();
        if (rhi) {
            return rhi->m_frame;
        }
    }
    return 0;
}

QWebNetworkJob::JobStatus QWebNetworkJob::status() const
{
    return d->jobStatus;
}

void QWebNetworkJob::setStatus(const JobStatus& status)
{
    d->jobStatus = status;
}

/*!
  \class QWebNetworkManager
  \internal
*/
QWebNetworkManager::QWebNetworkManager()
    : QObject(0)
    , m_scheduledWork(false)
{
    connect(this, SIGNAL(scheduleWork()), SLOT(doWork()), Qt::QueuedConnection);
}

QWebNetworkManager *QWebNetworkManager::self()
{
    // ensure everything's constructed and connected
    QWebNetworkInterface::defaultInterface();

    return s_manager;
}

bool QWebNetworkManager::add(ResourceHandle *handle, QWebNetworkInterface *interface, JobMode jobMode)
{
    if (!interface)
        interface = s_default_interface;

    ASSERT(interface);

    QWebNetworkJob *job = new QWebNetworkJob();
    handle->getInternal()->m_job = job;
    job->d->resourceHandle = handle;
    job->d->interface = interface;

    job->d->request.init(handle->request());

    const QString method = handle->getInternal()->m_request.httpMethod();
    if (method != "POST" && method != "GET" && method != "HEAD") {
        qWarning("REQUEST: [%s]\n", qPrintable(job->d->request.httpHeader.toString()));
        return false;
    }

    DEBUG() << "QWebNetworkManager::add:" <<  job->d->request.httpHeader.toString();

    if (jobMode == SynchronousJob) {
        Q_ASSERT(!m_synchronousJobs.contains(job));
        m_synchronousJobs[job] = 1;
    }

    interface->addJob(job);

    return true;
}

void QWebNetworkManager::cancel(ResourceHandle *handle)
{
    QWebNetworkJob *job = handle->getInternal()->m_job;
    if (!job)
        return;
    DEBUG() << "QWebNetworkManager::cancel:" <<  job->d->request.httpHeader.toString();
    job->d->resourceHandle = 0;
    job->d->interface->cancelJob(job);
    handle->getInternal()->m_job = 0;
}

/*!
  \internal
*/
void QWebNetworkManager::started(QWebNetworkJob *job)
{
    Q_ASSERT(job->d);
    Q_ASSERT(job->status() == QWebNetworkJob::JobCreated ||
             job->status() == QWebNetworkJob::JobRecreated);

    job->setStatus(QWebNetworkJob::JobStarted);
    ResourceHandleClient* client = 0;
    if (job->d->resourceHandle) {
        client = job->d->resourceHandle->client();
        if (!client)
            return;
    } else {
        return;
    }

    DEBUG() << "ResourceHandleManager::receivedResponse:";
    DEBUG() << job->d->response.toString();

    QStringList cookies = job->d->response.allValues("Set-Cookie");
    KURL url(job->url());
    foreach (QString c, cookies) {
        QCookieJar::cookieJar()->setCookies(url, url, c);
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
            contentType = MIMETypeRegistry::getMIMETypeForExtension(extension);
        }
    }
//     qDebug() << "Content-Type=" << contentType;
//     qDebug() << "Encoding=" << encoding;


    ResourceResponse response(url, contentType,
                              0 /* FIXME */,
                              encoding,
                              String() /* FIXME */);

    int statusCode = job->d->response.statusCode();
    if (job->url().scheme() != QLatin1String("file"))
        response.setHTTPStatusCode(statusCode);
    else if (statusCode == 404)
        response.setHTTPStatusCode(statusCode);

    /* Fill in the other fields */

    if (statusCode >= 300 && statusCode < 400) {
        // we're on a redirect page! if the 'Location:' field is valid, we redirect
        QString location = job->d->response.value("location");
        DEBUG() << "Redirection";
        if (!location.isEmpty()) {
            QUrl newUrl = job->d->request.url.resolved(location);
            if (job->d->resourceHandle) {
                ResourceRequest newRequest = job->d->resourceHandle->request();
                newRequest.setURL(KURL(newUrl));
                if (client)
                    client->willSendRequest(job->d->resourceHandle, newRequest, response);
            }

            QString method;
            if (statusCode == 302 || statusCode == 303) {
                // this is standard-correct for 303 and practically-correct (no standard-correct) for 302
                // also, it's required for Yahoo's login process for flickr.com which responds to a POST
                // with a 302 which must be GET'ed
                method = "GET";
                job->d->request.httpHeader.setContentLength(0);
            } else {
                method = job->d->request.httpHeader.method();
            }
            job->d->request.httpHeader.setRequest(method,
                                                  newUrl.toString(QUrl::RemoveScheme|QUrl::RemoveAuthority));
            job->d->request.setURL(newUrl);
            job->d->redirected = true;
            return;
        }
    }

    if (client)
        client->didReceiveResponse(job->d->resourceHandle, response);

}

void QWebNetworkManager::data(QWebNetworkJob *job, const QByteArray &data)
{
    Q_ASSERT(job->status() == QWebNetworkJob::JobStarted ||
             job->status() == QWebNetworkJob::JobReceivingData);

    job->setStatus(QWebNetworkJob::JobReceivingData);
    ResourceHandleClient* client = 0;
    if (job->d->resourceHandle) {
        client = job->d->resourceHandle->client();
        if (!client)
            return;
    } else {
        return;
    }

    if (job->d->redirected)
        return; // don't emit the "Document has moved here" type of HTML

    DEBUG() << "receivedData" << job->d->request.url.path();
    if (client)
        client->didReceiveData(job->d->resourceHandle, data.constData(), data.length(), data.length() /*FixMe*/);

}

void QWebNetworkManager::finished(QWebNetworkJob *job, int errorCode)
{
    Q_ASSERT(errorCode == 1 ||
             job->status() == QWebNetworkJob::JobStarted ||
             job->status() == QWebNetworkJob::JobReceivingData);

    if (m_synchronousJobs.contains(job))
        m_synchronousJobs.remove(job);

    job->setStatus(QWebNetworkJob::JobFinished);
    ResourceHandleClient* client = 0;
    if (job->d->resourceHandle) {
        client = job->d->resourceHandle->client();
        if (!client)
            return;
    } else {
        job->deref();
        return;
    }

    DEBUG() << "receivedFinished" << errorCode << job->url();

    if (job->d->redirected) {
        job->d->redirected = false;
        job->setStatus(QWebNetworkJob::JobRecreated);
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
                                          job->d->request.url.toString(), job->d->errorString));
        } else {
            client->didFinishLoading(job->d->resourceHandle);
        }
    }

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
    connection->deleteLater();
}

void QWebNetworkInterfacePrivate::sendFileData(QWebNetworkJob* job, int statusCode, const QByteArray &data)
{
    int error = statusCode >= 400 ? 1 : 0;
    if (!job->cancelled()) {
        QHttpResponseHeader response;
        response.setStatusLine(statusCode);
        response.setContentLength(data.length());
        job->setResponse(response);
        q->started(job);
        if (!data.isEmpty())
            q->data(job, data);
    }
    q->finished(job, error);
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
        header = data.mid(5, index - 5).toLower();
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
        data = decodePercentEncoding(data);
    }

    if (header.isEmpty())
        header = "text/plain;charset=US-ASCII";
    int statusCode = 200;
    QHttpResponseHeader response;
    response.setContentType(header);
    response.setContentLength(data.size());
    job->setResponse(response);

    int error = statusCode >= 400 ? 1 : 0;
    q->started(job);
    if (!data.isEmpty())
        q->data(job, data);
    q->finished(job, error);
}

void QWebNetworkManager::queueStart(QWebNetworkJob* job)
{
    Q_ASSERT(job->d);

    QMutexLocker locker(&m_queueMutex);
    job->ref();
    m_pendingWork.append(new JobWork(job));
    doScheduleWork();
}

void QWebNetworkManager::queueData(QWebNetworkJob* job, const QByteArray& data)
{
    ASSERT(job->d);

    QMutexLocker locker(&m_queueMutex);
    job->ref();
    m_pendingWork.append(new JobWork(job, data));
    doScheduleWork();
}

void QWebNetworkManager::queueFinished(QWebNetworkJob* job, int errorCode)
{
    Q_ASSERT(job->d);

    QMutexLocker locker(&m_queueMutex);
    job->ref();
    m_pendingWork.append(new JobWork(job, errorCode));
    doScheduleWork();
}

void QWebNetworkManager::doScheduleWork()
{
    if (!m_scheduledWork) {
        m_scheduledWork = true;
        emit scheduleWork();
    }
}


/*
 * We will work on a copy of m_pendingWork. While dispatching m_pendingWork
 * new work will be added that will be handled at a later doWork call. doWork
 * will be called we set m_scheduledWork to false early in this method.
 */
void QWebNetworkManager::doWork()
{
    m_queueMutex.lock();
    m_scheduledWork = false;
    bool hasSyncJobs = m_synchronousJobs.size();
    const QHash<QWebNetworkJob*, int> syncJobs = m_synchronousJobs;
    m_queueMutex.unlock();

    foreach (JobWork* work, m_pendingWork) {
        if (hasSyncJobs && !syncJobs.contains(work->job))
            continue;

        if (work->workType == JobWork::JobStarted)
            started(work->job);
        else if (work->workType == JobWork::JobData) {
            // This job was not yet started
            if (static_cast<int>(work->job->status()) < QWebNetworkJob::JobStarted)
                continue;

            data(work->job, work->data);
        } else if (work->workType == JobWork::JobFinished) {
            // This job was not yet started... we have no idea if data comes by...
            // and it won't start in case of errors
            if (static_cast<int>(work->job->status()) < QWebNetworkJob::JobStarted && work->errorCode != 1)
                continue;

            finished(work->job, work->errorCode);
        }

        m_queueMutex.lock();
        m_pendingWork.removeAll(work);
        m_queueMutex.unlock();

        work->job->deref();
        delete work;
    }

    m_queueMutex.lock();
    if (hasSyncJobs && m_synchronousJobs.size() == 0)
        doScheduleWork();
    m_queueMutex.unlock();
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

static bool gRoutineAdded = false;

static void gCleanupInterface()
{
    delete s_default_interface;
    s_default_interface = 0;
}

/*!
  \internal
  Sets a new default interface that will be used by all of WebKit
  for downloading data from the internet.
*/
void QWebNetworkInterface::setDefaultInterface(QWebNetworkInterface *defaultInterface)
{
    if (s_default_interface == defaultInterface)
        return;
    if (s_default_interface)
        delete s_default_interface;
    s_default_interface = defaultInterface;
    if (!gRoutineAdded) {
        qAddPostRoutine(gCleanupInterface);
        gRoutineAdded = true;
    }
}

/*!
  \internal
  Returns the default interface that will be used by WebKit. If no
  default interface has been set, QtWebkit will create an instance of
  QWebNetworkInterface to do the work.
*/
QWebNetworkInterface *QWebNetworkInterface::defaultInterface()
{
    if (!s_default_interface) {
        setDefaultInterface(new QWebNetworkInterface);
    }
    return s_default_interface;
}


/*!
  \internal
  Constructs a QWebNetworkInterface object.
*/
QWebNetworkInterface::QWebNetworkInterface(QObject *parent)
    : QObject(parent)
{
    d = new QWebNetworkInterfacePrivate;
    d->q = this;

    if (!s_manager)
        s_manager = new QWebNetworkManager;
}

/*!
  \internal
  Destructs the QWebNetworkInterface object.
*/
QWebNetworkInterface::~QWebNetworkInterface()
{
    delete d;
}

/*!
  \internal
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
    if (protocol == QLatin1String("http") || protocol == QLatin1String("https")) {
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
    QString path = url.path();
    if (protocol == QLatin1String("qrc")) {
        protocol = "file";
        path.prepend(QLatin1Char(':'));
    }

    if (!(protocol.isEmpty() || protocol == QLatin1String("file"))) {
        statusCode = 404;
    } else {
        // we simply ignore post data here.
        QFile f(path);
        DEBUG() << "opening" << QString(url.path());

        if (f.open(QIODevice::ReadOnly)) {
            QHttpResponseHeader response;
            response.setStatusLine(200);
            job->setResponse(response);
            data = f.readAll();
        } else {
            statusCode = 404;
        }
    }

    if (statusCode == 404) {
        QHttpResponseHeader response;
        response.setStatusLine(404);
        job->setResponse(response);
    }

    d->sendFileData(job, statusCode, data);
}

/*!
  \internal
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
    if (protocol == QLatin1String("http") || protocol == QLatin1String("https"))
        QWebNetworkManager::self()->cancelHttpJob(job);
}

/*!
  \internal
*/
void QWebNetworkInterface::started(QWebNetworkJob* job)
{
    Q_ASSERT(s_manager);
    s_manager->queueStart(job);
}

/*!
  \internal
*/
void QWebNetworkInterface::data(QWebNetworkJob* job, const QByteArray& data)
{
    Q_ASSERT(s_manager);
    s_manager->queueData(job, data);
}

/*!
  \internal
*/
void QWebNetworkInterface::finished(QWebNetworkJob* job, int errorCode)
{
    Q_ASSERT(s_manager);
    s_manager->queueFinished(job, errorCode);
}

/*!
  \fn void QWebNetworkInterface::sslErrors(QWebFrame *frame, const QUrl& url, const QList<QSslError>& errors, bool *continueAnyway);
  \internal

   Signal is emitted when an SSL error occurs.
*/

/*!
  \fn void QWebNetworkInterface::authenticate(QWebFrame *frame, const QUrl& url, const QString& hostname, quint16 port, QAuthenticator *auth);
  \internal

  Signal is emitted when network authentication is required.
*/

/*!
  \fn void QWebNetworkInterface::authenticateProxy(QWebFrame *frame, const QUrl& url, const QNetworkProxy& proxy, QAuthenticator *auth);
  \internal

  Signal is emitted when proxy authentication is required.
*/

/////////////////////////////////////////////////////////////////////////////
WebCoreHttp::WebCoreHttp(QObject* parent, const HostInfo &hi)
    : QObject(parent), info(hi),
      m_inCancel(false)
{
    for (int i = 0; i < 2; ++i) {
        connection[i].http = new QHttp(info.host, (hi.protocol == QLatin1String("https")) ? QHttp::ConnectionModeHttps : QHttp::ConnectionModeHttp, info.port);
        connect(connection[i].http, SIGNAL(responseHeaderReceived(const QHttpResponseHeader&)),
                this, SLOT(onResponseHeaderReceived(const QHttpResponseHeader&)));
        connect(connection[i].http, SIGNAL(readyRead(const QHttpResponseHeader&)),
                this, SLOT(onReadyRead()));
        connect(connection[i].http, SIGNAL(requestFinished(int, bool)),
                this, SLOT(onRequestFinished(int, bool)));
        connect(connection[i].http, SIGNAL(done(bool)),
                this, SLOT(onDone(bool)));
        connect(connection[i].http, SIGNAL(stateChanged(int)),
                this, SLOT(onStateChanged(int)));
        connect(connection[i].http, SIGNAL(authenticationRequired(const QString&, quint16, QAuthenticator*)),
                this, SLOT(onAuthenticationRequired(const QString&, quint16, QAuthenticator*)));
        connect(connection[i].http, SIGNAL(proxyAuthenticationRequired(const QNetworkProxy&, QAuthenticator*)),
                this, SLOT(onProxyAuthenticationRequired(const QNetworkProxy&, QAuthenticator*)));
        connect(connection[i].http, SIGNAL(sslErrors(const QList<QSslError>&)),
                this, SLOT(onSslErrors(const QList<QSslError>&)));
    }
}

WebCoreHttp::~WebCoreHttp()
{
    connection[0].http->deleteLater();
    connection[1].http->deleteLater();
}

void WebCoreHttp::request(QWebNetworkJob *job)
{
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
            job->networkInterface()->finished(job, 1);
            job = 0;
        }
    }
    if (!job)
        return;

    QHttp *http = connection[c].http;

    connection[c].current = job;
    connection[c].id = -1;
#ifndef QT_NO_NETWORKPROXY
    int proxyId = http->setProxy(job->frame()->page()->networkProxy());
#endif

    QByteArray postData = job->postData();
    if (!postData.isEmpty())
        connection[c].id = http->request(job->httpHeader(), postData);
    else
        connection[c].id = http->request(job->httpHeader());

    DEBUG() << "WebCoreHttp::scheduleNextRequest: using connection" << c;
    DEBUG() << job->httpHeader().toString();
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
    Q_ASSERT(connection[c].current);
    return c;
}

void WebCoreHttp::onResponseHeaderReceived(const QHttpResponseHeader &resp)
{
    QHttp *http = qobject_cast<QHttp*>(sender());
    if (http->currentId() == 0) {
        qDebug() << "ERROR!  Invalid job id.  Why?"; // foxnews.com triggers this
        return;
    }
    int c = getConnection();
    QWebNetworkJob *job = connection[c].current;
    DEBUG() << "WebCoreHttp::slotResponseHeaderReceived connection=" << c;
    DEBUG() << resp.toString();

    job->setResponse(resp);

    job->networkInterface()->started(job);
}

void WebCoreHttp::onReadyRead()
{
    QHttp *http = qobject_cast<QHttp*>(sender());
    if (http->currentId() == 0) {
        qDebug() << "ERROR!  Invalid job id.  Why?"; // foxnews.com triggers this
        return;
    }
    int c = getConnection();
    QWebNetworkJob *job = connection[c].current;
    Q_ASSERT(http == connection[c].http);
    //DEBUG() << "WebCoreHttp::slotReadyRead connection=" << c;

    QByteArray data;
    data.resize(http->bytesAvailable());
    http->read(data.data(), data.length());
    job->networkInterface()->data(job, data);
}

void WebCoreHttp::onRequestFinished(int id, bool error)
{
    int c = getConnection();
    if (connection[c].id != id) {
        return;
    }

    QWebNetworkJob *job = connection[c].current;
    if (!job) {
        scheduleNextRequest();
        return;
    }

    QHttp *http = connection[c].http;
    DEBUG() << "WebCoreHttp::slotFinished connection=" << c << error << job;
    if (error) {
        DEBUG() << "   error: " << http->errorString();
        job->setErrorString(http->errorString());
    }

    if (!error && http->bytesAvailable()) {
        QByteArray data;
        data.resize(http->bytesAvailable());
        http->read(data.data(), data.length());
        job->networkInterface()->data(job, data);
    }

    job->networkInterface()->finished(job, error ? 1 : 0);
    connection[c].current = 0;
    connection[c].id = -1;
    scheduleNextRequest();
}

void WebCoreHttp::onDone(bool error)
{
    DEBUG() << "WebCoreHttp::onDone" << error;
}

void WebCoreHttp::onStateChanged(int state)
{
    DEBUG() << "State changed to" << state << "and connections are" << connection[0].current << connection[1].current;
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
        request->networkInterface()->finished(request, 1);

    if (m_pendingRequests.isEmpty()
        && !connection[0].current && !connection[1].current)
        emit connectionClosed(info);
}

void WebCoreHttp::onSslErrors(const QList<QSslError>& errors)
{
    int c = getConnection();
    QWebNetworkJob *job = connection[c].current;
    if (job) {
        bool continueAnyway = false;
        emit job->networkInterface()->sslErrors(job->frame(), job->url(), errors, &continueAnyway);
#ifndef QT_NO_OPENSSL
        if (continueAnyway)
            connection[c].http->ignoreSslErrors();
#endif
    }
}

void WebCoreHttp::onAuthenticationRequired(const QString& hostname, quint16 port, QAuthenticator *auth)
{
    int c = getConnection();
    QWebNetworkJob *job = connection[c].current;
    if (job) {
        emit job->networkInterface()->authenticate(job->frame(), job->url(), hostname, port, auth);
        if (auth->isNull())
            connection[c].http->abort();
    }
}

void WebCoreHttp::onProxyAuthenticationRequired(const QNetworkProxy& proxy, QAuthenticator *auth)
{
    int c = getConnection();
    QWebNetworkJob *job = connection[c].current;
    if (job) {
        emit job->networkInterface()->authenticateProxy(job->frame(), job->url(), proxy, auth);
        if (auth->isNull())
            connection[c].http->abort();
    }
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

#endif
