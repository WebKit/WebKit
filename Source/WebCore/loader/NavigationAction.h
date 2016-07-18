/*
 * Copyright (C) 2006-2016 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NavigationAction_h
#define NavigationAction_h

#include "Event.h"
#include "FrameLoaderTypes.h"
#include "URL.h"
#include "ResourceRequest.h"
#include "UserGestureIndicator.h"
#include <wtf/Forward.h>

namespace WebCore {

class NavigationAction {
public:
    WEBCORE_EXPORT NavigationAction();
    WEBCORE_EXPORT explicit NavigationAction(const ResourceRequest&);
    WEBCORE_EXPORT NavigationAction(const ResourceRequest&, NavigationType);
    WEBCORE_EXPORT NavigationAction(const ResourceRequest&, FrameLoadType, bool isFormSubmission);

    NavigationAction(const ResourceRequest&, ShouldOpenExternalURLsPolicy);
    NavigationAction(const ResourceRequest&, NavigationType, Event*);
    NavigationAction(const ResourceRequest&, NavigationType, Event*, ShouldOpenExternalURLsPolicy);
    NavigationAction(const ResourceRequest&, NavigationType, Event*, ShouldOpenExternalURLsPolicy, const AtomicString& downloadAttribute);
    NavigationAction(const ResourceRequest&, NavigationType, ShouldOpenExternalURLsPolicy);
    NavigationAction(const ResourceRequest&, FrameLoadType, bool isFormSubmission, Event*);
    NavigationAction(const ResourceRequest&, FrameLoadType, bool isFormSubmission, Event*, ShouldOpenExternalURLsPolicy);
    NavigationAction(const ResourceRequest&, FrameLoadType, bool isFormSubmission, Event*, ShouldOpenExternalURLsPolicy, const AtomicString& downloadAttribute);

    NavigationAction copyWithShouldOpenExternalURLsPolicy(ShouldOpenExternalURLsPolicy) const;

    bool isEmpty() const { return m_resourceRequest.url().isEmpty(); }

    URL url() const { return m_resourceRequest.url(); }
    const ResourceRequest& resourceRequest() const { return m_resourceRequest; }

    NavigationType type() const { return m_type; }
    const Event* event() const { return m_event.get(); }

    bool processingUserGesture() const { return m_userGestureToken ? m_userGestureToken->processingUserGesture() : false; }
    RefPtr<UserGestureToken> userGestureToken() const { return m_userGestureToken; }

    ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy() const { return m_shouldOpenExternalURLsPolicy; }

    const AtomicString& downloadAttribute() const { return m_downloadAttribute; }

private:
    ResourceRequest m_resourceRequest;
    NavigationType m_type { NavigationType::Other };
    RefPtr<Event> m_event;
    RefPtr<UserGestureToken> m_userGestureToken;
    ShouldOpenExternalURLsPolicy m_shouldOpenExternalURLsPolicy { ShouldOpenExternalURLsPolicy::ShouldNotAllow };
    AtomicString m_downloadAttribute;
};

}

#endif
