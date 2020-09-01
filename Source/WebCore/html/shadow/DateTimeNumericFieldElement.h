/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

#include "DateTimeFieldElement.h"
#include <wtf/MonotonicTime.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

class DateTimeNumericFieldElement : public DateTimeFieldElement {
    WTF_MAKE_ISO_ALLOCATED(DateTimeNumericFieldElement);
public:
    struct Range {
        Range(int minimum, int maximum)
            : minimum(minimum), maximum(maximum) { }
        int clampValue(int) const;
        bool isInRange(int) const;

        int minimum;
        int maximum;
    };

protected:
    DateTimeNumericFieldElement(Document&, FieldOwner&, const Range&, const String& placeholder);

    // DateTimeFieldElement functions:
    bool hasValue() const final;
    void initialize(const AtomString&);
    void setEmptyValue(EventBehavior = DispatchNoEvent) final;
    void setValueAsInteger(int, EventBehavior = DispatchNoEvent) final;
    int valueAsInteger() const final;
    String visibleValue() const final;

private:
    // DateTimeFieldElement functions:
    String value() const final;
    void handleKeyboardEvent(KeyboardEvent&) final;
    void didBlur() final;

    String formatValue(int) const;

    const Range m_range;
    const String m_placeholder;
    int m_value { 0 };
    bool m_hasValue { false };
    StringBuilder m_typeAheadBuffer;
    MonotonicTime m_lastDigitCharTime;
};

} // namespace WebCore

#endif // ENABLE(DATE_AND_TIME_INPUT_TYPES)
