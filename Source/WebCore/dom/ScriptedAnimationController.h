/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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
 *  THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once

#include "DOMTimeStamp.h"
#include "PlatformScreen.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

#if USE(REQUEST_ANIMATION_FRAME_TIMER)
#include "Timer.h"
#endif

#if USE(REQUEST_ANIMATION_FRAME_TIMER) && USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
#include "Chrome.h"
#include "ChromeClient.h"
#include "DisplayRefreshMonitorClient.h"
#endif

namespace WebCore {

class Document;
class RequestAnimationFrameCallback;

class ScriptedAnimationController : public RefCounted<ScriptedAnimationController>
#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    , public DisplayRefreshMonitorClient
#endif
{
public:
    static Ref<ScriptedAnimationController> create(Document* document, PlatformDisplayID displayID)
    {
        return adoptRef(*new ScriptedAnimationController(document, displayID));
    }
    ~ScriptedAnimationController();
    void clearDocumentPointer() { m_document = nullptr; }
    bool requestAnimationFrameEnabled() const;

    typedef int CallbackId;

    CallbackId registerCallback(Ref<RequestAnimationFrameCallback>&&);
    void cancelCallback(CallbackId);
    void serviceScriptedAnimations(double timestamp);

    void suspend();
    void resume();
    void setThrottled(bool);
    WEBCORE_EXPORT bool isThrottled() const;

    void windowScreenDidChange(PlatformDisplayID);

private:
    ScriptedAnimationController(Document*, PlatformDisplayID);

    typedef Vector<RefPtr<RequestAnimationFrameCallback>> CallbackList;
    CallbackList m_callbacks;

    Document* m_document;
    CallbackId m_nextCallbackId { 0 };
    int m_suspendCount { 0 };

    void scheduleAnimation();

#if USE(REQUEST_ANIMATION_FRAME_TIMER)
    void animationTimerFired();
    Timer m_animationTimer;
    double m_lastAnimationFrameTimestamp { 0 };
#endif

#if USE(REQUEST_ANIMATION_FRAME_TIMER) && USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    // Override for DisplayRefreshMonitorClient
    void displayRefreshFired() override;
    RefPtr<DisplayRefreshMonitor> createDisplayRefreshMonitor(PlatformDisplayID) const override;

    bool m_isUsingTimer { false };
    bool m_isThrottled { false };
#endif
};

} // namespace WebCore
