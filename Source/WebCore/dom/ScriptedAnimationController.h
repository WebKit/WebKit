/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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

#include "AnimationFrameRate.h"
#include "Document.h"
#include "ReducedResolutionSeconds.h"
#include "Timer.h"
#include <wtf/CheckedPtr.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class ImminentlyScheduledWorkScope;
class Page;
class RequestAnimationFrameCallback;
class UserGestureToken;
class WeakPtrImplWithEventTargetData;

class ScriptedAnimationController : public RefCounted<ScriptedAnimationController>
{
public:
    static Ref<ScriptedAnimationController> create(Document& document)
    {
        return adoptRef(*new ScriptedAnimationController(document));
    }
    ~ScriptedAnimationController();
    void clearDocumentPointer() { m_document = nullptr; }

    WEBCORE_EXPORT Seconds interval() const;
    WEBCORE_EXPORT OptionSet<ThrottlingReason> throttlingReasons() const;

    void suspend();
    void resume();

    void addThrottlingReason(ThrottlingReason reason) { m_throttlingReasons.add(reason); }
    void removeThrottlingReason(ThrottlingReason reason) { m_throttlingReasons.remove(reason); }

    using CallbackId = int;
    CallbackId registerCallback(Ref<RequestAnimationFrameCallback>&&);
    void cancelCallback(CallbackId);
    void serviceRequestAnimationFrameCallbacks(ReducedResolutionSeconds);

private:
    ScriptedAnimationController(Document&);

    Page* page() const;
    Seconds preferredScriptedAnimationInterval() const;
    bool isThrottledRelativeToPage() const;
    bool shouldRescheduleRequestAnimationFrame(ReducedResolutionSeconds) const;
    void scheduleAnimation();
    RefPtr<Document> protectedDocument();

    struct CallbackData {
        Ref<RequestAnimationFrameCallback> callback;
        RefPtr<UserGestureToken> userGestureTokenToForward;
        RefPtr<ImminentlyScheduledWorkScope> scheduledWorkScope;
    };
    Vector<CallbackData> m_callbackDataList;

    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_document;
    CallbackId m_nextCallbackId { 0 };
    int m_suspendCount { 0 };

    ReducedResolutionSeconds m_lastAnimationFrameTimestamp;
    OptionSet<ThrottlingReason> m_throttlingReasons;
};

} // namespace WebCore
