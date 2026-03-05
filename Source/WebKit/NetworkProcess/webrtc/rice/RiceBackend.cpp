/*
 *  Copyright (C) 2025 Igalia S.L. All rights reserved.
 *  Copyright (C) 2025 Metrological Group B.V.
 *  Copyright (C) 2024 Matthew Waters <matthew@centricular.com>
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

#include "config.h"
#include "RiceBackend.h"

#if USE(LIBRICE)

#include "NetworkConnectionToWebProcess.h"
#include "RiceBackendProxyMessages.h"

#include <WebCore/RiceUtilities.h>
#include <rice-io.h>
#include <wtf/CompletionHandler.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
using namespace WebCore;

struct RecvSource {
    GSource source;
    Atomic<bool> needsDispatch;
};

static gboolean recvSourcePrepare(GSource* base, gint* timeout)
{
    auto source = reinterpret_cast<RecvSource*>(base);
    *timeout = -1;
    return source->needsDispatch.loadRelaxed();
}

static gboolean recvSourceCheck(GSource* base)
{
    auto source = reinterpret_cast<RecvSource*>(base);
    return source->needsDispatch.loadRelaxed();
}

static gboolean recvSourceDispatch(GSource* base, GSourceFunc callback, gpointer data)
{
    auto source = reinterpret_cast<RecvSource*>(base);

    // This needs to be before the callback to ensure that any later recvSourceWakeup() either in
    // the callback, or just after it can cause another wakeup to occur.
    source->needsDispatch.exchange(false);

    if (callback)
        callback(data);

    return G_SOURCE_CONTINUE;
}

static void recvSourceWakeup(GSource* base)
{
    auto source = reinterpret_cast<RecvSource*>(base);
    auto context = g_source_get_context(base);
    source->needsDispatch.exchange(true);

    if (context)
        g_main_context_wakeup(context);
}

static void recvSourceFinalize(GSource*)
{
}

static GSourceFuncs recvSourceEventFunctions = {
    recvSourcePrepare,
    recvSourceCheck,
    recvSourceDispatch,
    recvSourceFinalize,
    nullptr, nullptr
};

static GRefPtr<GSource> recvSourceNew()
{
    auto source = adoptGRef(g_source_new(&recvSourceEventFunctions, sizeof(RecvSource)));
    g_source_set_priority(source.get(), RunLoopSourcePriority::AsyncIONetwork);
    g_source_set_name(source.get(), "[WebKit] ICE Agent recv loop");

    auto recvSource = reinterpret_cast<RecvSource*>(source.get());
    recvSource->needsDispatch.exchange(true);

    return source;
}

struct RecvSourceData {
    ThreadSafeWeakPtr<RiceBackend> backend;
    unsigned streamId;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(RecvSourceData);

WTF_MAKE_TZONE_ALLOCATED_IMPL(RiceBackend);

void RiceBackend::initialize(NetworkConnectionToWebProcess& connectionToWebProcess, WebKit::WebPageProxyIdentifier&&, CompletionHandler<void(RefPtr<RiceBackend>&&)>&& completionHandler)
{
    Ref backend = RiceBackend::create(connectionToWebProcess);
    completionHandler(WTF::move(backend));
}

RiceBackend::RiceBackend(NetworkConnectionToWebProcess& connection)
    : m_connection(connection)
{
    static Atomic<uint32_t> counter = 0;
    auto id = counter.load();
    auto threadName = makeString("webrtc-rice-"_s, id);
    counter.exchangeAdd(1);

    m_runLoop = RunLoop::create(ASCIILiteral::fromLiteralUnsafe(threadName.ascii().data()));
}

RiceBackend::~RiceBackend() = default;

IPC::Connection* RiceBackend::messageSenderConnection() const
{
    return m_connection ? &m_connection->connection() : nullptr;
}

uint64_t RiceBackend::messageSenderDestinationID() const
{
    return identifier().toUInt64();
}

std::optional<SharedPreferencesForWebProcess> RiceBackend::sharedPreferencesForWebProcess() const
{
    if (auto connectionToWebProcess = m_connection.get())
        return connectionToWebProcess->sharedPreferencesForWebProcess();

    return std::nullopt;
}

GRefPtr<RiceSockets> RiceBackend::getSocketsForStream(unsigned streamId)
{
    return m_sockets.get(streamId).sockets;
}

GRefPtr<GSource> RiceBackend::getRecvSourceForStream(unsigned streamId)
{
    return m_sockets.get(streamId).source;
}

void RiceBackend::notifyIncomingData(unsigned streamId, WebCore::RTCIceProtocol protocol, String&& from, String&& to, WebCore::SharedMemory::Handle&& data)
{
    callOnMainRunLoopAndWait([&, streamId, protocol, data = WTF::move(data), from = WTF::move(from), to = WTF::move(to)] mutable {
        if (RefPtr connection = messageSenderConnection())
            connection->send(Messages::RiceBackendProxy::NotifyIncomingData { streamId, protocol, from, to, WTF::move(data) }, messageSenderDestinationID());
    });
}

struct ResolveAddressData {
    GRefPtr<GResolver> resolver;
    String address;
    RiceBackend::ResolveCallback callback;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(ResolveAddressData);

struct ResolveAddressDataInner {
    RiceBackend::ResolveCallback callback;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(ResolveAddressDataInner);

void RiceBackend::resolveAddress(const String& address, CompletionHandler<void(Expected<String, WebCore::ExceptionData>&&)>&& completionHandler)
{
    auto data = createResolveAddressData();
    data->resolver = adoptGRef(g_resolver_get_default());
    data->address = address;
    data->callback = WTF::move(completionHandler);
    g_main_context_invoke_full(m_runLoop->mainContext(), G_PRIORITY_DEFAULT, reinterpret_cast<GSourceFunc>(+[](gpointer userData) -> gboolean {
        auto data = reinterpret_cast<ResolveAddressData*>(userData);
        auto innerData = createResolveAddressDataInner();
        innerData->callback = WTF::move(data->callback);
        g_resolver_lookup_by_name_async(data->resolver.get(), data->address.utf8().data(), nullptr,
            reinterpret_cast<GAsyncReadyCallback>(+[](GResolver* resolver, GAsyncResult* result, gpointer userData) {
                auto data = reinterpret_cast<ResolveAddressDataInner*>(userData);
                GUniqueOutPtr<GError> error;
                GList* addresses = g_resolver_lookup_by_name_finish(resolver, result, &error.outPtr());
                if (!addresses) {
                    auto message = makeString("Unable to resolve address: "_s, String::fromUTF8(error->message));
                    callOnMainRunLoopAndWait([data, message = WTF::move(message)] {
                        data->callback(makeUnexpected(ExceptionData { ExceptionCode::NetworkError, message }));
                    });
                    destroyResolveAddressDataInner(data);
                    return;
                }

                GUniquePtr<char> address(g_inet_address_to_string(G_INET_ADDRESS(addresses->data)));
                callOnMainRunLoopAndWait([data, address = WTF::move(address)] {
                    data->callback(String::fromUTF8(address.get()));
                });

                g_resolver_free_addresses(addresses);
                destroyResolveAddressDataInner(data);
            }), innerData);
        return G_SOURCE_REMOVE;
    }), data, reinterpret_cast<GDestroyNotify>(destroyResolveAddressData));
}

void RiceBackend::sendData(unsigned streamId, WebCore::RTCIceProtocol protocol, String from, String to, WebCore::SharedMemory::Handle&& handle)
{
    auto sockets = getSocketsForStream(streamId);
    if (!sockets) [[unlikely]]
        return;

    const auto riceFrom = ensureRiceAddressFromCache(from);
    const auto riceTo = ensureRiceAddressFromCache(to);

    RiceTransportType transport;
    switch (protocol) {
    case RTCIceProtocol::Udp:
        transport = RICE_TRANSPORT_TYPE_UDP;
        break;
    case RTCIceProtocol::Tcp:
        transport = RICE_TRANSPORT_TYPE_TCP;
        break;
    };

    auto sharedMemory = SharedMemory::map(WTF::move(handle), SharedMemory::Protection::ReadOnly);
    if (!sharedMemory)
        return;

    auto buffer = sharedMemory->createSharedBuffer(sharedMemory->size());
    auto result = rice_sockets_send(sockets.get(), transport, riceFrom, riceTo, buffer->span().data(), buffer->size());
    if (result != RICE_ERROR_SUCCESS)
        g_printerr("Failed to send data to rice sockets, error code: %d\n", static_cast<uint32_t>(result));
}

void RiceBackend::finalizeStream(unsigned streamId)
{
    m_udpAddresses.removeIf([&](auto& keyValue) -> bool {
        auto& [key, addresses] = keyValue;
        if (key != streamId)
            return false;

        auto riceSockets = getSocketsForStream(streamId);
        for (auto& udpAddress : addresses)
            rice_sockets_remove_udp(riceSockets.get(), udpAddress.get());

        return true;
    });
    auto data = m_sockets.take(streamId);
    if (data.source)
        g_source_destroy(data.source.get());
}

void RiceBackend::gatherSocketAddresses(ScriptExecutionContextIdentifier identifier, unsigned streamId, GatherSocketAddressesCallback&& completionHandler)
{
    HashMap<std::pair<String, RTCIceProtocol>, String> result;

    auto recvData2 = createRecvSourceData();
    recvData2->backend = this;
    recvData2->streamId = streamId;
    auto sockets = adoptGRef(rice_sockets_new_with_notify([](auto userData) {
        auto recvData = reinterpret_cast<RecvSourceData*>(userData);
        RefPtr backend = recvData->backend.get();
        if (!backend)
            return;
        if (auto recvSource = backend->getRecvSourceForStream(recvData->streamId))
            recvSourceWakeup(recvSource.get());
    }, recvData2, reinterpret_cast<GDestroyNotify>(destroyRecvSourceData)));

    Vector<GUniquePtr<RiceAddress>> udpAddresses;

    size_t totalInterfaces = 0;
    auto interfaces = rice_interfaces(&totalInterfaces);
    std::span<RiceAddress*> interfaceAddresses = WTF::unsafeMakeSpan(interfaces, totalInterfaces);

    for (size_t i = 0; i < totalInterfaces; i++) {
        auto ipAddress = riceAddressToString(interfaceAddresses[i], false);
        // mDNS address, not supported yet.
        auto name = emptyString();
        if (auto socket = rice_udp_socket_new(interfaceAddresses[i])) {
            GUniquePtr<RiceAddress> localAddress(rice_udp_socket_local_addr(socket));

            result.add({ riceAddressToString(localAddress.get()).convertToLowercaseWithoutLocale(), WebCore::RTCIceProtocol::Udp }, name);
            udpAddresses.append(WTF::move(localAddress));
            rice_sockets_add_udp(sockets.get(), socket);
        }

        auto recvData = createRecvSourceData();
        recvData->backend = this;
        recvData->streamId = streamId;
        auto tcpListener = adoptGRef(rice_tcp_listen(interfaceAddresses[i], [](RiceTcpSocket* socket, void* userData) {
            auto recvData = reinterpret_cast<RecvSourceData*>(userData);
            RefPtr backend = recvData->backend.get();
            if (!backend)
                return;
            auto sockets = backend->getSocketsForStream(recvData->streamId);
            rice_sockets_add_tcp(sockets.get(), socket);
        }, recvData, reinterpret_cast<RiceIoDestroy>(destroyRecvSourceData)));

        GUniquePtr<RiceAddress> tcpListenerLocalAddress(rice_tcp_listener_local_addr(tcpListener.get()));
        result.add({ riceAddressToString(tcpListenerLocalAddress.get()).convertToLowercaseWithoutLocale(), WebCore::RTCIceProtocol::Tcp }, name);
        m_tcpListeners.append(WTF::move(tcpListener));
    }

    rice_addresses_free(interfaces, totalInterfaces);

    auto source = recvSourceNew();
    auto recvData = createRecvSourceData();
    recvData->backend = this;
    recvData->streamId = streamId;

    g_source_set_callback(source.get(), static_cast<GSourceFunc>([](auto userData) -> gboolean {
        RiceIoRecv recv;
        uint8_t data[16384];

        auto sourceData = reinterpret_cast<RecvSourceData*>(userData);
        RefPtr backend = sourceData->backend.get();
        if (!backend)
            return G_SOURCE_REMOVE;
        auto sockets = backend->getSocketsForStream(sourceData->streamId);
        if (!sockets)
            return G_SOURCE_CONTINUE;
        while (true) {
            rice_sockets_recv(sockets.get(), data, 16384, &recv);
            switch (recv.tag) {
            case RICE_IO_RECV_WOULD_BLOCK:
                rice_recv_clear(&recv);
                return G_SOURCE_CONTINUE;
            case RICE_IO_RECV_DATA: {
                auto from = riceAddressToString(recv.data.from);
                auto to = riceAddressToString(recv.data.to);
                RTCIceProtocol protocol;
                switch (recv.data.transport) {
                case RICE_TRANSPORT_TYPE_UDP:
                    protocol = RTCIceProtocol::Udp;
                    break;
                case RICE_TRANSPORT_TYPE_TCP:
                    protocol = RTCIceProtocol::Tcp;
                    break;
                };
                auto handle = SharedMemoryHandle::createCopy(unsafeMakeSpan(data, recv.data.len), SharedMemoryProtection::ReadOnly);
                if (!handle) [[unlikely]]
                    break;
                backend->notifyIncomingData(sourceData->streamId, protocol, WTF::move(from), WTF::move(to), WTF::move(*handle));
                break;
            }
            case RICE_IO_RECV_CLOSED:
                // TODO
                break;
            }
            rice_recv_clear(&recv);
        }

        return G_SOURCE_CONTINUE;
    }), recvData, reinterpret_cast<GDestroyNotify>(destroyRecvSourceData));

    g_source_attach(source.get(), m_runLoop->mainContext());
    m_sockets.add(streamId, SocketData { WTF::move(sockets), WTF::move(source) });
    m_udpAddresses.add(streamId, WTF::move(udpAddresses));

    completionHandler(WTF::move(result));
}

const RiceAddress* RiceBackend::ensureRiceAddressFromCache(const String& address)
{
    auto& result = m_addressCache.ensure(address, [address] {
        return riceAddressFromString(address);
    }).iterator->value;
    return result.get();
}

void RiceBackend::setSocketTypeOfService(unsigned streamId, unsigned value)
{
#if RICE_CHECK_VERSION(0, 2, 2)
    auto sockets = getSocketsForStream(streamId);
    if (!sockets) [[unlikely]]
        return;

    rice_sockets_set_tos(sockets.get(), value);
#else
    UNUSED_PARAM(streamId);
    UNUSED_PARAM(value);
#endif
}

} // namespace WebKit

#endif // USE(LIBRICE)
