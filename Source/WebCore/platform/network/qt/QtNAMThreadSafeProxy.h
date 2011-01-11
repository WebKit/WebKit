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
#ifndef QtNAMThreadSafeProxy_h
#define QtNAMThreadSafeProxy_h

#include <QMutex>
#include <QNetworkCookie>
#include <QNetworkReply>
#include <QObject>
#include <QWaitCondition>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkRequest;
class QUrl;
QT_END_NAMESPACE

namespace WebCore {

class QtNAMThreadSafeProxy : public QObject {
    Q_OBJECT
public:
    QtNAMThreadSafeProxy(QNetworkAccessManager *manager);

    void setCookies(const QUrl& url, const QString& cookies)
    {
        emit localSetCookiesRequested(url, cookies);
    }

    QList<QNetworkCookie> cookiesForUrl(const QUrl& url)
    {
        bool done = false;
        QList<QNetworkCookie> result;
        emit localCookiesForUrlRequested(url, &done, &result);

        QMutexLocker lock(&m_resultMutex);
        while (!done)
            m_resultWaitCondition.wait(&m_resultMutex);
        return result;
    }

    bool willLoadFromCache(const QUrl& url)
    {
        bool done = false;
        bool result;
        emit localWillLoadFromCacheRequested(url, &done, &result);

        QMutexLocker lock(&m_resultMutex);
        while (!done)
            m_resultWaitCondition.wait(&m_resultMutex);
        return result;
    }

signals:
    void localSetCookiesRequested(const QUrl&, const QString& cookies);
    void localCookiesForUrlRequested(const QUrl&, bool* done, QList<QNetworkCookie>* result);
    void localWillLoadFromCacheRequested(const QUrl&, bool* done, bool* result);

private slots:
    void localSetCookies(const QUrl&, const QString& cookies);
    void localCookiesForUrl(const QUrl&, bool* done, QList<QNetworkCookie>* result);
    void localWillLoadFromCache(const QUrl&, bool* done, bool* result);

private:
    QNetworkAccessManager* m_manager;
    QMutex m_resultMutex;
    QWaitCondition m_resultWaitCondition;
};


class QtNetworkReplyThreadSafeProxy : public QObject {
    Q_OBJECT
public:
    typedef QPair<QByteArray, QByteArray> RawHeaderPair;
    QtNetworkReplyThreadSafeProxy(QNetworkAccessManager *manager);
    ~QtNetworkReplyThreadSafeProxy();
    void abort()
    {
        emit localAbortRequested();
    }
    void setForwardingDefered(bool forwardingDefered)
    {
        emit localSetForwardingDeferedRequested(forwardingDefered);
    }

    QByteArray contentDispositionHeader() { QMutexLocker lock(&m_mirroredMembersMutex); return m_contentDispositionHeader; }
    qlonglong contentLengthHeader() { QMutexLocker lock(&m_mirroredMembersMutex); return m_contentLengthHeader; }
    QString contentTypeHeader() { QMutexLocker lock(&m_mirroredMembersMutex); return m_contentTypeHeader; }
    QNetworkReply::NetworkError error() { QMutexLocker lock(&m_mirroredMembersMutex); return m_error; }
    QString errorString() { QMutexLocker lock(&m_mirroredMembersMutex); return m_errorString; }
    QByteArray httpReasonPhrase() { QMutexLocker lock(&m_mirroredMembersMutex); return m_httpReasonPhrase; }
    int httpStatusCode() { QMutexLocker lock(&m_mirroredMembersMutex); return m_httpStatusCode; }
    QUrl url() { QMutexLocker lock(&m_mirroredMembersMutex); return m_url; }
    QUrl redirectionTarget() { QMutexLocker lock(&m_mirroredMembersMutex); return m_redirectionTarget; }
    QList<RawHeaderPair> rawHeaderPairs() { QMutexLocker lock(&m_mirroredMembersMutex); return m_rawHeaderPairs; }

    QNetworkReply* reply()
    {
        // Careful, acccessing the reply accross threads might be hazardous to your health
        return m_reply;
    }
public:
    void get(const QNetworkRequest &request)
    {
        emit localGetRequested(request);
    }
    void post(const QNetworkRequest &request, QIODevice* data)
    {
        emit localPostRequested(request, data);
    }
    void head(const QNetworkRequest &request)
    {
        emit localHeadRequested(request);
    }
    void put(const QNetworkRequest &request, QIODevice* data)
    {
        emit localPutRequested(request, data);
    }
    void deleteResource(const QNetworkRequest &request)
    {
        emit localDeleteResourceRequested(request);
    }
#if QT_VERSION >= QT_VERSION_CHECK(4, 7, 0)
    void sendCustomRequest(const QNetworkRequest &request, const QByteArray& verb)
    {
        emit localCustomRequestRequested(request, verb);
    }
#endif

signals:
    void localGetRequested(const QNetworkRequest& request);
    void localPostRequested(const QNetworkRequest& request, QIODevice* data);
    void localHeadRequested(const QNetworkRequest& request);
    void localPutRequested(const QNetworkRequest& request, QIODevice* data);
    void localDeleteResourceRequested(const QNetworkRequest& request);
    void localCustomRequestRequested(const QNetworkRequest& request, const QByteArray& verb);
    void localAbortRequested();
    void localSetForwardingDeferedRequested(bool forwardingDefered);

    void finished();
    void readyRead();
    void metaDataChanged();
    void uploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void dataReceived(const QByteArray &data);

private slots:
    void localGet(const QNetworkRequest& request);
    void localPost(const QNetworkRequest& request, QIODevice* data);
    void localHead(const QNetworkRequest& request);
    void localPut(const QNetworkRequest& request, QIODevice* data);
    void localDeleteResource(const QNetworkRequest& request);
    void localCustomRequest(const QNetworkRequest& request, const QByteArray& verb);
    void localAbort();
    void localForwardData();
    void localSetForwardingDefered(bool forwardingDefered);
    void localMirrorMembers();

private:
    void localSetReply(QNetworkReply *reply);

    QNetworkAccessManager *m_manager;
    QNetworkReply *m_reply;
    bool m_forwardingDefered;

    // Mirrored members
    QMutex m_mirroredMembersMutex;
    QByteArray m_contentDispositionHeader;
    qlonglong m_contentLengthHeader;
    QString m_contentTypeHeader;
    QNetworkReply::NetworkError m_error;
    QString m_errorString;
    QByteArray m_httpReasonPhrase;
    int m_httpStatusCode;
    QUrl m_url;
    QUrl m_redirectionTarget;
    QList<RawHeaderPair> m_rawHeaderPairs;
};

}


#endif // QtNAMThreadSafeProxy_h
