/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "GroupActivitiesSessionNotifier.h"

#if ENABLE(MEDIA_SESSION_COORDINATOR) && HAVE(GROUP_ACTIVITIES)

#import "GroupActivitiesCoordinator.h"
#import "WKGroupSession.h"
#import "WebFrameProxy.h"
#import "WebPageProxy.h"
#import <mutex>
#import <wtf/TZoneMallocInlines.h>

#import "WebKitSwiftSoftLink.h"

namespace WebKit {

using namespace PAL;
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(GroupActivitiesSessionNotifier);

GroupActivitiesSessionNotifier& GroupActivitiesSessionNotifier::singleton()
{
    static NeverDestroyed<GroupActivitiesSessionNotifier> notifier;
    return notifier;
}

GroupActivitiesSessionNotifier::GroupActivitiesSessionNotifier()
    : m_sessionObserver(adoptNS([allocWKGroupSessionObserverInstance() init]))
    , m_stateChangeObserver([this] (auto& session, auto state) { sessionStateChanged(session, state); })
{
    m_sessionObserver.get().newSessionCallback = [weakThis = WeakPtr { *this }] (WKGroupSession *groupSession) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        auto session = GroupActivitiesSession::create(groupSession);
        session->addStateChangeObserver(protectedThis->m_stateChangeObserver);

        for (auto& page : copyToVector(protectedThis->m_webPages)) {
            if (page->mainFrame() && page->mainFrame()->url() == session->fallbackURL()) {
                auto coordinator = GroupActivitiesCoordinator::create(session);
                page->createMediaSessionCoordinator(WTFMove(coordinator), [] (bool) { });
                return;
            }
        }

        auto result = protectedThis->m_sessions.add(session->fallbackURL(), session.copyRef());
        ASSERT_UNUSED(result, result.isNewEntry);

        [[NSWorkspace sharedWorkspace] openURL:session->fallbackURL().createNSURL().get()];
    };
}

void GroupActivitiesSessionNotifier::sessionStateChanged(const GroupActivitiesSession& session, GroupActivitiesSession::State state)
{
    if (state == GroupActivitiesSession::State::Invalidated)
        m_sessions.remove(session.fallbackURL());
};

void GroupActivitiesSessionNotifier::addWebPage(WebPageProxy& webPage)
{
    ASSERT(!m_webPages.contains(webPage));
    m_webPages.add(webPage);

    RefPtr frame = webPage.mainFrame();
    if (!frame)
        return;

    auto session = takeSessionForURL(frame->url());
    if (!session)
        return;

    auto coordinator = GroupActivitiesCoordinator::create(*session);
    webPage.createMediaSessionCoordinator(WTFMove(coordinator), [] (bool) { });
}

void GroupActivitiesSessionNotifier::removeWebPage(WebPageProxy& webPage)
{
    ASSERT(m_webPages.contains(webPage));
    m_webPages.remove(webPage);
}

void GroupActivitiesSessionNotifier::webPageURLChanged(WebPageProxy& webPage)
{
    ASSERT(m_webPages.contains(webPage));
    if (!m_webPages.contains(webPage))
        return;

    RefPtr frame = webPage.mainFrame();
    if (!frame)
        return;

    auto session = takeSessionForURL(frame->url());
    if (!session)
        return;

    auto coordinator = GroupActivitiesCoordinator::create(*session);
    webPage.createMediaSessionCoordinator(WTFMove(coordinator), [] (bool) { });
}

bool GroupActivitiesSessionNotifier::hasSessionForURL(const URL& url)
{
    return m_sessions.contains(url);
}

RefPtr<GroupActivitiesSession> GroupActivitiesSessionNotifier::takeSessionForURL(const URL& url)
{
    return m_sessions.take(url);
}

}

#endif // ENABLE(MEDIA_SESSION_COORDINATOR) && HAVE(GROUP_ACTIVITIES)
