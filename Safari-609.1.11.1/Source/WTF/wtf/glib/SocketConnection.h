/*
 * Copyright (C) 2019 Igalia, S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#pragma once

#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GSocketMonitor.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

typedef struct _GSocketConnection GSocketConnection;

namespace WTF {

class SocketConnection : public RefCounted<SocketConnection> {
public:
    typedef void (* MessageCallback)(SocketConnection&, GVariant*, gpointer);
    using MessageHandlers = HashMap<CString, std::pair<CString, MessageCallback>>;
    static Ref<SocketConnection> create(GRefPtr<GSocketConnection>&& connection, const MessageHandlers& messageHandlers, gpointer userData)
    {
        return adoptRef(*new SocketConnection(WTFMove(connection), messageHandlers, userData));
    }
    ~SocketConnection();

    bool isClosed() const { return !m_connection; }
    void sendMessage(const char*, GVariant*);
    void close();

private:
    SocketConnection(GRefPtr<GSocketConnection>&&, const MessageHandlers&, gpointer);

    bool read();
    bool readMessage();
    void write();
    void waitForSocketWritability();
    void didClose();

    GRefPtr<GSocketConnection> m_connection;
    const MessageHandlers& m_messageHandlers;
    gpointer m_userData;
    Vector<char> m_readBuffer;
    GSocketMonitor m_readMonitor;
    Vector<char> m_writeBuffer;
    GSocketMonitor m_writeMonitor;
};

} // namespace WTF

using WTF::SocketConnection;
