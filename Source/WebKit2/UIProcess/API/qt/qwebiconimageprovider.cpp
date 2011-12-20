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
#include "qwebiconimageprovider_p.h"

#include "QtWebContext.h"
#include "QtWebIconDatabaseClient.h"
#include <QtCore/QUrl>
#include <QtGui/QImage>
#include <wtf/text/WTFString.h>

using namespace WebKit;

QWebIconImageProvider::QWebIconImageProvider()
    : QDeclarativeImageProvider(QDeclarativeImageProvider::Image)
{
}

QWebIconImageProvider::~QWebIconImageProvider()
{
}

QImage QWebIconImageProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    RefPtr<QtWebContext> context = QtWebContext::defaultContext();
    QtWebIconDatabaseClient* iconDatabase = context->iconDatabase();

    QString encodedIconUrl = id;
    encodedIconUrl.remove(0, encodedIconUrl.indexOf('#') + 1);
    String pageURL = QUrl::fromPercentEncoding(encodedIconUrl.toUtf8());

    QImage icon = requestedSize.isValid() ? iconDatabase->iconImageForPageURL(pageURL, requestedSize) : iconDatabase->iconImageForPageURL(pageURL);
    ASSERT(!icon.isNull());

    if (size)
        *size = icon.size();

    return icon;
}
