/*
    Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)

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
#include "QtWebIconDatabaseClient.h"

#include "Image.h"
#include "KURL.h"
#include "QtWebContext.h"
#include "SharedBuffer.h"
#include "WKURLQt.h"
#include "WebContext.h"
#include "WebIconDatabase.h"
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtGui/QImage>

using namespace WebKit;

static inline QtWebIconDatabaseClient* toQtWebIconDatabaseClient(const void* clientInfo)
{
    ASSERT(clientInfo);
    return reinterpret_cast<QtWebIconDatabaseClient*>(const_cast<void*>(clientInfo));
}

QtWebIconDatabaseClient::QtWebIconDatabaseClient(QtWebContext *qtWebContext)
{
    m_contextId = qtWebContext->contextID();
    // The setter calls the getter here as it triggers the startup of the icon database.
    WebContext* context = qtWebContext->context();
    context->setIconDatabasePath(context->iconDatabasePath());
    m_iconDatabase = context->iconDatabase();

    WKIconDatabaseClient iconDatabaseClient;
    memset(&iconDatabaseClient, 0, sizeof(WKIconDatabaseClient));
    iconDatabaseClient.version = kWKIconDatabaseClientCurrentVersion;
    iconDatabaseClient.clientInfo = this;
    iconDatabaseClient.didChangeIconForPageURL = didChangeIconForPageURL;
    WKIconDatabaseSetIconDatabaseClient(toAPI(m_iconDatabase.get()), &iconDatabaseClient);
}

QtWebIconDatabaseClient::~QtWebIconDatabaseClient()
{
    WKIconDatabaseSetIconDatabaseClient(toAPI(m_iconDatabase.get()), 0);
}

void QtWebIconDatabaseClient::didChangeIconForPageURL(WKIconDatabaseRef iconDatabase, WKURLRef pageURL, const void* clientInfo)
{
    QUrl qUrl = WKURLCopyQUrl(pageURL);
    toQtWebIconDatabaseClient(clientInfo)->requestIconForPageURL(qUrl);
}

QImage QtWebIconDatabaseClient::iconImageForPageURL(const String& pageURL, const QSize& iconSize)
{
    MutexLocker locker(m_imageLock);

    WebCore::IntSize size(iconSize.width(), iconSize.height());
    RefPtr<WebCore::Image> image = m_iconDatabase->imageForPageURL(pageURL, size);
    if (!image)
        return QImage();

    QPixmap* nativeImage = image->nativeImageForCurrentFrame();
    if (!nativeImage)
        return QImage();

    return nativeImage->toImage();
}

unsigned QtWebIconDatabaseClient::iconURLHashForPageURL(const String& pageURL)
{
    String iconURL;
    m_iconDatabase->synchronousIconURLForPageURL(pageURL, iconURL);
    return StringHash::hash(iconURL);
}

void QtWebIconDatabaseClient::requestIconForPageURL(const QUrl& pageURL)
{
    String pageURLString = WebCore::KURL(pageURL).string();
    if (iconImageForPageURL(pageURLString).isNull())
        return;

    unsigned iconID = iconURLHashForPageURL(pageURLString);
    QUrl url;
    url.setScheme(QStringLiteral("image"));
    url.setHost(QStringLiteral("webicon"));
    QString path;
    path.append(QLatin1Char('/'));
    path.append(QString::number(m_contextId));
    path.append(QLatin1Char('/'));
    path.append(QString::number(iconID));
    url.setPath(path);
    url.setEncodedFragment(pageURL.toEncoded());
    emit iconChangedForPageURL(pageURL, url);
}

void QtWebIconDatabaseClient::retainIconForPageURL(const String& pageURL)
{
    m_iconDatabase->retainIconForPageURL(pageURL);
}

void QtWebIconDatabaseClient::releaseIconForPageURL(const String& pageURL)
{
    m_iconDatabase->releaseIconForPageURL(pageURL);
}

#include "moc_QtWebIconDatabaseClient.cpp"
