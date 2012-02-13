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

#ifndef NumberInputType_h
#define NumberInputType_h

#include "TextFieldInputType.h"

namespace WebCore {

class NumberInputType : public TextFieldInputType {
public:
    static PassOwnPtr<InputType> create(HTMLInputElement*);

private:
    NumberInputType(HTMLInputElement* element) : TextFieldInputType(element) { }
    virtual const AtomicString& formControlType() const OVERRIDE;
    virtual double valueAsNumber() const OVERRIDE;
    virtual void setValueAsNumber(double, TextFieldEventBehavior, ExceptionCode&) const OVERRIDE;
    virtual bool typeMismatchFor(const String&) const OVERRIDE;
    virtual bool typeMismatch() const OVERRIDE;
    virtual bool rangeUnderflow(const String&) const OVERRIDE;
    virtual bool rangeOverflow(const String&) const OVERRIDE;
    virtual bool supportsRangeLimitation() const OVERRIDE;
    virtual double minimum() const OVERRIDE;
    virtual double maximum() const OVERRIDE;
    virtual bool sizeShouldIncludeDecoration(int defaultSize, int& preferredSize) const OVERRIDE;
    virtual bool isSteppable() const OVERRIDE;
    virtual bool stepMismatch(const String&, double) const OVERRIDE;
    virtual double stepBase() const OVERRIDE;
    virtual double stepBaseWithDecimalPlaces(unsigned*) const OVERRIDE;
    virtual double defaultStep() const OVERRIDE;
    virtual double stepScaleFactor() const OVERRIDE;
    virtual void handleKeydownEvent(KeyboardEvent*) OVERRIDE;
    virtual void handleWheelEvent(WheelEvent*) OVERRIDE;
    virtual double parseToDouble(const String&, double) const OVERRIDE;
    virtual double parseToDoubleWithDecimalPlaces(const String&, double, unsigned*) const OVERRIDE;
    virtual String serialize(double) const OVERRIDE;
    virtual double acceptableError(double) const OVERRIDE;
    virtual void handleBlurEvent() OVERRIDE;
    virtual String visibleValue() const OVERRIDE;
    virtual String convertFromVisibleValue(const String&) const OVERRIDE;
    virtual bool isAcceptableValue(const String&) OVERRIDE;
    virtual String sanitizeValue(const String&) const OVERRIDE;
    virtual bool hasUnacceptableValue() OVERRIDE;
    virtual bool shouldRespectSpeechAttribute() OVERRIDE;
    virtual bool supportsPlaceholder() const OVERRIDE;
    virtual bool isNumberField() const OVERRIDE;
    virtual void minOrMaxAttributeChanged() OVERRIDE;
    virtual void stepAttributeChanged() OVERRIDE;
};

} // namespace WebCore

#endif // NumberInputType_h
