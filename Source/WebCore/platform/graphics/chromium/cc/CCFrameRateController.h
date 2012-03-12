/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCFrameRateController_h
#define CCFrameRateController_h

#include "cc/CCTimeSource.h"
#include <wtf/CurrentTime.h>
#include <wtf/Deque.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class CCFrameRateControllerClient {
public:
    virtual void vsyncTick() = 0;

protected:
    virtual ~CCFrameRateControllerClient() { }
};

class CCFrameRateControllerTimeSourceAdapter;

class CCFrameRateController {
public:
    explicit CCFrameRateController(PassRefPtr<CCTimeSource>);
    ~CCFrameRateController();

    void setClient(CCFrameRateControllerClient* client) { m_client = client; }

    void setActive(bool);

    void setMaxPendingFrames(int);

    // Use the following methods to adjust target frame rate.
    //
    // Multiple frames can be in-progress, but for every didBeginFrame, a
    // didFinishFrame should be posted.
    //
    // If the rendering pipeline crashes, call didAbortAllPendingFrames.
    void didBeginFrame();
    void didFinishFrame();
    void didAbortAllPendingFrames();
    void setMaxFramesPending(int); // 0 for unlimited.

protected:
    friend class CCFrameRateControllerTimeSourceAdapter;
    void onTimerTick();

    CCFrameRateControllerClient* m_client;
    int m_numFramesPending;
    int m_maxFramesPending;
    RefPtr<CCTimeSource> m_timeSource;
    OwnPtr<CCFrameRateControllerTimeSourceAdapter> m_timeSourceClientAdapter;
};

}
#endif // CCFrameRateController_h
