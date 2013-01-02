/*
 * Copyright (c) 2012 Hewlett-Packard Development Company, L.P.
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
#include "QtWebPageFindClient.h"

#include "qquickwebview_p_p.h"

using namespace WebCore;

namespace WebKit {

QtWebPageFindClient::QtWebPageFindClient(WKPageRef pageRef, QQuickWebView* webView)
    : m_webView(webView)
{
    WKPageFindClient findClient;
    memset(&findClient, 0, sizeof(WKPageFindClient));
    findClient.version = kWKPageFindClientCurrentVersion;
    findClient.clientInfo = this;
    findClient.didFindString = didFindString;
    findClient.didFailToFindString = didFailToFindString;
    WKPageSetPageFindClient(pageRef, &findClient);
}

void QtWebPageFindClient::didFindString(unsigned matchCount)
{
    m_webView->d_func()->didFindString(matchCount);
}

static QtWebPageFindClient* toQtWebPageFindClient(const void* clientInfo)
{
    ASSERT(clientInfo);
    return reinterpret_cast<QtWebPageFindClient*>(const_cast<void*>(clientInfo));
}

void QtWebPageFindClient::didFindString(WKPageRef page, WKStringRef string, unsigned matchCount, const void* clientInfo)
{
    toQtWebPageFindClient(clientInfo)->didFindString(matchCount);
}

void QtWebPageFindClient::didFailToFindString(WKPageRef page, WKStringRef string, const void* clientInfo)
{
    toQtWebPageFindClient(clientInfo)->didFindString(0);
}

} // namespace Webkit
