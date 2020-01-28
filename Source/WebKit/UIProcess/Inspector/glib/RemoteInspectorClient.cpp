/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "RemoteInspectorClient.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "APIDebuggableInfo.h"
#include "RemoteWebInspectorProxy.h"
#include <JavaScriptCore/RemoteInspectorUtils.h>
#include <WebCore/InspectorDebuggableType.h>
#include <gio/gio.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/Base64.h>

namespace WebKit {

class RemoteInspectorProxy final : public RemoteWebInspectorProxyClient {
    WTF_MAKE_FAST_ALLOCATED();
public:
    RemoteInspectorProxy(RemoteInspectorClient& inspectorClient, uint64_t connectionID, uint64_t targetID)
        : m_proxy(RemoteWebInspectorProxy::create())
        , m_inspectorClient(inspectorClient)
        , m_connectionID(connectionID)
        , m_targetID(targetID)
    {
        m_proxy->setClient(this);
    }

    ~RemoteInspectorProxy()
    {
        m_proxy->setClient(nullptr);
        m_proxy->invalidate();
    }

    void load(Inspector::DebuggableType debuggableType)
    {
        // FIXME <https://webkit.org/b/205536>: this should infer more useful data about the debug target.
        Ref<API::DebuggableInfo> debuggableInfo = API::DebuggableInfo::create(DebuggableInfoData::empty());
        debuggableInfo->setDebuggableType(debuggableType);
        m_proxy->load(WTFMove(debuggableInfo), m_inspectorClient.backendCommandsURL());
    }

    void show()
    {
        m_proxy->show();
    }

    void setTargetName(const CString& name)
    {
#if PLATFORM(GTK)
        m_proxy->updateWindowTitle(name);
#endif
    }

    void sendMessageToFrontend(const String& message)
    {
        m_proxy->sendMessageToFrontend(message);
    }

    void sendMessageToBackend(const String& message) override
    {
        m_inspectorClient.sendMessageToBackend(m_connectionID, m_targetID, message);
    }

