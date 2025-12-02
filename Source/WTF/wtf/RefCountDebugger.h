/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2013, 2025 Apple Inc. All rights reserved.
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
 *
 */

#pragma once

#include <wtf/Assertions.h>
#include <wtf/Compiler.h>
#include <wtf/FastMalloc.h>
#include <wtf/MainThread.h>
#include <wtf/Noncopyable.h>
#include <wtf/SwiftBridging.h>

namespace WTF {

#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
#define CHECK_REF_COUNTED_LIFECYCLE 1
#else
#define CHECK_REF_COUNTED_LIFECYCLE 0
#endif

// This base class holds debugging code, to share among refcounting subclasses.
class RefCountDebugger {
public:
    enum class RefCountIsThreadSafe : bool { No, Yes };

    WTF_EXPORT_PRIVATE static void logRefDuringDestruction(const void*);
    WTF_EXPORT_PRIVATE static void printRefDuringDestructionLogAndCrash (const void*) NO_RETURN_DUE_TO_CRASH;

    void willRef(unsigned refCount, RefCountIsThreadSafe isThreadSafe = RefCountIsThreadSafe::No) const
    {
        applyRefDerefThreadingCheck(refCount, isThreadSafe);
        applyRefDuringDestructionCheck();

#if CHECK_REF_COUNTED_LIFECYCLE
        ASSERT(!m_adoptionIsRequired);
#endif
    }

    void relaxAdoptionRequirement()
    {
#if CHECK_REF_COUNTED_LIFECYCLE
        ASSERT(m_adoptionIsRequired);
        m_adoptionIsRequired = false;
#endif
    }

    // Unsafe precondition: The caller must ensure thread-safe access to this object,
    // for example by using a mutex.
    void disableThreadingChecks()
    {
#if ASSERT_ENABLED
        m_areThreadingChecksEnabled = false;
#endif
    }

    static void enableThreadingChecksGlobally()
    {
#if ASSERT_ENABLED
        areThreadingChecksEnabledGlobally = true;
#endif
    }

protected:
    RefCountDebugger()
#if ASSERT_ENABLED
        : m_isOwnedByMainThread(isMainThread())
#endif
    {
    }

    void applyRefDuringDestructionCheck() const
    {
#if CHECK_REF_COUNTED_LIFECYCLE
        if (!m_deletionHasBegun)
            return;
        logRefDuringDestruction(this);
#endif
    }

    void applyRefDerefThreadingCheck(unsigned refCount, RefCountIsThreadSafe isThreadSafe = RefCountIsThreadSafe::No) const
    {
        UNUSED_PARAM(refCount);
        UNUSED_PARAM(isThreadSafe);

#if ASSERT_ENABLED
        if (isThreadSafe == RefCountIsThreadSafe::Yes)
            return;

        if (refCount == 1) {
            // Likely an ownership transfer across threads that may be safe.
            m_isOwnedByMainThread = isMainThread();
        } else if (areThreadingChecksEnabledGlobally && m_areThreadingChecksEnabled) {
            // If you hit this assertion, it means that the RefCounted object was ref/deref'd
            // from both the main thread and another in a way that is likely concurrent and unsafe.
            // Derive from ThreadSafeRefCounted and make sure the destructor is safe on threads
            // that call deref, or ref/deref from a single thread.
            ASSERT_WITH_MESSAGE(m_isOwnedByMainThread == isMainThread(), "Unsafe to ref/deref from different threads");
        }
#endif
    }

    ~RefCountDebugger()
    {
#if CHECK_REF_COUNTED_LIFECYCLE
        ASSERT(m_deletionHasBegun);
        ASSERT(!m_adoptionIsRequired);
#endif
    }

    void willDestroy(unsigned refCount) const
    {
        UNUSED_PARAM(refCount);
#if CHECK_REF_COUNTED_LIFECYCLE
        if (refCount == 1)
            return;
        printRefDuringDestructionLogAndCrash(this);
#endif
    }

    void willDelete() const
    {
#if CHECK_REF_COUNTED_LIFECYCLE
        m_deletionHasBegun = true;
#endif
    }

    void willDeref(unsigned refCount, RefCountIsThreadSafe isThreadSafe = RefCountIsThreadSafe::No) const
    {
        applyRefDerefThreadingCheck(refCount, isThreadSafe);

#if CHECK_REF_COUNTED_LIFECYCLE
        ASSERT(!m_adoptionIsRequired);
#endif

        ASSERT(refCount);
    }

#if CHECK_REF_COUNTED_LIFECYCLE
    bool deletionHasBegun() const
    {
        return m_deletionHasBegun;
    }
#endif

private:

#if CHECK_REF_COUNTED_LIFECYCLE
    friend void adopted(RefCountDebugger*);
#endif

#if ASSERT_ENABLED
    mutable bool m_isOwnedByMainThread;
    bool m_areThreadingChecksEnabled { true };
#endif
    WTF_EXPORT_PRIVATE static bool areThreadingChecksEnabledGlobally;
#if CHECK_REF_COUNTED_LIFECYCLE
    mutable std::atomic<bool> m_deletionHasBegun { false };
    mutable bool m_adoptionIsRequired { true };
#endif
};

#if CHECK_REF_COUNTED_LIFECYCLE
inline void adopted(RefCountDebugger* object)
{
    if (!object)
        return;
    object->m_adoptionIsRequired = false;
}
#endif

} // namespace WTF
