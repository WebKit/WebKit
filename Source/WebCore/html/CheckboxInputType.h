/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "BaseCheckableInputType.h"

namespace WebCore {

enum class WasSetByJavaScript : bool;

enum class SwitchAnimationType : bool { VisuallyOn, Pressed };

class CheckboxInputType final : public BaseCheckableInputType {
public:
    static Ref<CheckboxInputType> create(HTMLInputElement& element)
    {
        return adoptRef(*new CheckboxInputType(element));
    }

    bool valueMissing(const String&) const final;
    float switchAnimationVisuallyOnProgress() const;
    bool isSwitchVisuallyOn() const;
    float switchAnimationPressedProgress() const;

private:
    explicit CheckboxInputType(HTMLInputElement& element)
        : BaseCheckableInputType(Type::Checkbox, element)
    {
    }

    const AtomString& formControlType() const final;
    String valueMissingText() const final;
    void createShadowSubtree() final;
    void handleKeyupEvent(KeyboardEvent&) final;
    void handleMouseDownEvent(MouseEvent&) final;
    void handleMouseMoveEvent(MouseEvent&) final;
// FIXME: It should not be iOS-specific, but it's not been tested with a non-iOS touch
// implementation thus far.
#if ENABLE(IOS_TOUCH_EVENTS)
    void handleTouchEvent(TouchEvent&) final;
#endif
    void startSwitchPointerTracking(LayoutPoint, std::optional<unsigned> = std::nullopt);
    void stopSwitchPointerTracking();
    bool isSwitchPointerTracking() const;
    void willDispatchClick(InputElementClickState&) final;
    void didDispatchClick(Event&, const InputElementClickState&) final;
    bool matchesIndeterminatePseudoClass() const final;
    void willUpdateCheckedness(bool /* nowChecked */, WasSetByJavaScript);
    void disabledStateChanged() final;
    Seconds switchAnimationStartTime(SwitchAnimationType) const;
    void setSwitchAnimationStartTime(SwitchAnimationType, Seconds);
    bool isSwitchAnimating(SwitchAnimationType) const;
    void performSwitchAnimation(SwitchAnimationType);
    void stopSwitchAnimation(SwitchAnimationType);
    float switchAnimationProgress(SwitchAnimationType) const;
    void updateIsSwitchVisuallyOnFromAbsoluteLocation(LayoutPoint);
    void switchAnimationTimerFired();

    // FIXME: Consider moving all switch-related state (and methods?) to their own object so
    // CheckboxInputType can stay somewhat small.
    std::optional<int> m_switchPointerTrackingLogicalLeftPositionStart { std::nullopt };
    bool m_hasSwitchVisuallyOnChanged { false };
    bool m_isSwitchVisuallyOn { false };
    Seconds m_switchAnimationVisuallyOnStartTime { 0_s };
    Seconds m_switchAnimationPressedStartTime { 0_s };
    std::unique_ptr<Timer> m_switchAnimationTimer;
    std::optional<unsigned> m_switchPointerTrackingTouchIdentifier { std::nullopt };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_INPUT_TYPE(CheckboxInputType, Type::Checkbox)
