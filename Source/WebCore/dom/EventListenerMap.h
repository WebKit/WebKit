/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
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
 *
 */

#pragma once

#include <WebCore/PlatformExportMacros.h>
#include <WebCore/RegisteredEventListener.h>
#include <atomic>
#include <limits>
#include <memory>
#include <wtf/Assertions.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/Compiler.h>
#include <wtf/CurrentThread.h>
#include <wtf/Forward.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/Platform.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>

#if PLATFORM(IOS_FAMILY)
#include <WebCore/WebCoreThread.h>
#endif

namespace WebCore {

class EventTarget;

using EventListenerVector = Vector<Ref<RegisteredEventListener>, 1, CrashOnOverflow, 2>;

class EventListenerMap {
public:
    WEBCORE_EXPORT EventListenerMap();

    bool isEmpty() const { assertIsOwnerThreadOrGCThreadWithWorldStopped(); return entries().isEmpty(); }
    bool contains(const AtomString& eventType) const { assertIsOwnerThreadOrGCThreadWithWorldStopped(); return find(eventType); }
    bool NODELETE containsCapturing(const AtomString& eventType) const;
    bool NODELETE containsActive(const AtomString& eventType) const;

    void clear();
    void clearEntriesForTearDown()
    {
        releaseAssertOrSetThreadUID();
        Locker locker { m_lock };
        entriesForMutation().clear();
    }

    void replacePreservingOptions(const AtomString& eventType, EventListener& oldListener, Ref<EventListener>&& newListener, bool useCapture = false);
    bool add(const AtomString& eventType, Ref<EventListener>&&, const RegisteredEventListener::Options&);
    bool remove(const AtomString& eventType, EventListener&, bool useCapture);
    // Returns const so that m_entries cannot be mutated through it while only shared access is
    // held. Mutating callers use findForMutation(), which requires the lock exclusively.
    WEBCORE_EXPORT const EventListenerVector* NODELETE find(const AtomString& eventType) const WTF_REQUIRES_SHARED_LOCK(m_lock);
    Vector<AtomString> eventTypes() const;

    template<typename CallbackType>
    void enumerateEventListenerTypes(NOESCAPE const CallbackType& callback) const
    {
        assertIsOwnerThread();
        for (auto& entry : entries()) {
            uint32_t capturingCount = 0;
            uint32_t bubblingCount = 0;
            for (auto& listener : entry.second) {
                if (listener->useCapture())
                    ++capturingCount;
                else
                    ++bubblingCount;
            }
            callback(entry.first, std::min<uint32_t>(capturingCount, std::numeric_limits<uint16_t>::max()), std::min<uint32_t>(bubblingCount, std::numeric_limits<uint16_t>::max()));
        }
    }

    template<typename CallbackType>
    bool containsMatchingEventListener(NOESCAPE const CallbackType& callback) const
    {
        assertIsOwnerThread();
        for (auto& entry : entries()) {
            if (callback(entry.first, entries()))
                return true;
        }
        return false;
    }

    void removeFirstEventListenerCreatedFromMarkup(const AtomString& eventType);
    void copyEventListenersNotCreatedFromMarkupToTarget(EventTarget*);
    
    template<typename Visitor> void visitJSEventListenersInGCThread(Visitor&);
    Lock& lock() LIFETIME_BOUND { return m_lock; }

    // Grants read-only access to m_entries on the thread that owns this map, without locking.
    // Public because EventTarget and Style::Adjuster reach m_entries through find(). Unlike
    // releaseAssertOrSetThreadUID() this never claims the map and costs nothing in release
    // builds, so it is usable on the event dispatch path.
    void assertIsOwnerThread() const WTF_ASSERTS_ACQUIRED_SHARED_LOCK(m_lock)
    {
#if PLATFORM(IOS_FAMILY)
        if (WebThreadIsEnabled())
            return;
#endif
        ASSERT_WITH_SECURITY_IMPLICATION(!m_threadUID || m_threadUID == currentThreadID());
    }

private:
    using Entries = Vector<std::pair<AtomString, EventListenerVector>, 0, CrashOnOverflow, 4>;

    // The only ways to reach the entries. Thread safety analysis counts a non-const call on a
    // guarded container as a read, so a mutating call would be accepted under merely shared
    // access; routing every mutation through an accessor that requires the lock exclusively is
    // what makes the write half of the invariant enforceable.
    const Entries& entries() const LIFETIME_BOUND WTF_REQUIRES_SHARED_LOCK(m_lock) { return m_entries; }
    Entries& entriesForMutation() LIFETIME_BOUND WTF_REQUIRES_LOCK(m_lock) { return m_entries; }

    EventListenerVector* NODELETE findForMutation(const AtomString& eventType) WTF_REQUIRES_LOCK(m_lock);

    // As above, but also permits a collector thread, and only while it has the world stopped:
    // that is what makes an unlocked read from a thread that does not own the map safe, since
    // the owning thread cannot be mutating. The hasPendingActivity() and
    // isReachableFromOpaqueRoots() implementations query the map from the GC in exactly that
    // state. Concurrent marking is not accepted, so an unlocked read from a GC thread while the
    // owning thread is running is still caught. Out of line to keep JSC's Heap out of this header.
#if ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    void assertIsOwnerThreadOrGCThreadWithWorldStopped() const WTF_ASSERTS_ACQUIRED_SHARED_LOCK(m_lock) { }
#else
    WEBCORE_EXPORT void assertIsOwnerThreadOrGCThreadWithWorldStopped() const WTF_ASSERTS_ACQUIRED_SHARED_LOCK(m_lock);
#endif

    void releaseAssertOrSetThreadUID()
    {
#if PLATFORM(IOS_FAMILY)
        if (WebThreadIsEnabled())
            return;
#endif
        if (!m_threadUID) {
            ASSERT(!currentThreadMayBeGCThread());
            m_threadUID = currentThreadID();
            return;
        }
        if (m_threadUID == currentThreadID()) [[likely]]
            return;
        RELEASE_ASSERT(currentThreadMayBeGCThread());
    }

    // Mutated on the owning thread while holding m_lock and read on the GC threads by
    // visitJSEventListenersInGCThread(), which locks; the owning thread's own reads use
    // assertIsOwnerThread() instead of locking. Note that thread safety analysis treats a
    // non-const call on a guarded container as a read, so keeping every mutable path behind
    // exclusive access is what actually enforces the write half of this.
    Entries m_entries WTF_GUARDED_BY_LOCK(m_lock);
    Lock m_lock;
    uint32_t m_threadUID { 0 };
};

template<typename Visitor>
void EventListenerMap::visitJSEventListenersInGCThread(Visitor& visitor)
{
    Locker locker { m_lock };
    for (auto& entry : entries()) {
        for (auto& eventListener : entry.second)
            eventListener->callback().visitJSFunctionInGCThread(visitor);
    }
}

} // namespace WebCore
