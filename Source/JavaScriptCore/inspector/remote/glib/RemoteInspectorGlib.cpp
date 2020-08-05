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
#include "RemoteInspector.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "RemoteAutomationTarget.h"
#include "RemoteConnectionToTarget.h"
#include "RemoteInspectionTarget.h"
#include <gio/gio.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/GUniquePtr.h>

namespace Inspector {

RemoteInspector& RemoteInspector::singleton()
{
    static LazyNeverDestroyed<RemoteInspector> shared;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        shared.construct();
    });
    return shared;
}

RemoteInspector::RemoteInspector()
{
    if (g_getenv("WEBKIT_INSPECTOR_SERVER"))
        start();
}

void RemoteInspector::start()
{
    LockHolder lock(m_mutex);

    if (m_enabled)
        return;

    m_enabled = true;
    m_cancellable = adoptGRef(g_cancellable_new());

    GRefPtr<GSocketClient> socketClient = adoptGRef(g_socket_client_new());
    g_socket_client_connect_to_host_async(socketClient.get(), g_getenv("WEBKIT_INSPECTOR_SERVER"), 0, m_cancellable.get(),
        [](GObject* client, GAsyncResult* result, gpointer userData) {
            RemoteInspector* inspector = static_cast<RemoteInspector*>(userData);
            GUniqueOutPtr<GError> error;
            if (GRefPtr<GSocketConnection> connection = adoptGRef(g_socket_client_connect_to_host_finish(G_SOCKET_CLIENT(client), result, &error.outPtr())))
                inspector->setupConnection(SocketConnection::create(WTFMove(connection), messageHandlers(), inspector));
            else if (!g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("RemoteInspector failed to connect to inspector server at: %s: %s", g_getenv("WEBKIT_INSPECTOR_SERVER"), error->message);
        }, this);
}

void RemoteInspector::stopInternal(StopSource)
{
    if (!m_enabled)
        return;

    m_enabled = false;
    m_pushScheduled = false;
    g_cancellable_cancel(m_cancellable.get());
    m_cancellable = nullptr;

    for (auto targetConnection : m_targetConnectionMap.values())
        targetConnection->close();
    m_targetConnectionMap.clear();

    updateHasActiveDebugSession();

    m_automaticInspectionPaused = false;
    m_socketConnection = nullptr;
}

const SocketConnection::MessageHandlers& RemoteInspector::messageHandlers()
{
    static NeverDestroyed<const SocketConnection::MessageHandlers> messageHandlers = SocketConnection::MessageHandlers({
    { "DidClose", std::pair<CString, SocketConnection::MessageCallback> { { },
        [](SocketConnection&, GVariant*, gpointer userData) {
            auto& inspector = *static_cast<RemoteInspector*>(userData);
            inspector.stop();
        }}
    },
    { "GetTargetList", std::pair<CString, SocketConnection::MessageCallback> { { },
        [](SocketConnection&, GVariant*, gpointer userData) {
            auto& inspector = *static_cast<RemoteInspector*>(userData);
            inspector.receivedGetTargetListMessage();
        }}
    },
    { "Setup", std::pair<CString, SocketConnection::MessageCallback> { "(t)",
        [](SocketConnection&, GVariant* parameters, gpointer userData) {
            auto& inspector = *static_cast<RemoteInspector*>(userData);
            guint64 targetID;
            g_variant_get(parameters, "(t)", &targetID);
            inspector.receivedSetupMessage(targetID);
        }}
    },
    { "SendMessageToTarget", std::pair<CString, SocketConnection::MessageCallback> { "(ts)",
        [](SocketConnection&, GVariant* parameters, gpointer userData) {
            auto& inspector = *static_cast<RemoteInspector*>(userData);
            guint64 targetID;
            const char* message;
            g_variant_get(parameters, "(t&s)", &targetID, &message);
            inspector.receivedDataMessage(targetID, message);
        }}
    },
    { "FrontendDidClose", std::pair<CString, SocketConnection::MessageCallback> { "(t)",
        [](SocketConnection&, GVariant* parameters, gpointer userData) {
            auto& inspector = *static_cast<RemoteInspector*>(userData);
            guint64 targetID;
            g_variant_get(parameters, "(t)", &targetID);
            inspector.receivedCloseMessage(targetID);
        }}
    }
    });
    return messageHandlers;
}

void RemoteInspector::setupConnection(Ref<SocketConnection>&& connection)
{
    LockHolder lock(m_mutex);

    ASSERT(!m_socketConnection);
    m_socketConnection = WTFMove(connection);
    if (!m_targetMap.isEmpty())
        pushListingsSoon();
}

static const char* targetDebuggableType(RemoteInspectionTarget::Type type)
{
    switch (type) {
    case RemoteInspectionTarget::Type::JavaScript:
        return "JavaScript";
    case RemoteInspectionTarget::Type::ServiceWorker:
        return "ServiceWorker";
    case RemoteInspectionTarget::Type::WebPage:
        return "WebPage";
    default:
        break;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

TargetListing RemoteInspector::listingForInspectionTarget(const RemoteInspectionTarget& target) const
{
    if (!target.remoteDebuggingAllowed())
        return nullptr;

    return g_variant_new("(tsssb)", static_cast<guint64>(target.targetIdentifier()),
        targetDebuggableType(target.type()), target.name().utf8().data(),
        target.type() == RemoteInspectionTarget::Type::JavaScript ? "null" : target.url().utf8().data(),
        target.hasLocalDebugger());
}

TargetListing RemoteInspector::listingForAutomationTarget(const RemoteAutomationTarget& target) const
{
    return g_variant_new("(tsssb)", static_cast<guint64>(target.targetIdentifier()),
        "Automation", target.name().utf8().data(), "null", target.isPaired());
}

void RemoteInspector::pushListingsNow()
{
    if (!m_socketConnection)
        return;

    m_pushScheduled = false;

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("(a(tsssb)b)"));
    g_variant_builder_open(&builder, G_VARIANT_TYPE("a(tsssb)"));
    for (auto listing : m_targetListingMap.values())
        g_variant_builder_add_value(&builder, listing.get());
    g_variant_builder_close(&builder);
    g_variant_builder_add(&builder, "b", m_clientCapabilities && m_clientCapabilities->remoteAutomationAllowed);
    m_socketConnection->sendMessage("SetTargetList", g_variant_builder_end(&builder));
}

void RemoteInspector::pushListingsSoon()
{
    if (!m_socketConnection)
        return;

    if (m_pushScheduled)
        return;

    m_pushScheduled = true;

    RunLoop::current().dispatch([this] {
        LockHolder lock(m_mutex);
        if (m_pushScheduled)
            pushListingsNow();
    });
}

void RemoteInspector::sendAutomaticInspectionCandidateMessage()
{
    ASSERT(m_enabled);
    ASSERT(m_automaticInspectionEnabled);
    ASSERT(m_automaticInspectionPaused);
    ASSERT(m_automaticInspectionCandidateTargetIdentifier);
    ASSERT(m_socketConnection);
    // FIXME: Implement automatic inspection.
}

void RemoteInspector::sendMessageToRemote(TargetID targetIdentifier, const String& message)
{
    LockHolder lock(m_mutex);
    if (!m_socketConnection)
        return;

    m_socketConnection->sendMessage("SendMessageToFrontend", g_variant_new("(ts)", static_cast<guint64>(targetIdentifier), message.utf8().data()));
}

void RemoteInspector::receivedGetTargetListMessage()
{
    LockHolder lock(m_mutex);
    pushListingsNow();
}

void RemoteInspector::receivedSetupMessage(TargetID targetIdentifier)
{
    setup(targetIdentifier);
}

void RemoteInspector::receivedDataMessage(TargetID targetIdentifier, const char* message)
{
    RefPtr<RemoteConnectionToTarget> connectionToTarget;
    {
        LockHolder lock(m_mutex);
        connectionToTarget = m_targetConnectionMap.get(targetIdentifier);
        if (!connectionToTarget)
            return;
    }
    connectionToTarget->sendMessageToTarget(String::fromUTF8(message));
}

void RemoteInspector::receivedCloseMessage(TargetID targetIdentifier)
{
    RefPtr<RemoteConnectionToTarget> connectionToTarget;
    {
        LockHolder lock(m_mutex);
        RemoteControllableTarget* target = m_targetMap.get(targetIdentifier);
        if (!target)
            return;

        connectionToTarget = m_targetConnectionMap.take(targetIdentifier);
        updateHasActiveDebugSession();
    }

    if (connectionToTarget)
        connectionToTarget->close();
}

void RemoteInspector::setup(TargetID targetIdentifier)
{
    RemoteControllableTarget* target;
    {
        LockHolder lock(m_mutex);
        target = m_targetMap.get(targetIdentifier);
        if (!target)
            return;
    }

    auto connectionToTarget = adoptRef(*new RemoteConnectionToTarget(*target));
    ASSERT(is<RemoteInspectionTarget>(target) || is<RemoteAutomationTarget>(target));
    if (!connectionToTarget->setup()) {
        connectionToTarget->close();
        return;
    }

    LockHolder lock(m_mutex);
    m_targetConnectionMap.set(targetIdentifier, WTFMove(connectionToTarget));

    updateHasActiveDebugSession();
}

void RemoteInspector::sendMessageToTarget(TargetID targetIdentifier, const char* message)
{
    if (auto connectionToTarget = m_targetConnectionMap.get(targetIdentifier))
        connectionToTarget->sendMessageToTarget(String::fromUTF8(message));
}

void RemoteInspector::requestAutomationSession(const char* sessionID, const Client::SessionCapabilities& capabilities)
{
    if (!m_client)
        return;

    if (!m_clientCapabilities || !m_clientCapabilities->remoteAutomationAllowed)
        return;

    if (!sessionID || !sessionID[0])
        return;

    m_client->requestAutomationSession(String::fromUTF8(sessionID), capabilities);
    updateClientCapabilities();
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
