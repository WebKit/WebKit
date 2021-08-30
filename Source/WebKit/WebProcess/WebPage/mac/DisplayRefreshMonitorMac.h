/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#include "DisplayLinkObserverID.h"
#include <WebCore/DisplayRefreshMonitor.h>
#include <WebCore/RunLoopObserver.h>

namespace WebKit {

class DisplayRefreshMonitorMac final : public WebCore::DisplayRefreshMonitor {
public:
    static Ref<DisplayRefreshMonitorMac> create(WebCore::PlatformDisplayID displayID)
    {
        return adoptRef(*new DisplayRefreshMonitorMac(displayID));
    }

    virtual ~DisplayRefreshMonitorMac();

private:
    explicit DisplayRefreshMonitorMac(WebCore::PlatformDisplayID);

    void dispatchDisplayDidRefresh(const WebCore::DisplayUpdate&) final;

    bool startNotificationMechanism() final;
    void stopNotificationMechanism() final;

    void adjustPreferredFramesPerSecond(WebCore::FramesPerSecond) final;

    DisplayLinkObserverID m_observerID;
    std::unique_ptr<WebCore::RunLoopObserver> m_runLoopObserver;

    bool m_displayLinkIsActive { false };
    bool m_firstCallbackInCurrentRunloop { false };
};

} // namespace WebKit

#endif // PLATFORM(MAC)
