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

#include <wtf/RunLoop.h>

namespace WTF {

// Activity observers (used to implement WebCore::RunLoopObserver)
class ActivityObserver : public RefCounted<ActivityObserver> {
    WTF_MAKE_TZONE_ALLOCATED(ActivityObserver);
public:
    enum class ContinueObservation { Yes, No };
    using Callback = Function<ContinueObservation()>;

    static Ref<ActivityObserver> create(uint8_t order, OptionSet<RunLoop::Activity> activities, Callback&& callback)
    {
        return adoptRef(*new ActivityObserver(order, activities, WTFMove(callback)));
    }

    uint8_t order() const { return m_order; }
    OptionSet<RunLoop::Activity> activities() const { return m_activities; }

    ContinueObservation notify() const { return m_callback(); }

private:
    ActivityObserver(uint8_t order, OptionSet<RunLoop::Activity> activities, Callback&& callback)
        : m_order(order)
        , m_activities(activities)
        , m_callback(WTFMove(callback))
    {
        ASSERT(m_callback);
    }

private:
    uint8_t m_order { 0 };
    OptionSet<RunLoop::Activity> m_activities;
    Callback m_callback;
};

} // namespace WTF

using WTF::ActivityObserver;

#endif // USE(GLIB_EVENT_LOOP)
