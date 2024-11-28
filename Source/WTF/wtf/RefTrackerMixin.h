/*
 * Copyright (C) 2024 Igalia S.L.
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

#include "Compiler.h"
#include "DataLog.h"
#include "HashMap.h"
#include "Lock.h"
#include "StackShot.h"
#include "StackTrace.h"

#include <atomic>

namespace WTF {

// This file contains some tools for tracking references.
// See Strong.h for an example of how to use it.

#if ENABLE(REFTRACKER)
template<typename T>
struct RefTrackerLoggingDisabledScope {
    WTF_MAKE_NONCOPYABLE(RefTrackerLoggingDisabledScope);
    RefTrackerLoggingDisabledScope()
    {
        ++T::refTrackerSingleton().loggingDisabledDepth;
    }
    ~RefTrackerLoggingDisabledScope()
    {
        --T::refTrackerSingleton().loggingDisabledDepth;
    }
};

struct RefTracker {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RefTracker() = default;
    ~RefTracker() = default;

    // NEVER_INLINE to make skipping frames more predictable.
    WTF_EXPORT_PRIVATE void reportLive(void*);
    WTF_EXPORT_PRIVATE void reportDead(void*);
    WTF_EXPORT_PRIVATE void logAllLiveReferences();

    Lock lock { };
    UncheckedKeyHashMap<void*, std::unique_ptr<StackShot>> map WTF_GUARDED_BY_LOCK(lock) { };
    std::atomic<int> loggingDisabledDepth { };
};

template<typename T>
struct RefTrackerMixin final {
    ALWAYS_INLINE RefTrackerMixin()
    {
        RELEASE_ASSERT(!originalThis);
        originalThis = this;
        if (UNLIKELY(T::enabled()))
            T::refTrackerSingleton().reportLive(static_cast<void*>(this));
    }

    ALWAYS_INLINE RefTrackerMixin(RefTrackerMixin<T>&&)
    {
        RELEASE_ASSERT(!originalThis);
        originalThis = this;
        if (UNLIKELY(T::enabled()))
            T::refTrackerSingleton().reportLive(static_cast<void*>(this));
    }

    ALWAYS_INLINE RefTrackerMixin(const RefTrackerMixin<T>&)
    {
        RELEASE_ASSERT(!originalThis);
        originalThis = this;
        if (UNLIKELY(T::enabled()))
            T::refTrackerSingleton().reportLive(static_cast<void*>(this));
    }

    ALWAYS_INLINE ~RefTrackerMixin()
    {
        RELEASE_ASSERT(originalThis == this);
        if (UNLIKELY(T::enabled()))
            T::refTrackerSingleton().reportDead(static_cast<void*>(this));
    }

    RefTrackerMixin& operator=(const RefTrackerMixin& o)
    {
        RELEASE_ASSERT(o.originalThis == &o);
        RELEASE_ASSERT(originalThis == this);
        return *this;
    }

    RefTrackerMixin& operator=(RefTrackerMixin&& o)
    {
        RELEASE_ASSERT(o.originalThis == &o);
        RELEASE_ASSERT(originalThis == this);
        return *this;
    }

    // This guards against seeing an unconstructed object (say, if we are zero-initialized)
    RefTrackerMixin* originalThis = nullptr;
};

#define REFTRACKER_DECL(T, initializer) \
    struct T final { \
        inline static bool enabled() { \
            initializer \
            return Options::enable ## T(); \
        } \
        WTF_EXPORT_PRIVATE static WTF::RefTracker& refTrackerSingleton(); \
    };

#define REFTRACKER_MEMBERS(T) \
    WTF::RefTrackerMixin<T> m_refTrackerData;

#define REFTRACKER_IMPL(T) \
    WTF::RefTracker& T::refTrackerSingleton() \
    { \
        static WTF::LazyNeverDestroyed<WTF::RefTracker> s_singleton; \
        static std::once_flag s_onceFlag; \
        std::call_once(s_onceFlag, \
            [] { \
                s_singleton.construct(); \
            }); \
        return s_singleton.get(); \
    }

#else // ENABLE(REFTRACKER)

#define REFTRACKER_DECL(_, initializer)
#define REFTRACKER_MEMBERS(_)
#define REFTRACKER_IMPL(_)

#endif

} // namespace WTF
