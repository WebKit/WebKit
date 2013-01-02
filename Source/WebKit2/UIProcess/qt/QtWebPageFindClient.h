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

#ifndef QtWebPageFindClient_h
#define QtWebPageFindClient_h

#include <WKPage.h>
#include <wtf/text/WTFString.h>

class QQuickWebView;

namespace WebKit {

class QtWebPageFindClient {
public:
    QtWebPageFindClient(WKPageRef, QQuickWebView*);

private:

    void didFindString(unsigned matchCount);

    // WKPageFindClient callbacks.
    static void didFindString(WKPageRef, WKStringRef, unsigned matchCount, const void* clientInfo);
    static void didFailToFindString(WKPageRef, WKStringRef, const void* clientInfo);

    QQuickWebView* m_webView;
};

} // namespace Webkit

#endif // QtWebPageFindClient_h
