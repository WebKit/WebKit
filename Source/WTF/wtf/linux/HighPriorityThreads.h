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

#include <wtf/CanMakeWeakPtr.h>
#include <wtf/FastMalloc.h>
#include <wtf/ThreadGroup.h>

#if USE(GLIB)
#include <optional>
#include <wtf/RunLoop.h>
#include <wtf/glib/GRefPtr.h>

typedef struct _GDBusProxy GDBusProxy;
#endif

namespace WTF {

class HighPriorityThreads : public CanMakeWeakPtr<HighPriorityThreads> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(HighPriorityThreads);
    friend class LazyNeverDestroyed<HighPriorityThreads>;
public:
    WTF_EXPORT_PRIVATE static HighPriorityThreads& singleton();

    // Do nothing since this is a singleton.
    void ref() const { }
    void deref() const { }

    void registerThread(Thread&);

    WTF_EXPORT_PRIVATE void setEnabled(bool);

private:
    HighPriorityThreads();

    void applyState(const WTF::Thread&, Thread::SchedulingState);

#if USE(GLIB)
    void realTimeKitMakeThreadHighPriority(uint64_t processID, uint64_t threadID, int niceLevel);
    void scheduleDiscardRealTimeKitProxy();
    void discardRealTimeKitProxyTimerFired();
#endif

    Ref<ThreadGroup> m_threadGroup;
    // Every registered thread shares this state.
    Thread::SchedulingState m_state { Thread::SchedulingState::Full };
#if USE(GLIB)
    std::optional<GRefPtr<GDBusProxy>> m_realTimeKitProxy;
    std::optional<int> m_minNiceLevel; // min nice level we have permission to set.
    RunLoop::Timer m_discardRealTimeKitProxyTimer;
#endif
};

} // namespace WTF

using WTF::HighPriorityThreads;
