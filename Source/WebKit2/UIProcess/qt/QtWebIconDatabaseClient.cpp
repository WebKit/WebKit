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
#include <WKContextPrivate.h>
#include <WKRetainPtr.h>
#include <WKStringQt.h>

namespace WebKit {

static inline QtWebIconDatabaseClient* toQtWebIconDatabaseClient(const void* clientInfo)
{
    ASSERT(clientInfo);
    return reinterpret_cast<QtWebIconDatabaseClient*>(const_cast<void*>(clientInfo));
}

QtWebIconDatabaseClient::QtWebIconDatabaseClient(WebContext *context)
{
    m_iconDatabase = context->iconDatabase();

    WKIconDatabaseClient iconDatabaseClient;
    memset(&iconDatabaseClient, 0, sizeof(WKIconDatabaseClient));
    iconDatabaseClient.version = kWKIconDatabaseClientCurrentVersion;
    iconDatabaseClient.clientInfo = this;
    iconDatabaseClient.didChangeIconForPageURL = didChangeIconForPageURL;
    WKIconDatabaseSetIconDatabaseClient(toAPI(m_iconDatabase.get()), &iconDatabaseClient);
    // Triggers the startup of the icon database.
    WKRetainPtr<WKStringRef> path = adoptWK(WKStringCreateWithQString(QtWebContext::preparedStoragePath(QtWebContext::IconDatabaseStorage)));
    WKContextSetIconDatabasePath(toAPI(context), path.get());
}

QtWebIconDatabaseClient::~QtWebIconDatabaseClient()
{
    m_iconDatabase->close();
    WKIconDatabaseSetIconDatabaseClient(toAPI(m_iconDatabase.get()), 0);
}

void QtWebIconDatabaseClient::didChangeIconForPageURL(WKIconDatabaseRef, WKURLRef pageURL, const void* clientInfo)
{
    emit toQtWebIconDatabaseClient(clientInfo)->iconChangedForPageURL(toImpl(pageURL)->string());
}

QUrl QtWebIconDatabaseClient::iconForPageURL(const QString& pageURL)
{
    String iconURL;
    m_iconDatabase->synchronousIconURLForPageURL(pageURL, iconURL);

    if (iconURL.isEmpty())
        return QUrl();

    // Verify that the image data is actually available before reporting back
    // a url, since clients assume that the url can be used directly.
    WebCore::Image* iconImage = m_iconDatabase->imageForPageURL(pageURL);
    if (!iconImage || iconImage->isNull())
        return QUrl();

    return QUrl(iconURL);
}

QImage QtWebIconDatabaseClient::iconImageForPageURL(const QString& pageURL, const QSize& iconSize)
{
    MutexLocker locker(m_imageLock);

    WebCore::IntSize size(iconSize.width(), iconSize.height());

    QPixmap* nativeImage = m_iconDatabase->nativeImageForPageURL(pageURL, size);
    if (!nativeImage)
        return QImage();

    return nativeImage->toImage();
}

void QtWebIconDatabaseClient::retainIconForPageURL(const QString& pageURL)
{
    m_iconDatabase->retainIconForPageURL(pageURL);
}

void QtWebIconDatabaseClient::releaseIconForPageURL(const QString& pageURL)
{
    m_iconDatabase->releaseIconForPageURL(pageURL);
}

} // namespace WebKit

#include "moc_QtWebIconDatabaseClient.cpp"

