/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "SleepDisablerCocoa.h"

#if PLATFORM(IOS_FAMILY)
#import <wtf/NeverDestroyed.h>

#import <pal/ios/UIKitSoftLink.h>

namespace PAL {

class ScreenSleepDisabler {
public:
    static ScreenSleepDisabler& shared()
    {
        static MainThreadNeverDestroyed<ScreenSleepDisabler> screenSleepDisabler;
        return screenSleepDisabler;
    }

    ScreenSleepDisablerCounterToken takeAssertion()
    {
        return m_screenSleepDisablerCount.count();
    }

private:
    friend NeverDestroyed<ScreenSleepDisabler, WTF::MainThreadAccessTraits>;

    ScreenSleepDisabler()
        : m_screenSleepDisablerCount([this](RefCounterEvent) { updateState(); })
    { }

    void updateState()
    {
        if (!m_screenSleepDisablerCount.value())
            [PAL::getUIApplicationClass() sharedApplication].idleTimerDisabled = NO;
        else if (m_screenSleepDisablerCount.value() == 1)
            [PAL::getUIApplicationClass() sharedApplication].idleTimerDisabled = YES;
    }

    ScreenSleepDisablerCounter m_screenSleepDisablerCount;
};

void SleepDisablerCocoa::takeScreenSleepDisablingAssertion(const String&)
{
    m_screenSleepDisablerToken = ScreenSleepDisabler::shared().takeAssertion();
}

} // namespace PAL

#endif // PLATFORM(IOS_FAMILY)
