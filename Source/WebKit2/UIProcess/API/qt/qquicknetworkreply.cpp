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

#include "config.h"
#include "qquicknetworkreply_p.h"

#include "QtNetworkReplyData.h"
#include "QtNetworkRequestData.h"
#include "qquickwebview_p.h"
#include <QDateTime>

using namespace WebKit;

QQuickNetworkReply::QQuickNetworkReply(QObject* parent)
    : QObject(parent)
    , m_networkReplyData(adoptRef(new WebKit::QtNetworkReplyData))
{
    Q_ASSERT(parent);
}

QString QQuickNetworkReply::contentType() const
{
    return m_networkReplyData->m_contentType;
}

void QQuickNetworkReply::setContentType(const QString& contentType)
{
    m_networkReplyData->m_contentType = contentType;
}

QNetworkAccessManager::Operation QQuickNetworkReply::operation() const
{
    return m_networkReplyData->m_operation;
}

void QQuickNetworkReply::setOperation(QNetworkAccessManager::Operation operation)
{
    m_networkReplyData->m_operation = operation;
}

QString QQuickNetworkReply::contentDisposition() const
{
    return m_networkReplyData->m_contentDisposition;
}

void QQuickNetworkReply::setContentDisposition(const QString& disposition)
{
    m_networkReplyData->m_contentDisposition = disposition;
}

QString QQuickNetworkReply::location() const
{
    return m_networkReplyData->m_location;
}

void QQuickNetworkReply::setLocation(const QString& location)
{
    m_networkReplyData->m_location = location;
}

QString QQuickNetworkReply::lastModified() const
{
    return QDateTime::fromMSecsSinceEpoch(m_networkReplyData->m_lastModified).toString(Qt::ISODate);
}

void QQuickNetworkReply::setLastModified(const QString& lastModified)
{
    m_networkReplyData->m_lastModified = QDateTime::fromString(lastModified, Qt::ISODate).toMSecsSinceEpoch();
}

QString QQuickNetworkReply::cookie() const
{
    return m_networkReplyData->m_cookie;
}

void QQuickNetworkReply::setCookie(const QString& cookie)
{
    m_networkReplyData->m_cookie = cookie;
}

QString QQuickNetworkReply::userAgent() const
{
    return m_networkReplyData->m_userAgent;
}

void QQuickNetworkReply::setUserAgent(const QString& userAgent)
{
    m_networkReplyData->m_userAgent = userAgent;
}

QString QQuickNetworkReply::server() const
{
    return m_networkReplyData->m_server;
}

void QQuickNetworkReply::setServer(const QString& server)
{
    m_networkReplyData->m_server = server;
}

QString QQuickNetworkReply::data() const
{
    if (m_networkReplyData->m_dataHandle.isNull())
        return QString();
    RefPtr<SharedMemory> sm = SharedMemory::create(m_networkReplyData->m_dataHandle, SharedMemory::ReadOnly);
    if (!sm)
        return QString();

    uint64_t stringLength = m_networkReplyData->m_contentLength / sizeof(UChar);
    return QString(reinterpret_cast<const QChar*>(sm->data()), stringLength);
}

void QQuickNetworkReply::setData(const QString& data)
{
    // This function can be called several times. In this case the previous SharedMemory handles
    // will be overwritten and the previously allocated SharedMemory will die with the last handle.
    m_networkReplyData->m_contentLength = 0;

    if (data.isNull())
        return;
    const UChar* ucharData = reinterpret_cast<const UChar*>(data.constData());
    uint64_t smLength = sizeof(UChar) * data.length();

    RefPtr<SharedMemory> sm = SharedMemory::create(smLength);
    if (!sm)
        return;
    // The size of the allocated shared memory can be bigger than requested.
    // Usually the size will be rounded up to the next multiple of a page size.
    memcpy(sm->data(), ucharData, smLength);

    if (!sm->createHandle(m_networkReplyData->m_dataHandle, SharedMemory::ReadOnly))
        return;
    m_networkReplyData->m_contentLength = smLength;
}

void QQuickNetworkReply::send()
{
    QObject* schemeParent = parent()->parent();
    if (!schemeParent)
        return;
    QQuickWebViewExperimental* webViewExperimental = qobject_cast<QQuickWebViewExperimental*>(schemeParent->parent());
    if (!webViewExperimental)
        return;
    webViewExperimental->sendApplicationSchemeReply(this);
}

WTF::RefPtr<WebKit::QtNetworkRequestData> QQuickNetworkReply::networkRequestData() const
{
    return m_networkRequestData;
}

void QQuickNetworkReply::setNetworkRequestData(WTF::RefPtr<WebKit::QtNetworkRequestData> data)
{
    m_networkRequestData = data;
    m_networkReplyData->m_replyUuid = data->m_replyUuid;
}

WTF::RefPtr<WebKit::QtNetworkReplyData> QQuickNetworkReply::networkReplyData() const
{
    return m_networkReplyData;
}

#include "moc_qquicknetworkreply_p.cpp"
