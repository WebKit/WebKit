/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "InputType.h"

namespace WebCore {

class SliderThumbElement;

class RangeInputType final : public InputType {
public:
    explicit RangeInputType(HTMLInputElement&);

private:
    bool isRangeControl() const final;
    const AtomString& formControlType() const final;
    double valueAsDouble() const final;
    ExceptionOr<void> setValueAsDecimal(const Decimal&, TextFieldEventBehavior) const final;
    bool typeMismatchFor(const String&) const final;
    bool supportsRequired() const final;
    StepRange createStepRange(AnyStepHandling) const final;
    bool isSteppable() const final;
#if !PLATFORM(IOS_FAMILY)
    void handleMouseDownEvent(MouseEvent&) final;
#endif
    ShouldCallBaseEventHandler handleKeydownEvent(KeyboardEvent&) final;
    RenderPtr<RenderElement> createInputRenderer(RenderStyle&&) final;
    void createShadowSubtree() final;
    Decimal parseToNumber(const String&, const Decimal&) const final;
    String serialize(const Decimal&) const final;
    void accessKeyAction(bool sendMouseEvents) final;
    void attributeChanged(const QualifiedName&) final;
    void setValue(const String&, bool valueChanged, TextFieldEventBehavior) final;
    String fallbackValue() const final;
    String sanitizeValue(const String& proposedValue) const final;
    bool shouldRespectListAttribute() final;
    HTMLElement* sliderThumbElement() const final;
    HTMLElement* sliderTrackElement() const final;

    SliderThumbElement& typedSliderThumbElement() const;

#if ENABLE(DATALIST_ELEMENT)
    void listAttributeTargetChanged() final;
    void updateTickMarkValues();
    Optional<Decimal> findClosestTickMarkValue(const Decimal&) final;

    bool m_tickMarkValuesDirty { true };
    Vector<Decimal> m_tickMarkValues;
#endif

#if ENABLE(TOUCH_EVENTS)
    void handleTouchEvent(TouchEvent&) final;
#endif

    void disabledStateChanged() final;

#if ENABLE(TOUCH_EVENTS) && !PLATFORM(IOS_FAMILY) && ENABLE(TOUCH_SLIDER)
    bool hasTouchEventHandler() const final;
#endif
};

} // namespace WebCore
