/*
 * Copyright (C) 2025 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if USE(GLIB_EVENT_LOOP)

#include <wtf/Lock.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WTF {

// Activity observers (used to implement WebCore::RunLoopObserver)
class ActivityObserver : public ThreadSafeRefCounted<ActivityObserver> {
    WTF_MAKE_TZONE_ALLOCATED(ActivityObserver);
public:
    using Callback = Function<void()>;

    static Ref<ActivityObserver> create(Ref<RunLoop>&& runLoop, bool isRepeating, uint8_t order, OptionSet<RunLoop::Activity> activities, Callback&& callback)
    {
        return adoptRef(*new ActivityObserver(WTFMove(runLoop), isRepeating, order, activities, WTFMove(callback)));
    }

    ~ActivityObserver()
    {
        ASSERT(!m_callback);
    }

    void start()
    {
        ASSERT(m_callback);
        if (auto runLoop = m_runLoop.get())
            runLoop->observeActivity(*this);
    }

    void stop()
    {
        {
            Locker lock { m_callbackLock };
            if (!m_callback)
                return;

            m_callback = nullptr;
        }

        if (auto runLoop = m_runLoop.get())
            runLoop->unobserveActivity(*this);
    }

    uint8_t order() const { return m_order; }
    OptionSet<RunLoop::Activity> activities() const { return m_activities; }

    void notify()
    {
        {
            Locker lock { m_callbackLock };
            if (!m_callback)
                return;
        }

        m_callback();

        if (!m_isRepeating)
            stop();
    }

private:
    ActivityObserver(Ref<RunLoop>&& runLoop, bool isRepeating, uint8_t order, OptionSet<RunLoop::Activity> activities, Callback&& callback)
        : m_runLoop(WTFMove(runLoop))
        , m_isRepeating(isRepeating)
        , m_order(order)
        , m_activities(activities)
        , m_callback(WTFMove(callback))
    {
        ASSERT(m_callback);
    }

private:
    ThreadSafeWeakPtr<RunLoop> m_runLoop;
    bool m_isRepeating { false };
    uint8_t m_order { 0 };
    OptionSet<RunLoop::Activity> m_activities;
    mutable Lock m_callbackLock;
    Callback m_callback WTF_GUARDED_BY_LOCK(m_callbackLock);
};

} // namespace WTF

using WTF::ActivityObserver;

#endif // USE(GLIB_EVENT_LOOP)
