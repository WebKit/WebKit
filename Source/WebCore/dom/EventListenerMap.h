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
#include <wtf/Forward.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/Platform.h>
#include <wtf/ThreadAssertions.h>
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

    bool isEmpty() const;
    bool contains(const AtomString& eventType) const { return find(eventType); }
    bool NODELETE containsCapturing(const AtomString& eventType) const;
    bool NODELETE containsActive(const AtomString& eventType) const;

    void clear();
    void clearEntriesForTearDown()
    {
        releaseAssertOrSetThreadUID();
        Locker locker { m_lock };
        m_entries.clear();
    }

    void replacePreservingOptions(const AtomString& eventType, EventListener& oldListener, Ref<EventListener>&& newListener, bool useCapture = false);
    bool add(const AtomString& eventType, Ref<EventListener>&&, const RegisteredEventListener::Options&);
    bool remove(const AtomString& eventType, EventListener&, bool useCapture);
    // Returns a pointer into m_entries, so it must be const: handing out a mutable pointer would let
    // callers write to guarded state without thread safety analysis being able to check them.
    WEBCORE_EXPORT const EventListenerVector* NODELETE find(const AtomString& eventType) const;
    Vector<AtomString> eventTypes() const;

    template<typename CallbackType>
    void enumerateEventListenerTypes(NOESCAPE const CallbackType& callback) const
    {
        assertIsOwnerThreadForReading();
        for (auto& entry : m_entries) {
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
        assertIsOwnerThreadForReading();
        for (auto& entry : m_entries) {
            if (callback(entry.first, m_entries))
                return true;
        }
        return false;
    }

    void removeFirstEventListenerCreatedFromMarkup(const AtomString& eventType);
    void copyEventListenersNotCreatedFromMarkupToTarget(EventTarget*);
    
    template<typename Visitor> void visitJSEventListenersInGCThread(Visitor&);

private:
    // 294937@main disabled these assertions when the web thread is enabled, after the release
    // assertion failed in the field: on USE(WEB_THREAD), isMainThread() is false on the web thread
    // whenever the web thread lock is momentarily dropped, so an owner latched in that window would
    // reject later legitimate access from the UI thread. WebThreadIsEnabled() is not visible to WTF,
    // so this bypass has to live here rather than in ThreadLikeReleaseAssertion.
    void assertIsOwnerThreadForReading() const WTF_ASSERTS_ACQUIRED_SHARED_LOCK(m_lock)
    {
#if PLATFORM(IOS_FAMILY)
        if (WebThreadIsEnabled())
            return;
#endif
        assertIsOwnerThread(m_lock, m_ownerThread);
    }

    void releaseAssertOrSetThreadUID()
    {
#if PLATFORM(IOS_FAMILY)
        if (WebThreadIsEnabled())
            return;
#endif
        releaseAssertIsCurrentAndLatch(m_ownerThread);
    }

    // Mutating lookup, for callers that write through the result. Requires the lock, so that those
    // writes are covered even though thread safety analysis cannot follow the returned pointer.
    EventListenerVector* NODELETE findForWriting(const AtomString& eventType) WTF_REQUIRES_LOCK(m_lock);

    // Only mutated on the owner thread while holding m_lock, so owner-thread reads use
    // assertIsOwnerThread() instead of locking; the GC thread must lock even to read
    // (see visitJSEventListenersInGCThread()).
    Vector<std::pair<AtomString, EventListenerVector>, 0, CrashOnOverflow, 4> m_entries WTF_GUARDED_BY_LOCK(m_lock);
    mutable Lock m_lock;
    ThreadLikeReleaseAssertion m_ownerThread;
};

template<typename Visitor>
void EventListenerMap::visitJSEventListenersInGCThread(Visitor& visitor)
{
    Locker locker { m_lock };
    for (auto& entry : m_entries) {
        for (auto& eventListener : entry.second)
            eventListener->callback().visitJSFunctionInGCThread(visitor);
    }
}

} // namespace WebCore
