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
    explicit NumberInputType(HTMLInputElement& element) : TextFieldInputType(element) { }

private:
    virtual const AtomicString& formControlType() const override;
    virtual void setValue(const String&, bool valueChanged, TextFieldEventBehavior) override;
    virtual double valueAsDouble() const override;
    virtual void setValueAsDouble(double, TextFieldEventBehavior, ExceptionCode&) const override;
    virtual void setValueAsDecimal(const Decimal&, TextFieldEventBehavior, ExceptionCode&) const override;
    virtual bool typeMismatchFor(const String&) const override;
    virtual bool typeMismatch() const override;
    virtual bool sizeShouldIncludeDecoration(int defaultSize, int& preferredSize) const override;
    virtual float decorationWidth() const override;
    virtual bool isSteppable() const override;
    virtual StepRange createStepRange(AnyStepHandling) const override;
    virtual void handleKeydownEvent(KeyboardEvent*) override;
    virtual Decimal parseToNumber(const String&, const Decimal&) const override;
    virtual String serialize(const Decimal&) const override;
    virtual String localizeValue(const String&) const override;
    virtual String visibleValue() const override;
    virtual String convertFromVisibleValue(const String&) const override;
    virtual String sanitizeValue(const String&) const override;
    virtual bool hasBadInput() const override;
    virtual String badInputText() const override;
    virtual bool shouldRespectSpeechAttribute() override;
    virtual bool supportsPlaceholder() const override;
    virtual bool isNumberField() const override;
    virtual void minOrMaxAttributeChanged() override;
    virtual void stepAttributeChanged() override;
};

} // namespace WebCore

#endif // NumberInputType_h
