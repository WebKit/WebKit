/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef PageThrottler_h
#define PageThrottler_h

#include "Timer.h"

#include "UserActivity.h"
#include "ViewState.h"
#include <wtf/RefCounter.h>

namespace WebCore {

class Page;

enum PageActivityCounterType { };
typedef RefCounter<PageActivityCounterType> PageActivityCounter;
typedef PageActivityCounter::Token PageActivityAssertionToken;

struct PageActivityState {
    enum {
        UserInputActivity = 1 << 0,
        MediaActivity = 1 << 1,
        PageLoadActivity = 1 << 2,
    };

    typedef unsigned Flags;

    static const Flags NoFlags = 0;
    static const Flags AllFlags = UserInputActivity | MediaActivity | PageLoadActivity;
};

class PageThrottler {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PageThrottler(Page&);

    void didReceiveUserInput() { m_userInputHysteresis.impulse(); }
    PageActivityState::Flags activityState() { return m_activityState; }
    void pluginDidEvaluateWhileAudioIsPlaying() { m_mediaActivityHysteresis.impulse(); }
    PageActivityAssertionToken mediaActivityToken();
    PageActivityAssertionToken pageLoadActivityToken();

private:
    void mediaActivityCounterChanged();
    void setActivityFlag(PageActivityState::Flags, bool);

    Page& m_page;
    PageActivityState::Flags m_activityState { PageActivityState::NoFlags };
    HysteresisActivity m_userInputHysteresis;
    HysteresisActivity m_mediaActivityHysteresis;
    PageActivityCounter m_mediaActivityCounter;
    PageActivityCounter m_pageLoadActivityCounter;
};

}
#endif
