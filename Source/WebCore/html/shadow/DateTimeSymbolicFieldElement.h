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

#ifndef DateTimeSymbolicFieldElement_h
#define DateTimeSymbolicFieldElement_h

#if ENABLE(INPUT_TYPE_TIME_MULTIPLE_FIELDS)
#include "DateTimeFieldElement.h"

namespace WebCore {

// DateTimeSymbolicFieldElement represents non-numeric field of data time
// format, such as: AM/PM, and month.
class DateTimeSymbolicFieldElement : public DateTimeFieldElement {
    WTF_MAKE_NONCOPYABLE(DateTimeSymbolicFieldElement);

protected:
    DateTimeSymbolicFieldElement(Document*, FieldEventHandler&, const Vector<String>&);
    virtual void setValueAsInteger(int, EventBehavior = DispatchNoEvent) OVERRIDE FINAL;

private:
    static const int invalidIndex = -1;

    // DateTimeFieldElement functions.
    virtual void handleKeyboardEvent(KeyboardEvent*) OVERRIDE FINAL;
    virtual bool hasValue() const OVERRIDE FINAL;
    virtual void setEmptyValue(const DateComponents& dateForReadOnlyField, EventBehavior = DispatchNoEvent) OVERRIDE FINAL;
    virtual void stepDown() OVERRIDE FINAL;
    virtual void stepUp() OVERRIDE FINAL;
    virtual String value() const OVERRIDE FINAL;
    virtual int valueAsInteger() const OVERRIDE FINAL;
    virtual String visibleValue() const OVERRIDE FINAL;

    const Vector<String> m_symbols;
    int m_selectedIndex;
};

} // namespace WebCore

#endif
#endif
