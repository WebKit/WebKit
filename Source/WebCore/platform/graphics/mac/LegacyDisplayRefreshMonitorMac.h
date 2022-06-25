/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 */

#pragma once

#if PLATFORM(MAC)

#include "DisplayRefreshMonitor.h"
#include <wtf/WeakPtr.h>

typedef struct __CVDisplayLink *CVDisplayLinkRef;

namespace WebCore {

class LegacyDisplayRefreshMonitorMac : public DisplayRefreshMonitor {
public:
    static Ref<LegacyDisplayRefreshMonitorMac> create(PlatformDisplayID displayID)
    {
        return adoptRef(*new LegacyDisplayRefreshMonitorMac(displayID));
    }
    
    virtual ~LegacyDisplayRefreshMonitorMac();

    void displayLinkCallbackFired();

private:
    explicit LegacyDisplayRefreshMonitorMac(PlatformDisplayID);

    void dispatchDisplayDidRefresh(const DisplayUpdate&) final;

    void stop() final;

    bool startNotificationMechanism() final;
    void stopNotificationMechanism() final;
    std::optional<FramesPerSecond> displayNominalFramesPerSecond() final;
    
    bool ensureDisplayLink();

    static FramesPerSecond nominalFramesPerSecondFromDisplayLink(CVDisplayLinkRef);

    CVDisplayLinkRef m_displayLink { nullptr };
    
    DisplayUpdate m_currentUpdate;
    bool m_displayLinkIsActive { false };
};

} // namespace WebCore

#endif // PLATFORM(MAC)
