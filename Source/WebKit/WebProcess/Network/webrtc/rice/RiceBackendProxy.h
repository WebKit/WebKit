/*
 *  Copyright (C) 2025 Igalia S.L. All rights reserved.
 *  Copyright (C) 2025 Metrological Group B.V.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#if USE(LIBRICE)

#include "MessageReceiver.h"
#include "MessageSender.h"
#include "SharedPreferencesForWebProcess.h"
#include "WebPageProxyIdentifier.h"
#include "WebProcess.h"
#include <WebCore/Document.h>
#include <WebCore/GStreamerIceAgent.h>
#include <WebCore/RTCIceComponent.h>
#include <WebCore/RTCIceConnectionState.h>
#include <WebCore/SharedMemory.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebKit {

struct RiceBackendIdentifierType { };

using RiceBackendIdentifier = ObjectIdentifier<RiceBackendIdentifierType>;

class RiceBackendProxy : public IPC::MessageSender, public IPC::MessageReceiver, public WebCore::RiceBackend, public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RiceBackendProxy, WTF::DestructionThread::MainRunLoop> {
public:
    static Ref<RiceBackendProxy> create(WebPageProxyIdentifier, WebCore::RiceBackendClient&);
    ~RiceBackendProxy();

    void ref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref(); }
    void deref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref(); }

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

private:
    RiceBackendProxy(Ref<IPC::Connection>&&, WebPageProxyIdentifier, RiceBackendIdentifier, WebCore::RiceBackendClient&);

    // RiceBackend (Web -> Network)
    void resolveAddress(const String&, WebCore::RiceBackend::ResolveAddressCallback&&) final;
    void send(unsigned, WebCore::RTCIceProtocol, String&&, String&&, WebCore::SharedMemory::Handle&&) final;
    Vector<String> gatherSocketAddresses(unsigned) final;
    void finalizeStream(unsigned) final;

    void refRiceBackend() final { ref(); }
    void derefRiceBackend() final { deref(); }

    // RiceBackendClient (Network -> Web)
    void notifyIncomingData(unsigned, WebCore::RTCIceProtocol, String&&, String&&, WebCore::SharedMemory::Handle&&);

    // MessageSender
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;

    const Ref<IPC::Connection> m_connection;
    WebPageProxyIdentifier m_webPageProxyID;
    RefPtr<WebCore::RiceBackendClient> m_client;
    const RiceBackendIdentifier m_identifier;
};

} // namespace WebKit

#endif // USE(LIBRICE)
