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
#include <wtf/URL.h>
#include <wtf/WorkQueue.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>

class WebKitTestServer {
    WTF_MAKE_FAST_ALLOCATED;
public:

    enum ServerOptions {
        ServerHTTPS = 0,
        ServerRunInThread = 1,
    };
    using ServerOptionsBitSet = std::bitset<2>;

    WebKitTestServer(ServerOptionsBitSet = 0);
    WebKitTestServer(ServerOptions option)
        : WebKitTestServer(ServerOptionsBitSet().set(option)) { }

    SoupServer* soupServer() const { return m_soupServer.get(); }
    const URL& baseURL() const { return m_baseURL; }
    unsigned port() const;
    CString getURIForPath(const char* path) const;
    void run(SoupServerCallback);

#if SOUP_CHECK_VERSION(2, 50, 0)
    void addWebSocketHandler(SoupServerWebsocketCallback, gpointer userData);
    void removeWebSocketHandler();
    const URL& baseWebSocketURL() const { return m_baseWebSocketURL; }
    CString getWebSocketURIForPath(const char* path) const;
#endif

private:
    GRefPtr<SoupServer> m_soupServer;
    URL m_baseURL;
#if SOUP_CHECK_VERSION(2, 50, 0)
    URL m_baseWebSocketURL;
#endif
    RefPtr<WorkQueue> m_queue;
};
