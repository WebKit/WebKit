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
#include <wtf/linux/RealTimeThreads.h>

#include <sched.h>
#include <signal.h>
#include <string.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SafeStrerror.h>

#if USE(GLIB)
#include <gio/gio.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <wtf/Seconds.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/glib/Sandbox.h>
#endif

#ifndef SCHED_RESET_ON_FORK
#define SCHED_RESET_ON_FORK 0x40000000
#endif

namespace WTF {

static const int s_realTimeThreadPriority = 5;

RealTimeThreads& RealTimeThreads::singleton()
{
    static LazyNeverDestroyed<RealTimeThreads> realTimeThreads;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        realTimeThreads.construct();
    });
    return realTimeThreads;
}

RealTimeThreads::RealTimeThreads()
    : m_threadGroup(ThreadGroup::create())
#if USE(GLIB)
    , m_discardRealTimeKitProxyTimer(RunLoop::main(), this, &RealTimeThreads::discardRealTimeKitProxyTimerFired)
#endif
{
#if USE(GLIB)
    m_discardRealTimeKitProxyTimer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
#endif

    callOnMainThread([] {
        struct sigaction action;
        sigemptyset(&action.sa_mask);
        action.sa_sigaction = +[](int, siginfo_t*, void*) {
            // We don't know which thread caused the limit to be reached,
            // so we demote all real time threads to avoid SIGKILL.
            RealTimeThreads::singleton().demoteAllThreadsFromRealTime();
        };
        action.sa_flags = SA_SIGINFO;
        sigaction(SIGXCPU, &action, nullptr);
    });
}

void RealTimeThreads::registerThread(Thread& thread)
{
    ThreadGroupAddResult addResult;
    {
        Locker locker { m_threadGroup->getLock() };
        addResult = m_threadGroup->add(locker, thread);
    }

    if (addResult != ThreadGroupAddResult::NewlyAdded)
        return;

    callOnMainThread([this, thread = Ref { thread }] {
        if (m_enabled)
            promoteThreadToRealTime(thread);
    });
}

void RealTimeThreads::setEnabled(bool enabled)
{
    ASSERT(isMainThread());
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;

    Locker locker { m_threadGroup->getLock() };
    for (const auto& thread : m_threadGroup->threads(locker)) {
        if (m_enabled)
            promoteThreadToRealTime(thread);
        else
            demoteThreadFromRealTime(thread);
    }
}

void RealTimeThreads::promoteThreadToRealTime(const Thread& thread)
{
    ASSERT(isMainThread());

    struct sched_param param;
    param.sched_priority = std::clamp(s_realTimeThreadPriority, sched_get_priority_min(SCHED_RR), sched_get_priority_max(SCHED_RR));
    auto error = sched_setscheduler(thread.id(), SCHED_RR | SCHED_RESET_ON_FORK, &param);
    if (error) {
#if USE(GLIB)
        realTimeKitMakeThreadRealTime(getpid(), thread.id(), param.sched_priority);
#else
        LOG_ERROR("Failed to set thread %d as real time: %s", thread.id(), safeStrerror(error).data());
#endif
    }
}

void RealTimeThreads::demoteThreadFromRealTime(const Thread& thread)
{
    ASSERT(isMainThread());

    struct sched_param param = { 0 };
    sched_setscheduler(thread.id(), SCHED_OTHER | SCHED_RESET_ON_FORK, &param);
}

void RealTimeThreads::demoteAllThreadsFromRealTime()
{
    Locker locker { m_threadGroup->getLock() };
    for (const auto& thread : m_threadGroup->threads(locker))
        demoteThreadFromRealTime(thread);
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

void RealTimeThreads::realTimeKitMakeThreadRealTime(uint64_t processID, uint64_t threadID, uint32_t priority)
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

#ifdef RLIMIT_RTTIME
    // RealTimeKit requires the client to have RLIMIT_RTTIME set.
    struct rlimit rl;
    if (getrlimit(RLIMIT_RTTIME, &rl) >= 0) {
        auto rttimeMax = realTimeKitGetProperty(m_realTimeKitProxy->get(), "RTTimeUSecMax", &error.outPtr());
        if (error) {
            LOG_ERROR("Failed to get RTTimeUSecMax from RealtimeKit: %s", error->message);
            if (!g_error_matches(error.get(), G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_INTERFACE))
                m_realTimeKitProxy = nullptr;

            scheduleDiscardRealTimeKitProxy();
            return;
        }

        if (rl.rlim_max > static_cast<uint64_t>(rttimeMax)) {
            rl.rlim_cur = rl.rlim_max = rttimeMax;
            setrlimit(RLIMIT_RTTIME, &rl);
        }
    }
#endif

    GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_sync(m_realTimeKitProxy->get(), "MakeThreadRealtimeWithPID",
        g_variant_new("(ttu)", processID, threadID, priority), G_DBUS_CALL_FLAGS_NONE, s_dbusCallTimeout.millisecondsAs<int>(), nullptr, &error.outPtr()));
    if (!result) {
        LOG_ERROR("Failed to make thread real time: %s", error->message);
        if (!g_error_matches(error.get(), G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_INTERFACE))
            m_realTimeKitProxy = nullptr;
    }

    scheduleDiscardRealTimeKitProxy();
}

void RealTimeThreads::scheduleDiscardRealTimeKitProxy()
{
    if (!m_realTimeKitProxy || !m_realTimeKitProxy.value())
        return;
    m_discardRealTimeKitProxyTimer.startOneShot(30_s);
}

void RealTimeThreads::discardRealTimeKitProxyTimerFired()
{
    m_realTimeKitProxy = std::nullopt;
}
#endif // USE(GLIB)

} // namespace WTF
