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

#include "DOMHighResTimeStamp.h"
#include "Timer.h"
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class Document;
class Page;
class RequestAnimationFrameCallback;

class ScriptedAnimationController : public RefCounted<ScriptedAnimationController>
{
public:
    static Ref<ScriptedAnimationController> create(Document& document)
    {
        return adoptRef(*new ScriptedAnimationController(document));
    }
    ~ScriptedAnimationController();
    void clearDocumentPointer() { m_document = nullptr; }
    bool requestAnimationFrameEnabled() const;

    typedef int CallbackId;

    CallbackId registerCallback(Ref<RequestAnimationFrameCallback>&&);
    void cancelCallback(CallbackId);
    void serviceRequestAnimationFrameCallbacks(DOMHighResTimeStamp timestamp);

    void suspend();
    void resume();

    enum class ThrottlingReason {
        VisuallyIdle                    = 1 << 0,
        OutsideViewport                 = 1 << 1,
        LowPowerMode                    = 1 << 2,
        NonInteractedCrossOriginFrame   = 1 << 3,
    };
    void addThrottlingReason(ThrottlingReason);
    void removeThrottlingReason(ThrottlingReason);

    WEBCORE_EXPORT bool isThrottled() const;
    WEBCORE_EXPORT Seconds interval() const;

private:
    ScriptedAnimationController(Document&);

    void scheduleAnimation();
    void animationTimerFired();

    Page* page() const;

    typedef Vector<RefPtr<RequestAnimationFrameCallback>> CallbackList;
    CallbackList m_callbacks;

    WeakPtr<Document> m_document;
    CallbackId m_nextCallbackId { 0 };
    int m_suspendCount { 0 };

    Timer m_animationTimer;
    double m_lastAnimationFrameTimestamp { 0 };

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    OptionSet<ThrottlingReason> m_throttlingReasons;
    bool m_isUsingTimer { false };
#endif
};

} // namespace WebCore
