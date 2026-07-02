/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_SESSION_COORDINATOR) && HAVE(GROUP_ACTIVITIES)

#include <wtf/Observer.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/URL.h>
#include <wtf/WeakHashSet.h>

OBJC_CLASS WKGroupSession;

namespace WebKit {

class GroupActivitiesSession : public RefCounted<GroupActivitiesSession> {
    WTF_MAKE_TZONE_ALLOCATED(GroupActivitiesSession);
public:
    static Ref<GroupActivitiesSession> create(RetainPtr<WKGroupSession>&&);
    ~GroupActivitiesSession();

    void join();
    void leave();

    enum class State : uint8_t {
        Waiting,
        Joined,
        Invalidated,
    };
    State state() const;

    String uuid() const;
    URL fallbackURL() const;

    using StateChangeObserver = WTF::Observer<void(const GroupActivitiesSession&, State)>;
    void addStateChangeObserver(const StateChangeObserver&);

    using FallbackURLObserver = WTF::Observer<void(const GroupActivitiesSession&, URL)>;
    void addFallbackURLObserver(const FallbackURLObserver&);

private:
    friend class GroupActivitiesSessionNotifier;
    friend class GroupActivitiesCoordinator;
    GroupActivitiesSession(RetainPtr<WKGroupSession>&&);
    WKGroupSession* groupSession() { return m_groupSession.get(); }

    RetainPtr<WKGroupSession> m_groupSession;
    WeakHashSet<StateChangeObserver> m_stateChangeObservers;
    WeakHashSet<FallbackURLObserver> m_fallbackURLObservers;
};

}

#endif // ENABLE(MEDIA_SESSION_COORDINATOR) && HAVE(GROUP_ACTIVITIES)
