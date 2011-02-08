/*
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CookieJar.h"

#include "Cookie.h"
#include "Document.h"
#include "FrameLoaderClientQt.h"
#include "KURL.h"
#include "NetworkingContext.h"
#include "PlatformString.h"
#include "QtNAMThreadSafeProxy.h"
#include "qwebframe.h"
#include "qwebpage.h"
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QStringList>

namespace WebCore {


static QNetworkAccessManager *networkAccessManager(const Document *document)
{
    if (!document)
        return 0;
    Frame* frame = document->frame();
    if (!frame)
        return 0;
    FrameLoader* loader = frame->loader();
    if (!loader)
        return 0;
    return loader->networkingContext()->networkAccessManager();
}

void setCookies(Document* document, const KURL& url, const String& value)
{
    QNetworkAccessManager* manager = networkAccessManager(document);
    if (!manager)
        return;

    // Create the manipulator on the heap to let it live until the
    // async request is picked by the other thread's event loop.
    QtNAMThreadSafeProxy* managerProxy = new QtNAMThreadSafeProxy(manager);
    managerProxy->setCookies(url, value);
    managerProxy->deleteLater();
}

String cookies(const Document* document, const KURL& url)
{
    QNetworkAccessManager* manager = networkAccessManager(document);
    if (!manager)
        return String();

    QtNAMThreadSafeProxy managerProxy(manager);
    QList<QNetworkCookie> cookies = managerProxy.cookiesForUrl(url);
    if (cookies.isEmpty())
        return String();

    QStringList resultCookies;
    foreach (QNetworkCookie networkCookie, cookies) {
        if (networkCookie.isHttpOnly())
            continue;
        resultCookies.append(QString::fromAscii(
                             networkCookie.toRawForm(QNetworkCookie::NameAndValueOnly).constData()));
    }

    return resultCookies.join(QLatin1String("; "));
}

String cookieRequestHeaderFieldValue(const Document* document, const KURL &url)
{
    QNetworkAccessManager* manager = networkAccessManager(document);
    if (!manager)
        return String();

    QtNAMThreadSafeProxy managerProxy(manager);
    QList<QNetworkCookie> cookies = managerProxy.cookiesForUrl(url);
    if (cookies.isEmpty())
        return String();

    QStringList resultCookies;
    foreach (QNetworkCookie networkCookie, cookies) {
        resultCookies.append(QString::fromAscii(
                             networkCookie.toRawForm(QNetworkCookie::NameAndValueOnly).constData()));
    }

    return resultCookies.join(QLatin1String("; "));
}

bool cookiesEnabled(const Document* document)
{
    QNetworkAccessManager* manager = networkAccessManager(document);
    if (!manager)
        return false;

    QtNAMThreadSafeProxy managerProxy(manager);
    return managerProxy.hasCookieJar();
}

bool getRawCookies(const Document*, const KURL&, Vector<Cookie>& rawCookies)
{
    // FIXME: Not yet implemented
    rawCookies.clear();
    return false; // return true when implemented
}

void deleteCookie(const Document*, const KURL&, const String&)
{
    // FIXME: Not yet implemented
}

}

// vim: ts=4 sw=4 et
