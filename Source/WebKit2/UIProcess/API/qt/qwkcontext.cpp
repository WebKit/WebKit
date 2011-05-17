/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
#include "qwkcontext.h"
#include "qwkcontext_p.h"

#include "ClientImpl.h"
#include "IconDatabase.h"
#include <QDir>
#include <QPixmap>
#include <QUrl>
#include "WebIconDatabase.h"
#include "WebPlatformStrategies.h"
#include "WKIconDatabase.h"

using namespace WebKit;

static inline void initializePlatformStrategiesIfNeeded()
{
    static bool initialized = false;
    if (initialized)
        return;

    WebPlatformStrategies::initialize();
    initialized = true;
}

QWKContextPrivate::QWKContextPrivate(QWKContext* qq, WebContext* webContext)
    : q(qq)
{
    if (!webContext)
        context = WebContext::create(String());
    else
        context = webContext;

    initializePlatformStrategiesIfNeeded();

    WKIconDatabaseClient iconDatabaseClient = {
        0,      // version
        qq,     // clientInfo
        qt_wk_didChangeIconForPageURL,
        qt_wk_didRemoveAllIcons
    };

    WKIconDatabaseSetIconDatabaseClient(toAPI(context->iconDatabase()), &iconDatabaseClient);
}

QWKContextPrivate::~QWKContextPrivate()
{ }

QWKContext::QWKContext(QObject* parent)
    : QObject(parent)
    , d(new QWKContextPrivate(this))
{ }

QWKContext::QWKContext(WKContextRef contextRef, QObject* parent)
    : QObject(parent)
    , d(new QWKContextPrivate(this, toImpl(contextRef)))
{ }

QWKContext::~QWKContext()
{
    delete d;
}

void QWKContext::setIconDatabasePath(const QString& path)
{
    // FIXME: There is currently no way to disable the icon database once it's enabled.
    QFileInfo info(path);
    if (info.isDir() && info.isWritable()) {
        QString databasePath = path + QDir::separator() + QString(WebCore::IconDatabase::defaultDatabaseFilename());
        d->context->setIconDatabasePath(databasePath);
    }
}

QIcon QWKContext::iconForPageURL(const QUrl& pageURL) const
{
    RefPtr<WebCore::Image> image = d->context->iconDatabase()->imageForPageURL(pageURL.toString());
    if (!image)
        return QIcon();

    QPixmap* nativeImage = image->nativeImageForCurrentFrame();
    if (!nativeImage)
        return QIcon();

    return QIcon(*nativeImage);
}

#include "moc_qwkcontext.cpp"
