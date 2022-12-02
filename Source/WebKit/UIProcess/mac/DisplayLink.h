/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#if HAVE(CVDISPLAYLINK)

#include "DisplayLinkObserverID.h"
#include <CoreVideo/CVDisplayLink.h>
#include <WebCore/AnimationFrameRate.h>
#include <WebCore/DisplayUpdate.h>
#include <WebCore/PlatformScreen.h>
#include <wtf/CheckedPtr.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>

namespace WebKit {

class DisplayLink {
    WTF_MAKE_FAST_ALLOCATED;
public:
    class Client : public CanMakeThreadSafeCheckedPtr {
    friend class DisplayLink;
    public:
        virtual ~Client() = default;
        
    private:
        virtual void displayLinkFired(WebCore::PlatformDisplayID, WebCore::DisplayUpdate, bool wantsFullSpeedUpdates, bool anyObserverWantsCallback) = 0;
    };

    explicit DisplayLink(WebCore::PlatformDisplayID);
    ~DisplayLink();

    WebCore::PlatformDisplayID displayID() const { return m_displayID; }
    WebCore::FramesPerSecond nominalFramesPerSecond() const { return m_displayNominalFramesPerSecond; }

    void addObserver(Client&, DisplayLinkObserverID, WebCore::FramesPerSecond);
    void removeObserver(Client&, DisplayLinkObserverID);

    void removeClient(Client&);

    // FIXME: Maybe callers should just register a DisplayLinkObserverID with the appropriate fps.
    void incrementFullSpeedRequestClientCount(Client&);
    void decrementFullSpeedRequestClientCount(Client&);

    void setObserverPreferredFramesPerSecond(Client&, DisplayLinkObserverID, WebCore::FramesPerSecond);

private:
    static CVReturn displayLinkCallback(CVDisplayLinkRef, const CVTimeStamp*, const CVTimeStamp*, CVOptionFlags, CVOptionFlags*, void* data);
    void notifyObserversDisplayWasRefreshed();

    bool removeInfoForClientIfUnused(Client&) WTF_REQUIRES_LOCK(m_clientsLock);

    static WebCore::FramesPerSecond nominalFramesPerSecondFromDisplayLink(CVDisplayLinkRef);

    struct ObserverInfo {
        DisplayLinkObserverID observerID;
        WebCore::FramesPerSecond preferredFramesPerSecond;
    };

    struct ClientInfo {
        unsigned fullSpeedUpdatesClientCount { 0 };
        Vector<ObserverInfo> observers;
    };

    CVDisplayLinkRef m_displayLink { nullptr };
    Lock m_clientsLock;
    HashMap<CheckedRef<Client>, ClientInfo> m_clients WTF_GUARDED_BY_LOCK(m_clientsLock);
    const WebCore::PlatformDisplayID m_displayID;
    WebCore::FramesPerSecond m_displayNominalFramesPerSecond { WebCore::FullSpeedFramesPerSecond };
    WebCore::DisplayUpdate m_currentUpdate;
    unsigned m_fireCountWithoutObservers { 0 };
};

class DisplayLinkCollection {
public:
    void add(std::unique_ptr<DisplayLink>&&);

    DisplayLink* displayLinkForDisplay(WebCore::PlatformDisplayID) const;

    const Vector<std::unique_ptr<DisplayLink>>& displayLinks() const { return m_displayLinks; }

private:
    Vector<std::unique_ptr<DisplayLink>> m_displayLinks;
};

} // namespace WebKit

#endif // HAVE(CVDISPLAYLINK)
