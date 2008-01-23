/*
    Copyright (C) 2007-2008 Trolltech ASA

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

#if QT_VERSION >= 0x040400

#include <QNetworkRequest>
#include <QNetworkAccessManager>

#include "FormData.h"

class QFile;
class QNetworkReply;

namespace WebCore {

class ResourceHandle;

class QNetworkReplyHandler : public QObject
{
    Q_OBJECT
public:
    QNetworkReplyHandler(ResourceHandle *handle);

    QNetworkReply* reply() const { return m_reply; }

    void abort();

    QNetworkReply *release();

private slots:
    void finish();
    void sendResponseIfNeeded();
    void forwardData();

private:
    void start();

    QNetworkReply* m_reply;
    ResourceHandle* m_resourceHandle;
    bool m_redirected;
    bool m_responseSent;
    QNetworkAccessManager::Operation m_method;
    QNetworkRequest m_request;
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

    void setParent(QNetworkReply*);
    bool isSequential() const;

protected:
    qint64 readData(char*, qint64);
    qint64 writeData(const char*, qint64);

private Q_SLOTS:
    void slotFinished();

private:
    void moveToNextElement();

private:
    Vector<FormDataElement> m_formElements;
    QFile* m_currentFile;
    qint64 m_currentDelta;
};

}

#endif

#endif // QNETWORKREPLYHANDLER_H
