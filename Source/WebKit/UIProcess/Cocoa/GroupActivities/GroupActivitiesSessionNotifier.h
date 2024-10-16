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

#include "GroupActivitiesSession.h"
#include <wtf/HashMap.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/URLHash.h>

OBJC_CLASS WKGroupSessionObserver;

namespace WebKit {

class WebPageProxy;

class GroupActivitiesSessionNotifier : public RefCountedAndCanMakeWeakPtr<GroupActivitiesSessionNotifier> {
    WTF_MAKE_TZONE_ALLOCATED(GroupActivitiesSessionNotifier);
public:
    static GroupActivitiesSessionNotifier& singleton();
    static Ref<GroupActivitiesSessionNotifier> create();

    bool hasSessionForURL(const URL&);
    RefPtr<GroupActivitiesSession> takeSessionForURL(const URL&);
    void removeSession(const GroupActivitiesSession&);

    void addWebPage(WebPageProxy&);
    void removeWebPage(WebPageProxy&);
    void webPageURLChanged(WebPageProxy&);

private:
    friend class NeverDestroyed<GroupActivitiesSessionNotifier>;
    GroupActivitiesSessionNotifier();

    void sessionStateChanged(const GroupActivitiesSession&, GroupActivitiesSession::State);

    UncheckedKeyHashMap<URL, Ref<GroupActivitiesSession>> m_sessions;
    RetainPtr<WKGroupSessionObserver> m_sessionObserver;
    WeakHashSet<WebPageProxy> m_webPages;
    GroupActivitiesSession::StateChangeObserver m_stateChangeObserver;
};

}

#endif
