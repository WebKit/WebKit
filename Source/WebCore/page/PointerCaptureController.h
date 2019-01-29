/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(POINTER_EVENTS)

#include <wtf/HashMap.h>

namespace WebCore {

class Element;
class EventTarget;
class PointerEvent;

class PointerCaptureController {
    WTF_MAKE_NONCOPYABLE(PointerCaptureController);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit PointerCaptureController();

    enum class ImplicitCapture : uint8_t { Yes, No };

    ExceptionOr<void> setPointerCapture(Element*, int32_t);
    ExceptionOr<void> releasePointerCapture(Element*, int32_t, ImplicitCapture implicit = ImplicitCapture::No);
    bool hasPointerCapture(Element*, int32_t);

    void pointerLockWasApplied();

    void touchEndedOrWasCancelledForIdentifier(int32_t);
    void pointerEventWillBeDispatched(const PointerEvent&, EventTarget*);
    void pointerEventWasDispatched(const PointerEvent&);

private:
    struct CapturingData {
        Element* pendingTargetOverride;
        Element* targetOverride;
    };

    void processPendingPointerCapture(const PointerEvent&);
    HashMap<int32_t, CapturingData> m_activePointerIdsToCapturingData;
};

} // namespace WebCore

#endif // ENABLE(POINTER_EVENTS)
