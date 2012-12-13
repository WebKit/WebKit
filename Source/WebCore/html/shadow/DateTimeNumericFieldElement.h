/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef DateTimeNumericFieldElement_h
#define DateTimeNumericFieldElement_h

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "DateTimeFieldElement.h"

namespace WebCore {

// DateTimeNumericFieldElement represents numeric field of date time format,
// such as:
//  - hour
//  - minute
//  - millisecond
//  - second
//  - year
class DateTimeNumericFieldElement : public DateTimeFieldElement {
    WTF_MAKE_NONCOPYABLE(DateTimeNumericFieldElement);

protected:
    struct Range {
        Range(int minimum, int maximum);
        int clampValue(int) const;
        bool isInRange(int) const;

        int maximum;
        int minimum;
    };

    DateTimeNumericFieldElement(Document*, FieldOwner&, int minimum, int maximum, const String& placeholder, int step = 1, int stepBase = 0);

    int clampValue(int value) const { return m_range.clampValue(value); }
    virtual int clampValueForHardLimits(int) const;
    virtual int defaultValueForStepDown() const;
    virtual int defaultValueForStepUp() const;
    const Range& range() const { return m_range; }

    // DateTimeFieldElement functions.
    virtual bool hasValue() const OVERRIDE FINAL;
    virtual int maximum() const OVERRIDE FINAL;
    virtual void setEmptyValue(EventBehavior = DispatchNoEvent) OVERRIDE FINAL;
    virtual void setValueAsInteger(int, EventBehavior = DispatchNoEvent) OVERRIDE;
    virtual int valueAsInteger() const OVERRIDE;
    virtual String visibleValue() const OVERRIDE FINAL;

private:
    // DateTimeFieldElement functions.
    virtual void didBlur() OVERRIDE FINAL;
    virtual void handleKeyboardEvent(KeyboardEvent*) OVERRIDE FINAL;
    virtual float maximumWidth(const Font&) OVERRIDE;
    virtual int minimum() const OVERRIDE FINAL;
    virtual void stepDown() OVERRIDE FINAL;
    virtual void stepUp() OVERRIDE FINAL;
    virtual String value() const OVERRIDE FINAL;

    String formatValue(int) const;
    int roundUp(int) const;
    int roundDown(int) const;

    DOMTimeStamp m_lastDigitCharTime;
    const String m_placeholder;
    const Range m_range;
    int m_value;
    bool m_hasValue;
    int m_step;
    int m_stepBase;
};

} // namespace WebCore

#endif
#endif
