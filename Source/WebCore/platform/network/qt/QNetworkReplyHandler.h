/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

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
#ifndef QNetworkReplyHandler_h
#define QNetworkReplyHandler_h

#include <QObject>

#include <QNetworkRequest>
#include <QNetworkAccessManager>

#include "FormData.h"

QT_BEGIN_NAMESPACE
class QFile;
class QNetworkReply;
QT_END_NAMESPACE

namespace WebCore {

class ResourceHandle;
class ResourceResponse;
class QNetworkReplyHandler;

class QNetworkReplyHandlerCallQueue {
public:
    QNetworkReplyHandlerCallQueue(QNetworkReplyHandler*, bool deferSignals);
    bool deferSignals() const { return m_deferSignals; }
    void setDeferSignals(bool);

    typedef void (QNetworkReplyHandler::*EnqueuedCall)();
    void push(EnqueuedCall method);
    void clear() { m_enqueuedCalls.clear(); }

    void lock();
    void unlock();
private:
    QNetworkReplyHandler* m_replyHandler;
    int m_locks;
    bool m_deferSignals;
    bool m_flushing;
    QList<EnqueuedCall> m_enqueuedCalls;

    void flush();
};

class QNetworkReplyWrapper : public QObject {
    Q_OBJECT
public:
    QNetworkReplyWrapper(QNetworkReplyHandlerCallQueue*, QNetworkReply*, QObject* parent = 0);
    ~QNetworkReplyWrapper();

    QNetworkReply* reply() const { return m_reply; }
    QNetworkReply* release();

    void synchronousLoad() { receiveMetaData(); }

    QUrl redirectionTargetUrl() const { return m_redirectionTargetUrl; }
    QString encoding() const { return m_encoding; }
    QString advertisedMimeType() const { return m_advertisedMimeType; }

private Q_SLOTS:
    void receiveMetaData();
    void didReceiveFinished();
    void didReceiveReadyRead();

private:
    void resetConnections();

    QNetworkReply* m_reply;
    QUrl m_redirectionTargetUrl;

    QString m_encoding;
    QString m_advertisedMimeType;

    QNetworkReplyHandlerCallQueue* m_queue;
};

class QNetworkReplyHandler : public QObject
{
    Q_OBJECT
public:
    enum LoadType {
        AsynchronousLoad,
        SynchronousLoad
    };

    QNetworkReplyHandler(ResourceHandle*, LoadType, bool deferred = false);
    void setLoadingDeferred(bool deferred) { m_queue.setDeferSignals(deferred); }

    QNetworkReply* reply() const { return m_replyWrapper ? m_replyWrapper->reply() : 0; }

    void abort();

    QNetworkReply* release();

    void synchronousLoad();

public slots:
    void finish();
    void sendResponseIfNeeded();
    void forwardData();
    void uploadProgress(qint64 bytesSent, qint64 bytesTotal);

private:
    void start();
    void resetState();
    String httpMethod() const;
    void redirect(ResourceResponse&, const QUrl&);
    bool wasAborted() const { return !m_resourceHandle; }
    QNetworkReply* sendNetworkRequest();

    QNetworkReplyWrapper* m_replyWrapper;
    ResourceHandle* m_resourceHandle;
    bool m_redirected;
    bool m_responseContainsData;
    LoadType m_loadType;
    QNetworkAccessManager::Operation m_method;
    QNetworkRequest m_request;

    // defer state holding
    int m_redirectionTries;

    QNetworkReplyHandlerCallQueue m_queue;
};

// Self destructing QIODevice for FormData
//  For QNetworkAccessManager::put we will have to gurantee that the
//  QIODevice is valid as long finished() of the QNetworkReply has not
//  been emitted. With the presence of QNetworkReplyHandler::release I do
//  not want to gurantee this.
class FormDataIODevice : public QIODevice {
    Q_OBJECT
public:
    FormDataIODevice(FormData*);
    ~FormDataIODevice();

    bool isSequential() const;
    qint64 getFormDataSize() const { return m_fileSize + m_dataSize; }

protected:
    qint64 readData(char*, qint64);
    qint64 writeData(const char*, qint64);

private:
    void moveToNextElement();
    qint64 computeSize();
    void openFileForCurrentElement();

private:
    Vector<FormDataElement> m_formElements;
    QFile* m_currentFile;
    qint64 m_currentDelta;
    qint64 m_fileSize;
    qint64 m_dataSize;
};

}

#endif // QNetworkReplyHandler_h