    void closeFromFrontend() override
    {
        m_inspectorClient.closeFromFrontend(m_connectionID, m_targetID);
    }

private:
    Ref<RemoteWebInspectorProxy> m_proxy;
    RemoteInspectorClient& m_inspectorClient;
    uint64_t m_connectionID;
    uint64_t m_targetID;
};

const SocketConnection::MessageHandlers& RemoteInspectorClient::messageHandlers()
{
    static NeverDestroyed<const SocketConnection::MessageHandlers> messageHandlers = SocketConnection::MessageHandlers({
    { "DidClose", std::pair<CString, SocketConnection::MessageCallback> { { },
        [](SocketConnection&, GVariant*, gpointer userData) {
            auto& client = *static_cast<RemoteInspectorClient*>(userData);
            client.connectionDidClose();
        }}
    },
    { "DidSetupInspectorClient", std::pair<CString, SocketConnection::MessageCallback> { "(ay)",
        [](SocketConnection&, GVariant* parameters, gpointer userData) {
            auto& client = *static_cast<RemoteInspectorClient*>(userData);
            GRefPtr<GVariant> backendCommandsVariant;
            g_variant_get(parameters, "(@ay)", &backendCommandsVariant.outPtr());
            client.setBackendCommands(g_variant_get_bytestring(backendCommandsVariant.get()));
        }}
    },
    { "SetTargetList", std::pair<CString, SocketConnection::MessageCallback> { "(ta(tsssb))",
        [](SocketConnection&, GVariant* parameters, gpointer userData) {
            auto& client = *static_cast<RemoteInspectorClient*>(userData);
            guint64 connectionID;
            GUniqueOutPtr<GVariantIter> iter;
            g_variant_get(parameters, "(ta(tsssb))", &connectionID, &iter.outPtr());
            size_t targetCount = g_variant_iter_n_children(iter.get());
            Vector<RemoteInspectorClient::Target> targetList;
            targetList.reserveInitialCapacity(targetCount);
            guint64 targetID;
            const char* type;
            const char* name;
            const char* url;
            gboolean hasLocalDebugger;
            while (g_variant_iter_loop(iter.get(), "(t&s&s&sb)", &targetID, &type, &name, &url, &hasLocalDebugger)) {
                if (!g_strcmp0(type, "JavaScript") || !g_strcmp0(type, "ServiceWorker") || !g_strcmp0(type, "WebPage"))
                    targetList.uncheckedAppend({ targetID, type, name, url });
            }
            client.setTargetList(connectionID, WTFMove(targetList));
        }}
    },
    { "SendMessageToFrontend", std::pair<CString, SocketConnection::MessageCallback> { "(tts)",
        [](SocketConnection&, GVariant* parameters, gpointer userData) {
            auto& client = *static_cast<RemoteInspectorClient*>(userData);
            guint64 connectionID, targetID;
            const char* message;
            g_variant_get(parameters, "(tt&s)", &connectionID, &targetID, &message);
            client.sendMessageToFrontend(connectionID, targetID, message);
        }}
    }
    });
    return messageHandlers;
}

RemoteInspectorClient::RemoteInspectorClient(const char* address, unsigned port, RemoteInspectorObserver& observer)
    : m_hostAndPort(String::fromUTF8(address) + ':' + String::number(port))
    , m_observer(observer)
    , m_cancellable(adoptGRef(g_cancellable_new()))
{
    GRefPtr<GSocketClient> socketClient = adoptGRef(g_socket_client_new());
    g_socket_client_connect_to_host_async(socketClient.get(), m_hostAndPort.utf8().data(), 0, m_cancellable.get(),
        [](GObject* object, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            GRefPtr<GSocketConnection> connection = adoptGRef(g_socket_client_connect_to_host_finish(G_SOCKET_CLIENT(object), result, &error.outPtr()));
            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                return;
            auto* client = static_cast<RemoteInspectorClient*>(userData);
            if (connection)
                client->setupConnection(SocketConnection::create(WTFMove(connection), messageHandlers(), client));
            else {
                WTFLogAlways("RemoteInspectorClient failed to connect to inspector server: %s", error->message);
                client->m_observer.connectionClosed(*client);
            }
    }, this);
}

RemoteInspectorClient::~RemoteInspectorClient()
{
    g_cancellable_cancel(m_cancellable.get());
}

void RemoteInspectorClient::setupConnection(Ref<SocketConnection>&& connection)
{
    m_socketConnection = WTFMove(connection);
    m_socketConnection->sendMessage("SetupInspectorClient", g_variant_new("(@ay)", g_variant_new_bytestring(Inspector::backendCommandsHash().data())));
}

void RemoteInspectorClient::setBackendCommands(const char* backendCommands)
{
    if (!backendCommands || !backendCommands[0]) {
        m_backendCommandsURL = String();
        return;
    }

    Vector<char> base64Data;
    base64Encode(backendCommands, strlen(backendCommands), base64Data);
    m_backendCommandsURL = "data:text/javascript;base64," + base64Data;
}

void RemoteInspectorClient::connectionDidClose()
{
    m_targets.clear();
    m_inspectorProxyMap.clear();
    m_socketConnection = nullptr;
    m_observer.connectionClosed(*this);
}

static Inspector::DebuggableType debuggableType(const String& targetType)
{
    if (targetType == "JavaScript")
        return Inspector::DebuggableType::JavaScript;
    if (targetType == "ServiceWorker")
        return Inspector::DebuggableType::ServiceWorker;
    if (targetType == "WebPage")
        return Inspector::DebuggableType::WebPage;
    RELEASE_ASSERT_NOT_REACHED();
}

void RemoteInspectorClient::inspect(uint64_t connectionID, uint64_t targetID, const String& targetType)
{
    auto addResult = m_inspectorProxyMap.ensure(std::make_pair(connectionID, targetID), [this, connectionID, targetID] {
        return makeUnique<RemoteInspectorProxy>(*this, connectionID, targetID);
    });
    if (!addResult.isNewEntry) {
        addResult.iterator->value->show();
        return;
    }

    m_socketConnection->sendMessage("Setup", g_variant_new("(tt)", connectionID, targetID));
    addResult.iterator->value->load(debuggableType(targetType));
}

void RemoteInspectorClient::sendMessageToBackend(uint64_t connectionID, uint64_t targetID, const String& message)
{
    m_socketConnection->sendMessage("SendMessageToBackend", g_variant_new("(tts)", connectionID, targetID, message.utf8().data()));
}

void RemoteInspectorClient::closeFromFrontend(uint64_t connectionID, uint64_t targetID)
{
    ASSERT(m_inspectorProxyMap.contains(std::make_pair(connectionID, targetID)));
    m_socketConnection->sendMessage("FrontendDidClose", g_variant_new("(tt)", connectionID, targetID));
    m_inspectorProxyMap.remove(std::make_pair(connectionID, targetID));
}

void RemoteInspectorClient::setTargetList(uint64_t connectionID, Vector<Target>&& targetList)
{
    // Find closed targets to remove them.
    Vector<uint64_t, 4> targetsToRemove;
    for (auto& connectionTargetPair : m_inspectorProxyMap.keys()) {
        if (connectionTargetPair.first != connectionID)
            continue;
        bool found = false;
        for (const auto& target : targetList) {
            if (target.id == connectionTargetPair.second) {
                m_inspectorProxyMap.get(connectionTargetPair)->setTargetName(target.name);
                found = true;
                break;
            }
        }
        if (!found)
            targetsToRemove.append(connectionTargetPair.second);
    }
    for (auto targetID : targetsToRemove)
        m_inspectorProxyMap.remove(std::make_pair(connectionID, targetID));

    m_targets.set(connectionID, WTFMove(targetList));
    m_observer.targetListChanged(*this);
}

void RemoteInspectorClient::sendMessageToFrontend(uint64_t connectionID, uint64_t targetID, const char* message)
{
    auto proxy = m_inspectorProxyMap.get(std::make_pair(connectionID, targetID));
    ASSERT(proxy);
    if (!proxy)
        return;
    proxy->sendMessageToFrontend(String::fromUTF8(message));
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
