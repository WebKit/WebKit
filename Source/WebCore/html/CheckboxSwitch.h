/*
 * Copyright (C) 2023-2026 Apple Inc. All rights reserved.
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

#include "SwitchTrigger.h"
#include <wtf/CheckedPtr.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class HTMLInputElement;
class LayoutPoint;
class MouseEvent;
class Timer;
class WeakPtrImplWithEventTargetData;

#if ENABLE(IOS_TOUCH_EVENTS)
class Touch;
class TouchEvent;
#endif

enum class SwitchAnimationType : bool { VisuallyOn, Held };

class CheckboxSwitch final : public CanMakeCheckedPtr<CheckboxSwitch> {
    WTF_MAKE_TZONE_ALLOCATED(CheckboxSwitch);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(CheckboxSwitch);
public:
    explicit CheckboxSwitch(HTMLInputElement&);
    ~CheckboxSwitch();

    void handleMouseDownEvent(MouseEvent&);
    void handleMouseMoveEvent(MouseEvent&);
    void handleMouseUpEvent(MouseEvent&);
#if ENABLE(IOS_TOUCH_EVENTS)
    // Returns whether the element should get a click.
    bool handleTouchEvent(TouchEvent&);
#endif
    bool willSkipToggleForClick(bool checked);
    void didToggleForClick(bool wasChecked);
    bool takeDidSkipSwitchToggle();
    void stopSwitchPointerTracking();
    void stopAnimationsAndPointerTracking();

    float switchAnimationVisuallyOnProgress() const;
    NODELETE bool isSwitchVisuallyOn() const;
    float switchAnimationHeldProgress() const;
    NODELETE bool isSwitchHeld() const;

private:
    HTMLInputElement* element() const { return m_element.get(); }
    bool isSwitch() const;

#if ENABLE(IOS_TOUCH_EVENTS)
    Touch* subsequentTouchEventTouch(const TouchEvent&) const;
#endif
    void startSwitchPointerTracking(LayoutPoint);
    NODELETE bool isSwitchPointerTracking() const;
    bool hasSwitchThumbReturned() const;
    NODELETE Seconds switchAnimationStartTime(SwitchAnimationType) const;
    NODELETE void setSwitchAnimationStartTime(SwitchAnimationType, Seconds);
    NODELETE bool isSwitchAnimating(SwitchAnimationType) const;
    void performSwitchAnimation(SwitchAnimationType);
    void performSwitchVisuallyOnAnimation(SwitchTrigger);
    void setIsSwitchHeld(bool /* isHeld */);
    NODELETE void stopSwitchAnimation(SwitchAnimationType);
    float switchAnimationProgress(SwitchAnimationType) const;
    void updateIsSwitchVisuallyOnFromAbsoluteLocation(LayoutPoint);
    void switchAnimationTimerFired();

    WeakPtr<HTMLInputElement, WeakPtrImplWithEventTargetData> m_element;
    std::optional<int> m_switchPointerTrackingLogicalLeftPositionStart { std::nullopt };
    bool m_hasSwitchVisuallyOnChanged { false };
    bool m_isSwitchVisuallyOn { false };
    bool m_isSwitchHeld { false };
    bool m_didSkipSwitchToggle { false };
    Seconds m_switchAnimationVisuallyOnStartTime { 0_s };
    Seconds m_switchAnimationHeldStartTime { 0_s };
    const std::unique_ptr<Timer> m_switchAnimationTimer;
#if ENABLE(IOS_TOUCH_EVENTS)
    const std::unique_ptr<Timer> m_switchHeldTimer;
    std::optional<unsigned> m_switchPointerTrackingTouchIdentifier { std::nullopt };
#endif
};

} // namespace WebCore
