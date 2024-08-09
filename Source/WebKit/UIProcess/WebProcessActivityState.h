/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <wtf/WeakRef.h>

namespace WebKit {

class ProcessThrottlerActivity;
class WebProcessProxy;

class WebProcessActivityState {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WebProcessActivityState(WebProcessProxy&);
    void takeVisibleActivity();
    void takeAudibleActivity();
    void takeCapturingActivity();

    void reset();
    void dropVisibleActivity();
    void dropAudibleActivity();
    void dropCapturingActivity();

    bool hasValidVisibleActivity() const;
    bool hasValidAudibleActivity() const;
    bool hasValidCapturingActivity() const;

#if PLATFORM(IOS_FAMILY)
    void takeOpeningAppLinkActivity();
    void dropOpeningAppLinkActivity();
    bool hasValidOpeningAppLinkActivity() const;
#endif

private:
    WeakRef<WebProcessProxy> m_process;

    std::unique_ptr<ProcessThrottlerActivity> m_isVisibleActivity;
#if PLATFORM(MAC)
    UniqueRef<ProcessThrottlerTimedActivity> m_wasRecentlyVisibleActivity;
#endif
    std::unique_ptr<ProcessThrottlerActivity> m_isAudibleActivity;
    std::unique_ptr<ProcessThrottlerActivity> m_isCapturingActivity;
#if PLATFORM(IOS_FAMILY)
    std::unique_ptr<ProcessThrottlerActivity> m_openingAppLinkActivity;
#endif
};

} // namespace WebKit
