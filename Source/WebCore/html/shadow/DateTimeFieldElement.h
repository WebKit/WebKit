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

#ifndef DateTimeFieldElement_h
#define DateTimeFieldElement_h

#if ENABLE(INPUT_TYPE_TIME_MULTIPLE_FIELDS)

#include "HTMLDivElement.h"

namespace WebCore {

class DateComponents;

// DateTimeFieldElement is base class of date time field element.
class DateTimeFieldElement : public HTMLElement {
    WTF_MAKE_NONCOPYABLE(DateTimeFieldElement);

public:
    enum EventBehavior {
        DispatchNoEvent,
        DispatchEvent,
    };

    // FieldEventHandler implementer must call removeEventHandler when
    // it doesn't handle event, e.g. at destruction.
    class FieldEventHandler {
    public:
        virtual ~FieldEventHandler();
        virtual void fieldValueChanged() = 0;
        virtual void focusOnNextField() = 0;
    };

    virtual void defaultEventHandler(Event*) OVERRIDE;
    virtual bool hasValue() const = 0;
    bool isReadOnly() const;
    void removeEventHandler() { m_fieldEventHandler = 0; }
    void setReadOnly();
    virtual void setEmptyValue(const DateComponents& dateForReadOnlyField, EventBehavior = DispatchNoEvent) = 0;
    virtual void setValueAsDate(const DateComponents&) = 0;
    virtual void setValueAsInteger(int, EventBehavior = DispatchNoEvent) = 0;
    virtual void stepDown() = 0;
    virtual void stepUp() = 0;
    virtual String value() const = 0;
    double valueAsDouble() const;
    virtual int valueAsInteger() const = 0;
    virtual String visibleValue() const = 0;

protected:
    DateTimeFieldElement(Document*, FieldEventHandler&);
    void focusOnNextField();
    virtual void handleKeyboardEvent(KeyboardEvent*) = 0;
    void initialize(const AtomicString&);
    virtual double unitInMillisecond() const = 0;
    void updateVisibleValue(EventBehavior);

private:
    void defaultKeyboardEventHandler(KeyboardEvent*);

    FieldEventHandler* m_fieldEventHandler;
};

} // namespace WebCore

#endif
#endif
