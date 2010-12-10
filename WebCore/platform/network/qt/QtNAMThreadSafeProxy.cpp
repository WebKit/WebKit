/*
    Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

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
#include "QtNAMThreadSafeProxy.h"

#include "Assertions.h"
#include <QAbstractNetworkCache>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QStringList>

// Use unused variables to be able to call qRegisterMetaType statically.
static int dummyStaticVar1 = qRegisterMetaType<QFutureInterface<bool> >("QFutureInterface<bool>");
static int dummyStaticVar2 = qRegisterMetaType<QFutureInterface<QList<QNetworkCookie> > >("QFutureInterface<QList<QNetworkCookie> >");

namespace WebCore {

QtNAMThreadSafeProxy::QtNAMThreadSafeProxy(QNetworkAccessManager *manager)
    : m_manager(manager)
{
    moveToThread(manager->thread());

    connect(this, SIGNAL(localSetCookiesRequested(const QUrl&, const QString&)), SLOT(localSetCookies(const QUrl&, const QString&)));
    connect(this, SIGNAL(localCookiesForUrlRequested(QFutureInterface<QList<QNetworkCookie> >, const QUrl&)), SLOT(localCookiesForUrl(QFutureInterface<QList<QNetworkCookie> >, const QUrl&)));
    connect(this, SIGNAL(localWillLoadFromCacheRequested(QFutureInterface<bool>, const QUrl&)), SLOT(localWillLoadFromCache(QFutureInterface<bool>, const QUrl&)));
}

void QtNAMThreadSafeProxy::localSetCookies(const QUrl& url, const QString& cookies)
{
    QList<QNetworkCookie> cookieList = QNetworkCookie::parseCookies(cookies.toAscii());
    QList<QNetworkCookie>::Iterator it = cookieList.begin();
    while (it != cookieList.end()) {
        if (it->isHttpOnly())
            it = cookieList.erase(it);
        else
            ++it;
    }
    m_manager->cookieJar()->setCookiesFromUrl(cookieList, url);
}

void QtNAMThreadSafeProxy::localCookiesForUrl(QFutureInterface<QList<QNetworkCookie> > fi, const QUrl& url)
{
    fi.reportResult(m_manager->cookieJar()->cookiesForUrl(url));
    fi.reportFinished();
}

void QtNAMThreadSafeProxy::localWillLoadFromCache(QFutureInterface<bool> fi, const QUrl& url)
{
    bool retVal = false;
    if (m_manager->cache())
        retVal = m_manager->cache()->metaData(url).isValid();

    fi.reportFinished(&retVal);
}

QtNetworkReplyThreadSafeProxy::QtNetworkReplyThreadSafeProxy(QNetworkAccessManager *manager)
    : m_manager(manager)
    , m_reply(0)
    , m_forwardingDefered(false)
    , m_contentLengthHeader(0)
    , m_error(QNetworkReply::NoError)
    , m_httpStatusCode(0)
{
    moveToThread(manager->thread());

    // This might be unnecessarily heavy to do for each request while we could have the same wrapper for the manager instead
    connect(this, SIGNAL(localGetRequested(const QNetworkRequest&)), SLOT(localGet(const QNetworkRequest&)));
    connect(this, SIGNAL(localPostRequested(const QNetworkRequest&, QIODevice*)), SLOT(localPost(const QNetworkRequest&, QIODevice*)));
    connect(this, SIGNAL(localHeadRequested(const QNetworkRequest&)), SLOT(localHead(const QNetworkRequest&)));
    connect(this, SIGNAL(localPutRequested(const QNetworkRequest&, QIODevice*)), SLOT(localPut(const QNetworkRequest&, QIODevice*)));
    connect(this, SIGNAL(localDeleteResourceRequested(const QNetworkRequest&)), SLOT(localDeleteResource(const QNetworkRequest&)));
    connect(this, SIGNAL(localCustomRequestRequested(const QNetworkRequest&, const QByteArray&)), SLOT(localCustomRequest(const QNetworkRequest&, const QByteArray&)));
    connect(this, SIGNAL(localAbortRequested()), SLOT(localAbort()));
    connect(this, SIGNAL(localSetForwardingDeferedRequested(bool)), SLOT(localSetForwardingDefered(bool)));
}

QtNetworkReplyThreadSafeProxy::~QtNetworkReplyThreadSafeProxy()
{
    delete m_reply;
}

void QtNetworkReplyThreadSafeProxy::localGet(const QNetworkRequest& request)
{
    localSetReply(m_manager->get(request));
}

void QtNetworkReplyThreadSafeProxy::localPost(const QNetworkRequest& request, QIODevice* data)
{
    localSetReply(m_manager->post(request, data));
}

void QtNetworkReplyThreadSafeProxy::localHead(const QNetworkRequest& request)
{
    localSetReply(m_manager->head(request));
}

void QtNetworkReplyThreadSafeProxy::localPut(const QNetworkRequest& request, QIODevice* data)
{
    localSetReply(m_manager->put(request, data));
}

void QtNetworkReplyThreadSafeProxy::localDeleteResource(const QNetworkRequest& request)
{
    localSetReply(m_manager->deleteResource(request));
}

void QtNetworkReplyThreadSafeProxy::localCustomRequest(const QNetworkRequest& request, const QByteArray& verb)
{
#if QT_VERSION >= QT_VERSION_CHECK(4, 7, 0)
    localSetReply(m_manager->sendCustomRequest(request, verb));
#endif
}

void QtNetworkReplyThreadSafeProxy::localAbort()
{
    if (m_reply)
        m_reply->abort();
}

void QtNetworkReplyThreadSafeProxy::localForwardData()
{
    if (m_reply->bytesAvailable()) {
        QByteArray data = m_reply->read(m_reply->bytesAvailable());
        emit dataReceived(data);
    }
}

void QtNetworkReplyThreadSafeProxy::localSetForwardingDefered(bool forwardingDefered)
{
    if (m_forwardingDefered && !forwardingDefered)
        localForwardData();
    m_forwardingDefered = forwardingDefered;
}

void QtNetworkReplyThreadSafeProxy::localMirrorMembers()
{
    ASSERT(m_reply);
    QMutexLocker lock(&m_mirroredMembersMutex);

    m_contentDispositionHeader = m_reply->rawHeader("Content-Disposition");
    m_contentLengthHeader = m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
    m_contentTypeHeader = m_reply->header(QNetworkRequest::ContentTypeHeader).toString();
    m_error = m_reply->error();
    m_errorString = m_reply->errorString();
    m_httpReasonPhrase = m_reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toByteArray();
    m_httpStatusCode = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    m_url = m_reply->url();
    m_redirectionTarget = m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
#if QT_VERSION >= QT_VERSION_CHECK(4, 7, 0)
    m_rawHeaderPairs = m_reply->rawHeaderPairs();
#else
    m_rawHeaderPairs.clear();
    foreach (const QByteArray& headerName, m_reply->rawHeaderList())
        m_rawHeaderPairs.append(RawHeaderPair(headerName, m_reply->rawHeader(headerName)));
#endif
}

void QtNetworkReplyThreadSafeProxy::localSetReply(QNetworkReply *reply)
{
    ASSERT(!m_reply);
    m_reply = reply;
    m_reply->setParent(0);
    connect(m_reply, SIGNAL(readyRead()), this, SLOT(localForwardData()));
    // Make sure localMirrorMembers() is called before the outward signal
    connect(m_reply, SIGNAL(finished()), this, SLOT(localMirrorMembers()), Qt::DirectConnection);
    connect(m_reply, SIGNAL(finished()), this, SIGNAL(finished()));
    // Make sure localMirrorMembers() is called before the outward signal
    connect(m_reply, SIGNAL(metaDataChanged()), this, SLOT(localMirrorMembers()), Qt::DirectConnection);
    connect(m_reply, SIGNAL(metaDataChanged()), this, SIGNAL(metaDataChanged()));
    connect(m_reply, SIGNAL(uploadProgress(qint64, qint64)), this, SIGNAL(uploadProgress(qint64, qint64)));
}


}
