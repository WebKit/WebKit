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

#include "PointerID.h"
#include <wtf/HashMap.h>

namespace WebCore {

class Element;
class EventTarget;
class PointerEvent;

class PointerCaptureController {
    WTF_MAKE_NONCOPYABLE(PointerCaptureController);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit PointerCaptureController(Page&);

    ExceptionOr<void> setPointerCapture(Element*, PointerID);
    ExceptionOr<void> releasePointerCapture(Element*, PointerID);
    bool hasPointerCapture(Element*, PointerID);

    void pointerLockWasApplied();

#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY)
    std::pair<bool, bool> dispatchEventForTouchAtIndex(EventTarget&, const PlatformTouchEvent&, unsigned, bool isPrimary, WindowProxy&);
#endif

    void touchEndedOrWasCancelledForIdentifier(PointerID);
    bool hasCancelledPointerEventForIdentifier(PointerID);
    void dispatchEvent(PointerEvent&, EventTarget*);
    WEBCORE_EXPORT void cancelPointer(PointerID, const IntPoint&);

private:
    struct CapturingData {
        RefPtr<Element> pendingTargetOverride;
        RefPtr<Element> targetOverride;
        String pointerType;
        bool cancelled { false };
    };

    void pointerEventWillBeDispatched(const PointerEvent&, EventTarget*);
    void pointerEventWasDispatched(const PointerEvent&);
    void processPendingPointerCapture(const PointerEvent&);

    Page& m_page;
    // While PointerID is defined as int32_t, we use int64_t here so that we may use a value outside of the int32_t range to have safe
    // empty and removed values, allowing any int32_t to be provided through the API for lookup in this hashmap.
    using PointerIdToCapturingDataMap = HashMap<int64_t, CapturingData, WTF::IntHash<int64_t>, WTF::SignedWithZeroKeyHashTraits<int64_t>>;
    PointerIdToCapturingDataMap m_activePointerIdsToCapturingData;
};

} // namespace WebCore

#endif // ENABLE(POINTER_EVENTS)
