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
#ifndef QNETWORKREPLYHANDLER_H
#define QNETWORKREPLYHANDLER_H

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

class QNetworkReplyHandler : public QObject
{
    Q_OBJECT
public:
    enum LoadMode {
        LoadNormal,
        LoadDeferred,
        LoadResuming
    };

    QNetworkReplyHandler(ResourceHandle *handle, LoadMode);
    void setLoadMode(LoadMode);

    QNetworkReply* reply() const { return m_reply; }

    void abort();

    QNetworkReply* release();

signals:
    void processQueuedItems();

private slots:
    void finish();
    void sendResponseIfNeeded();
    void forwardData();
    void sendQueuedItems();
    void uploadProgress(qint64 bytesSent, qint64 bytesTotal);

private:
    void start();
    void resetState();

    QNetworkReply* m_reply;
    ResourceHandle* m_resourceHandle;
    bool m_redirected;
    bool m_responseSent;
    bool m_responseDataSent;
    LoadMode m_loadMode;
    QNetworkAccessManager::Operation m_method;
    QNetworkRequest m_request;

    // defer state holding
    bool m_shouldStart;
    bool m_shouldFinish;
    bool m_shouldSendResponse;
    bool m_shouldForwardData;
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

protected:
    qint64 readData(char*, qint64);
    qint64 writeData(const char*, qint64);

private:
    void moveToNextElement();

private:
    Vector<FormDataElement> m_formElements;
    QFile* m_currentFile;
    qint64 m_currentDelta;
};

}

#endif // QNETWORKREPLYHANDLER_H
