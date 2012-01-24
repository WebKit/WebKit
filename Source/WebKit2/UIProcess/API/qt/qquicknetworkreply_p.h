/*
 * Copyright (C) 2011 Zeno Albisser <zeno@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef qquicknetworkreply_p_h
#define qquicknetworkreply_p_h

#include "QtNetworkReplyData.h"
#include "QtNetworkRequestData.h"
#include "SharedMemory.h"
#include "qwebkitglobal.h"
#include <QNetworkAccessManager>
#include <QObject>
#include <QtDeclarative/qdeclarativelist.h>
#include <QtQuick/qquickitem.h>

class QWEBKIT_EXPORT QQuickNetworkReply : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString contentType READ contentType WRITE setContentType)
    Q_PROPERTY(QString data READ data WRITE setData)
    Q_ENUMS(QNetworkAccessManager::Operation)

public:
    QQuickNetworkReply(QObject* parent);
    QString contentType() const;
    void setContentType(const QString&);
    QNetworkAccessManager::Operation operation() const;
    void setOperation(QNetworkAccessManager::Operation);
    QString contentDisposition() const;
    void setContentDisposition(const QString&);
    QString location() const;
    void setLocation(const QString&);
    QString lastModified() const;
    void setLastModified(const QString&);
    QString cookie() const;
    void setCookie(const QString&);
    QString userAgent() const;
    void setUserAgent(const QString&);
    QString server() const;
    void setServer(const QString&);

    QString data() const;
    void setData(const QString& data);

    WebKit::QtRefCountedNetworkRequestData* networkRequestData() const;
    void setNetworkRequestData(WTF::PassRefPtr<WebKit::QtRefCountedNetworkRequestData> data);
    WebKit::QtRefCountedNetworkReplyData* networkReplyData() const;

public Q_SLOTS:
    void send();

private:
    WTF::RefPtr<WebKit::QtRefCountedNetworkRequestData> m_networkRequestData;
    WTF::RefPtr<WebKit::QtRefCountedNetworkReplyData> m_networkReplyData;
    WTF::RefPtr<WebKit::SharedMemory> m_sharedMemory;
    uint64_t m_dataLength;
};

QML_DECLARE_TYPE(QQuickNetworkReply)

#endif // qquicknetworkreply_p_h

