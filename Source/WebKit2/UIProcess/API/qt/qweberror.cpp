/*
 * Copyright (C) 2011 Andreas Kling <kling@webkit.org>
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
#include "qweberror.h"

#include "qweberror_p.h"
#include <QtCore/QUrl>
#include <WKSharedAPICast.h>
#include <WKString.h>
#include <WKStringQt.h>
#include <WKType.h>
#include <WKURL.h>
#include <WKURLQt.h>

using namespace WebKit;

QWebError::QWebError(QWebErrorPrivate* priv)
    : d(priv)
{
}

QWebError::QWebError(const QWebError& other)
    : d(other.d)
{
}

QWebError QWebErrorPrivate::createQWebError(WKErrorRef errorRef)
{
    return QWebError(new QWebErrorPrivate(errorRef));
}

QWebErrorPrivate::QWebErrorPrivate(WKErrorRef errorRef)
    : error(errorRef)
{
}

QWebErrorPrivate::~QWebErrorPrivate()
{
}

QWebError::Type QWebError::type() const
{
    WKRetainPtr<WKStringRef> errorDomainPtr = adoptWK(WKErrorCopyDomain(d->error.get()));
    WTF::String errorDomain = toWTFString(errorDomainPtr.get());

    if (errorDomain == "QtNetwork")
        return QWebError::NetworkError;
    if (errorDomain == "HTTP")
        return QWebError::HttpError;
    // FIXME: Redirection overflow currently puts the URL hostname in the errorDomain field.
    //        We should expose that error somehow. Source is QNetworkReplyHandler::redirect().
    return QWebError::EngineError;
}

int QWebError::errorCode() const
{
    return WKErrorGetErrorCode(d->error.get());
}

QUrl QWebError::url() const
{
    WKRetainPtr<WKURLRef> failingURL = adoptWK(WKErrorCopyFailingURL(d->error.get()));
    return WKURLCopyQUrl(failingURL.get());
}
