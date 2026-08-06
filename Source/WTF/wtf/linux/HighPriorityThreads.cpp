/*
 * Copyright (C) 2021 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/linux/HighPriorityThreads.h>

#include <unistd.h>
#include <wtf/Logging.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

#if USE(GLIB)
#include <gio/gio.h>
#include <wtf/Seconds.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/glib/Sandbox.h>
#endif

namespace WTF {

#if USE(GLIB)
// Requested nice value. rtkit clamps this to its own MinNiceLevel.
static constexpr int s_highPriorityNiceLevel = -20;
#endif

HighPriorityThreads& HighPriorityThreads::singleton()
{
    static LazyNeverDestroyed<HighPriorityThreads> highPriorityThreads;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        highPriorityThreads.construct();
    });
    return highPriorityThreads;
}

HighPriorityThreads::HighPriorityThreads()
    : m_threadGroup(ThreadGroup::create())
#if USE(GLIB)
    , m_discardRealTimeKitProxyTimer(RunLoop::mainSingleton(), "HighPriorityThreads::DiscardRealTimeKitProxyTimer"_s, this, &HighPriorityThreads::discardRealTimeKitProxyTimerFired)
#endif
{
#if USE(GLIB)
    m_discardRealTimeKitProxyTimer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
#endif
}

void HighPriorityThreads::registerThread(Thread& thread)
{
    RELEASE_ASSERT(thread.qos() == Thread::QOS::UserInteractive);

    {
        Locker locker { m_threadGroup->getLock() };
        // A thread registers once at startup
        if (m_threadGroup->add(locker, thread) != ThreadGroupAddResult::NewlyAdded) {
            ASSERT_NOT_REACHED();
            return;
        }
    }

    callOnMainThread([this, thread = Ref { thread }] {
        applyState(thread, m_state);
    });
}

void HighPriorityThreads::setEnabled(bool enabled)
{
    RELEASE_ASSERT(isMainThread());

    const auto state = enabled ? Thread::SchedulingState::Full : Thread::SchedulingState::Demoted;
    if (m_state == state)
        return;

    m_state = state;

    Locker locker { m_threadGroup->getLock() };
    for (const auto& thread : m_threadGroup->threads(locker))
        applyState(thread, m_state);
}

void HighPriorityThreads::applyState(const Thread& thread, Thread::SchedulingState state)
{
    thread.updateSchedulingAttributes(state);

#if USE(GLIB)
    // Lowering a nice level needs a privilege the process does not have, so rtkit sets it. Raising it
    // back needs none, so a demotion needs no rtkit call.
    if (state == Thread::SchedulingState::Full)
        realTimeKitMakeThreadHighPriority(getpid(), thread.id(), s_highPriorityNiceLevel);
#endif
}

#if USE(GLIB)
static const Seconds s_dbusCallTimeout = 20_ms;

static int64_t realTimeKitGetProperty(GDBusProxy* proxy, const char* propertyName, GError** error)
{
    const char* interfaceName = shouldUsePortal() ? "org.freedesktop.portal.Realtime" : "org.freedesktop.RealtimeKit1";
    GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_sync(proxy, "org.freedesktop.DBus.Properties.Get",
        g_variant_new("(ss)", interfaceName, propertyName), G_DBUS_CALL_FLAGS_NONE, s_dbusCallTimeout.millisecondsAs<int>(), nullptr, error));
    if (!result)
        return -1;

    GRefPtr<GVariant> property;
    g_variant_get(result.get(), "(v)", &property.outPtr());
    if (g_variant_is_of_type(property.get(), G_VARIANT_TYPE_INT64))
        return g_variant_get_int64(property.get());
    if (g_variant_is_of_type(property.get(), G_VARIANT_TYPE_INT32))
        return g_variant_get_int32(property.get());
    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "Invalid property type received for property %s at interface %s", propertyName, interfaceName);
    return -1;
}

void HighPriorityThreads::realTimeKitMakeThreadHighPriority(uint64_t processID, uint64_t threadID, int niceLevel)
{
    m_discardRealTimeKitProxyTimer.stop();

    GUniqueOutPtr<GError> error;
    if (!m_realTimeKitProxy) {
        if (shouldUsePortal()) {
            m_realTimeKitProxy = adoptGRef(g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
                static_cast<GDBusProxyFlags>(G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS | G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES), nullptr,
                "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.Realtime", nullptr, &error.outPtr()));
        } else {
            m_realTimeKitProxy = adoptGRef(g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                static_cast<GDBusProxyFlags>(G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS | G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES), nullptr,
                "org.freedesktop.RealtimeKit1", "/org/freedesktop/RealtimeKit1", "org.freedesktop.RealtimeKit1", nullptr, &error.outPtr()));
        }

        if (!m_realTimeKitProxy.value()) {
            LOG_ERROR("Failed to connect to RealtimeKit: %s", error->message);
            return;
        }
    }

    if (!m_realTimeKitProxy.value())
        return;

    // Clamp to what this rtkit grants to avoid refusal
    if (!m_minNiceLevel) {
        auto minNiceLevel = realTimeKitGetProperty(m_realTimeKitProxy->get(), "MinNiceLevel", &error.outPtr());
        if (error) {
            LOG_ERROR("Failed to get MinNiceLevel from RealtimeKit: %s", error->message);
            if (!g_error_matches(error.get(), G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_INTERFACE))
                m_realTimeKitProxy = nullptr;

            scheduleDiscardRealTimeKitProxy();
            return;
        }
        m_minNiceLevel = static_cast<int>(minNiceLevel);
    }
    niceLevel = std::max(niceLevel, *m_minNiceLevel);

    GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_sync(m_realTimeKitProxy->get(), "MakeThreadHighPriorityWithPID",
        g_variant_new("(tti)", processID, threadID, niceLevel), G_DBUS_CALL_FLAGS_NONE, s_dbusCallTimeout.millisecondsAs<int>(), nullptr, &error.outPtr()));
    if (!result) {
        // We use portal for sandboxed threads, as it takes care of mapping them.
        // However, this fails under certain containers (e.g. webkit-container-sdk).
        if (shouldUsePortal() && g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
            LOG_ERROR("Portal was unable to raise priority of sandboxed process p%" PRId64 ", t%" PRId64, processID, threadID);
        else
            LOG_ERROR("Failed to raise priority of thread p%" PRId64 ", t%" PRId64 ": %s", processID, threadID, error->message);
        if (!g_error_matches(error.get(), G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_INTERFACE))
            m_realTimeKitProxy = nullptr;
    }

    scheduleDiscardRealTimeKitProxy();
}

void HighPriorityThreads::scheduleDiscardRealTimeKitProxy()
{
    if (!m_realTimeKitProxy || !m_realTimeKitProxy.value())
        return;
    m_discardRealTimeKitProxyTimer.startOneShot(30_s);
}

void HighPriorityThreads::discardRealTimeKitProxyTimerFired()
{
    m_realTimeKitProxy = std::nullopt;
}
#endif // USE(GLIB)

} // namespace WTF
