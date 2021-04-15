/*
 * Copyright (C) 2011 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <bitset>
#include <libsoup/soup.h>
#include <wtf/WorkQueue.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>

class WebKitTestServer {
    WTF_MAKE_FAST_ALLOCATED;
public:

    enum ServerOptions {
        ServerHTTPS = 0,
        ServerRunInThread = 1,
        ServerNonLoopback = 2,
    };
    using ServerOptionsBitSet = std::bitset<3>;

    WebKitTestServer(ServerOptionsBitSet = 0);
    virtual ~WebKitTestServer();

    SoupURI* baseURI() const { return m_baseURI; }
    CString getURIForPath(const char* path) const;
    void run(SoupServerCallback);

#if SOUP_CHECK_VERSION(2, 50, 0)
    void addWebSocketHandler(SoupServerWebsocketCallback, gpointer userData);
    void removeWebSocketHandler();
    SoupURI* baseWebSocketURI() const { return m_baseWebSocketURI; }
    CString getWebSocketURIForPath(const char* path) const;
#endif

private:
    GRefPtr<SoupServer> m_soupServer;
    SoupURI* m_baseURI { nullptr };
#if SOUP_CHECK_VERSION(2, 50, 0)
    SoupURI* m_baseWebSocketURI { nullptr };
#endif
    RefPtr<WorkQueue> m_queue;
};
