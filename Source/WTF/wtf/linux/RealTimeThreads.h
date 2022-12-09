/*
 * Copyright (C) 2021 Igalia S.L.
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

#include <wtf/FastMalloc.h>
#include <wtf/ThreadGroup.h>

#if USE(GLIB)
#include <optional>
#include <wtf/RunLoop.h>
#include <wtf/glib/GRefPtr.h>

typedef struct _GDBusProxy GDBusProxy;
#endif

namespace WTF {

class RealTimeThreads {
    WTF_MAKE_FAST_ALLOCATED;
    friend class LazyNeverDestroyed<RealTimeThreads>;
public:
    WTF_EXPORT_PRIVATE static RealTimeThreads& singleton();

    void registerThread(Thread&);

    WTF_EXPORT_PRIVATE void setEnabled(bool);

private:
    RealTimeThreads();

    void promoteThreadToRealTime(const WTF::Thread&);
    void demoteThreadFromRealTime(const WTF::Thread&);
    void demoteAllThreadsFromRealTime();

#if USE(GLIB)
    void realTimeKitMakeThreadRealTime(uint64_t processID, uint64_t threadID, uint32_t priority);
    void scheduleDiscardRealTimeKitProxy();
    void discardRealTimeKitProxyTimerFired();
#endif

    Ref<ThreadGroup> m_threadGroup;
    bool m_enabled { true };
#if USE(GLIB)
    std::optional<GRefPtr<GDBusProxy>> m_realTimeKitProxy;
    RunLoop::Timer<RealTimeThreads> m_discardRealTimeKitProxyTimer;
#endif
};

} // namespace WTF

using WTF::RealTimeThreads;
