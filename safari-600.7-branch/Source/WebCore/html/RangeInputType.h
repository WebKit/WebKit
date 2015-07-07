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

#ifndef RangeInputType_h
#define RangeInputType_h

#include "InputType.h"

namespace WebCore {

class SliderThumbElement;

class RangeInputType final : public InputType {
public:
    explicit RangeInputType(HTMLInputElement&);

private:
    virtual bool isRangeControl() const override;
    virtual const AtomicString& formControlType() const override;
    virtual double valueAsDouble() const override;
    virtual void setValueAsDecimal(const Decimal&, TextFieldEventBehavior, ExceptionCode&) const override;
    virtual bool typeMismatchFor(const String&) const override;
    virtual bool supportsRequired() const override;
    virtual StepRange createStepRange(AnyStepHandling) const override;
    virtual bool isSteppable() const override;
#if !PLATFORM(IOS)
    virtual void handleMouseDownEvent(MouseEvent*) override;
#endif
    virtual void handleKeydownEvent(KeyboardEvent*) override;
    virtual RenderPtr<RenderElement> createInputRenderer(PassRef<RenderStyle>) override;
    virtual void createShadowSubtree() override;
    virtual Decimal parseToNumber(const String&, const Decimal&) const override;
    virtual String serialize(const Decimal&) const override;
    virtual void accessKeyAction(bool sendMouseEvents) override;
    virtual void minOrMaxAttributeChanged() override;
    virtual void setValue(const String&, bool valueChanged, TextFieldEventBehavior) override;
    virtual String fallbackValue() const override;
    virtual String sanitizeValue(const String& proposedValue) const override;
    virtual bool shouldRespectListAttribute() override;
    virtual HTMLElement* sliderThumbElement() const override;
    virtual HTMLElement* sliderTrackElement() const override;

    SliderThumbElement& typedSliderThumbElement() const;

#if ENABLE(DATALIST_ELEMENT)
    virtual void listAttributeTargetChanged() override;
    void updateTickMarkValues();
    virtual Decimal findClosestTickMarkValue(const Decimal&) override;

    bool m_tickMarkValuesDirty;
    Vector<Decimal> m_tickMarkValues;
#endif

#if ENABLE(TOUCH_EVENTS)
    virtual void handleTouchEvent(TouchEvent*) override;

#if PLATFORM(IOS)
    virtual void disabledAttributeChanged() override;
#else
#if ENABLE(TOUCH_SLIDER)
    virtual bool hasTouchEventHandler() const override;
#endif
#endif // PLATFORM(IOS)
#endif // ENABLE(TOUCH_EVENTS)

};

} // namespace WebCore

#endif // RangeInputType_h
