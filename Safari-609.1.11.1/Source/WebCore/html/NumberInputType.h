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

#include "TextFieldInputType.h"

namespace WebCore {

class NumberInputType final : public TextFieldInputType {
public:
    explicit NumberInputType(HTMLInputElement& element) : TextFieldInputType(element) { }

private:
    const AtomString& formControlType() const final;
    void setValue(const String&, bool valueChanged, TextFieldEventBehavior) final;
    double valueAsDouble() const final;
    ExceptionOr<void> setValueAsDouble(double, TextFieldEventBehavior) const final;
    ExceptionOr<void> setValueAsDecimal(const Decimal&, TextFieldEventBehavior) const final;
    bool typeMismatchFor(const String&) const final;
    bool typeMismatch() const final;
    bool sizeShouldIncludeDecoration(int defaultSize, int& preferredSize) const final;
    float decorationWidth() const final;
    bool isSteppable() const final;
    StepRange createStepRange(AnyStepHandling) const final;
    ShouldCallBaseEventHandler handleKeydownEvent(KeyboardEvent&) final;
    Decimal parseToNumber(const String&, const Decimal&) const final;
    String serialize(const Decimal&) const final;
    String localizeValue(const String&) const final;
    String visibleValue() const final;
    String convertFromVisibleValue(const String&) const final;
    String sanitizeValue(const String&) const final;
    bool hasBadInput() const final;
    String badInputText() const final;
    bool supportsPlaceholder() const final;
    bool isNumberField() const final;
    void attributeChanged(const QualifiedName&) final;
};

} // namespace WebCore
