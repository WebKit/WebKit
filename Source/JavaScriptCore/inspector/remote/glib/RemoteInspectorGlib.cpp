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

#define REMOTE_INSPECTOR_DBUS_INTERFACE "org.webkit.RemoteInspector"
#define REMOTE_INSPECTOR_DBUS_OBJECT_PATH "/org/webkit/RemoteInspector"
#define INSPECTOR_DBUS_INTERFACE "org.webkit.Inspector"
#define INSPECTOR_DBUS_OBJECT_PATH "/org/webkit/Inspector"

namespace Inspector {

RemoteInspector& RemoteInspector::singleton()
{
    static NeverDestroyed<RemoteInspector> shared;
    return shared;
}

RemoteInspector::RemoteInspector()
{
    if (g_getenv("WEBKIT_INSPECTOR_SERVER"))
        start();
}

void RemoteInspector::start()
{
    std::lock_guard<Lock> lock(m_mutex);

    if (m_enabled)
        return;

    m_enabled = true;
    m_cancellable = adoptGRef(g_cancellable_new());

    GUniquePtr<char> inspectorAddress(g_strdup(g_getenv("WEBKIT_INSPECTOR_SERVER")));
    char* portPtr = g_strrstr(inspectorAddress.get(), ":");
    ASSERT(portPtr);
    *portPtr = '\0';
    portPtr++;
    unsigned port = g_ascii_strtoull(portPtr, nullptr, 10);
    GUniquePtr<char> dbusAddress(g_strdup_printf("tcp:host=%s,port=%u", inspectorAddress.get(), port));
    g_dbus_connection_new_for_address(dbusAddress.get(), G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT, nullptr, m_cancellable.get(),
        [](GObject*, GAsyncResult* result, gpointer userData) {
            RemoteInspector* inspector = static_cast<RemoteInspector*>(userData);
            GUniqueOutPtr<GError> error;
            if (GRefPtr<GDBusConnection> connection = adoptGRef(g_dbus_connection_new_for_address_finish(result, &error.outPtr())))
                inspector->setupConnection(WTFMove(connection));
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
    m_dbusConnection = nullptr;
}

static const char introspectionXML[] =
    "<node>"
    "  <interface name='" REMOTE_INSPECTOR_DBUS_INTERFACE "'>"
    "    <method name='GetTargetList'>"
    "    </method>"
    "    <method name='Setup'>"
    "      <arg type='t' name='target' direction='in'/>"
    "    </method>"
    "    <method name='SendMessageToTarget'>"
    "      <arg type='t' name='target' direction='in'/>"
    "      <arg type='s' name='message' direction='in'/>"
    "    </method>"
    "    <method name='FrontendDidClose'>"
    "      <arg type='t' name='target' direction='in'/>"
    "    </method>"
    "  </interface>"
    "</node>";

const GDBusInterfaceVTable RemoteInspector::s_interfaceVTable = {
    // method_call
    [] (GDBusConnection*, const gchar* /*sender*/, const gchar* /*objectPath*/, const gchar* /*interfaceName*/, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        auto* inspector = static_cast<RemoteInspector*>(userData);
        if (!g_strcmp0(methodName, "GetTargetList")) {
            inspector->receivedGetTargetListMessage();
            g_dbus_method_invocation_return_value(invocation, nullptr);
        } else if (!g_strcmp0(methodName, "Setup")) {
            guint64 targetID;
            g_variant_get(parameters, "(t)", &targetID);
            inspector->receivedSetupMessage(targetID);
            g_dbus_method_invocation_return_value(invocation, nullptr);
        } else if (!g_strcmp0(methodName, "SendMessageToTarget")) {
            guint64 targetID;
            const char* message;
            g_variant_get(parameters, "(t&s)", &targetID, &message);
            inspector->receivedDataMessage(targetID, message);
            g_dbus_method_invocation_return_value(invocation, nullptr);
        } else if (!g_strcmp0(methodName, "FrontendDidClose")) {
            guint64 targetID;
            g_variant_get(parameters, "(t)", &targetID);
            inspector->receivedCloseMessage(targetID);
            g_dbus_method_invocation_return_value(invocation, nullptr);
        }
    },
    // get_property
    nullptr,
    // set_property
    nullptr,
    // padding
    { 0 }
};

void RemoteInspector::setupConnection(GRefPtr<GDBusConnection>&& connection)
{
    std::lock_guard<Lock> lock(m_mutex);

    ASSERT(connection);
    ASSERT(!m_dbusConnection);
    m_dbusConnection = WTFMove(connection);

    static GDBusNodeInfo* introspectionData = nullptr;
    if (!introspectionData)
        introspectionData = g_dbus_node_info_new_for_xml(introspectionXML, nullptr);

    g_dbus_connection_register_object(m_dbusConnection.get(), REMOTE_INSPECTOR_DBUS_OBJECT_PATH,
        introspectionData->interfaces[0], &s_interfaceVTable, this, nullptr, nullptr);

    if (!m_targetMap.isEmpty())
        pushListingsSoon();
}

static void dbusConnectionCallAsyncReadyCallback(GObject* source, GAsyncResult* result, gpointer)
{
    GUniqueOutPtr<GError> error;
    GRefPtr<GVariant> resultVariant = adoptGRef(g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &error.outPtr()));
    if (!resultVariant && !g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning("RemoteInspector failed to send DBus message: %s", error->message);
}

TargetListing RemoteInspector::listingForInspectionTarget(const RemoteInspectionTarget& target) const
{
    if (!target.remoteDebuggingAllowed())
        return nullptr;

    // FIXME: Support remote debugging of a ServiceWorker.
    if (target.type() == RemoteInspectionTarget::Type::ServiceWorker)
        return nullptr;

    ASSERT(target.type() == RemoteInspectionTarget::Type::Web || target.type() == RemoteInspectionTarget::Type::JavaScript);
    return g_variant_new("(tsssb)", static_cast<guint64>(target.targetIdentifier()),
        target.type() == RemoteInspectionTarget::Type::Web ? "Web" : "JavaScript",
        target.name().utf8().data(), target.type() == RemoteInspectionTarget::Type::Web ? target.url().utf8().data() : "null",
        target.hasLocalDebugger());
}

TargetListing RemoteInspector::listingForAutomationTarget(const RemoteAutomationTarget& target) const
{
    return g_variant_new("(tsssb)", static_cast<guint64>(target.targetIdentifier()),
        "Automation", target.name().utf8().data(), "null", target.isPaired());
}

void RemoteInspector::pushListingsNow()
{
    if (!m_dbusConnection)
        return;

    m_pushScheduled = false;

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("(a(tsssb)b)"));
    g_variant_builder_open(&builder, G_VARIANT_TYPE("a(tsssb)"));
    for (auto listing : m_targetListingMap.values())
        g_variant_builder_add_value(&builder, listing.get());
    g_variant_builder_close(&builder);
    g_variant_builder_add(&builder, "b", m_clientCapabilities && m_clientCapabilities->remoteAutomationAllowed);
    g_dbus_connection_call(m_dbusConnection.get(), nullptr,
        INSPECTOR_DBUS_OBJECT_PATH, INSPECTOR_DBUS_INTERFACE, "SetTargetList",
        g_variant_builder_end(&builder), nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START,
        -1, m_cancellable.get(), dbusConnectionCallAsyncReadyCallback, nullptr);
}

void RemoteInspector::pushListingsSoon()
{
    if (!m_dbusConnection)
        return;

    if (m_pushScheduled)
        return;

    m_pushScheduled = true;

    RunLoop::current().dispatch([this] {
        std::lock_guard<Lock> lock(m_mutex);
        if (m_pushScheduled)
            pushListingsNow();
    });
}

void RemoteInspector::updateAutomaticInspectionCandidate(RemoteInspectionTarget* target)
{
    std::lock_guard<Lock> lock(m_mutex);

    ASSERT(target);
    unsigned targetIdentifier = target->targetIdentifier();
    if (!targetIdentifier)
        return;

    auto result = m_targetMap.set(targetIdentifier, target);
    ASSERT_UNUSED(result, !result.isNewEntry);

    // If the target has just allowed remote control, then the listing won't exist yet.
    // If the target has no identifier remove the old listing.
    if (auto targetListing = listingForTarget(*target))
        m_targetListingMap.set(targetIdentifier, targetListing);
    else
        m_targetListingMap.remove(targetIdentifier);
    // FIXME: Implement automatic inspection.
    pushListingsSoon();
}

void RemoteInspector::sendAutomaticInspectionCandidateMessage()
{
    ASSERT(m_enabled);
    ASSERT(m_automaticInspectionEnabled);
    ASSERT(m_automaticInspectionPaused);
    ASSERT(m_automaticInspectionCandidateTargetIdentifier);
    ASSERT(m_dbusConnection);
    // FIXME: Implement automatic inspection.
}

void RemoteInspector::sendMessageToRemote(unsigned targetIdentifier, const String& message)
{
    std::lock_guard<Lock> lock(m_mutex);
    if (!m_dbusConnection)
        return;

    g_dbus_connection_call(m_dbusConnection.get(), nullptr,
        INSPECTOR_DBUS_OBJECT_PATH, INSPECTOR_DBUS_INTERFACE, "SendMessageToFrontend",
        g_variant_new("(ts)", static_cast<guint64>(targetIdentifier), message.utf8().data()),
        nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START,
        -1, m_cancellable.get(), dbusConnectionCallAsyncReadyCallback, nullptr);
}

void RemoteInspector::receivedGetTargetListMessage()
{
    std::lock_guard<Lock> lock(m_mutex);
    pushListingsNow();
}

void RemoteInspector::receivedSetupMessage(unsigned targetIdentifier)
{
    setup(targetIdentifier);
}

void RemoteInspector::receivedDataMessage(unsigned targetIdentifier, const char* message)
{
    RefPtr<RemoteConnectionToTarget> connectionToTarget;
    {
        std::lock_guard<Lock> lock(m_mutex);
        connectionToTarget = m_targetConnectionMap.get(targetIdentifier);
        if (!connectionToTarget)
            return;
    }
    connectionToTarget->sendMessageToTarget(String::fromUTF8(message));
}

void RemoteInspector::receivedCloseMessage(unsigned targetIdentifier)
{
    RefPtr<RemoteConnectionToTarget> connectionToTarget;
    {
        std::lock_guard<Lock> lock(m_mutex);
        RemoteControllableTarget* target = m_targetMap.get(targetIdentifier);
        if (!target)
            return;

        connectionToTarget = m_targetConnectionMap.take(targetIdentifier);
        updateHasActiveDebugSession();
    }

    if (connectionToTarget)
        connectionToTarget->close();
}

void RemoteInspector::setup(unsigned targetIdentifier)
{
    RemoteControllableTarget* target;
    {
        std::lock_guard<Lock> lock(m_mutex);
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

    std::lock_guard<Lock> lock(m_mutex);
    m_targetConnectionMap.set(targetIdentifier, WTFMove(connectionToTarget));

    updateHasActiveDebugSession();
}

void RemoteInspector::sendMessageToTarget(unsigned targetIdentifier, const char* message)
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
