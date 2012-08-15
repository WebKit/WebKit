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

#ifndef TimeInputType_h
#define TimeInputType_h

#include "BaseDateAndTimeInputType.h"

#if ENABLE(INPUT_TYPE_TIME)

#if ENABLE(INPUT_TYPE_TIME_MULTIPLE_FIELDS)
#include "DateTimeEditElement.h"
#endif

namespace WebCore {

class TimeInputType : public BaseDateAndTimeInputType {
public:
    static PassOwnPtr<InputType> create(HTMLInputElement*);

#if ENABLE(INPUT_TYPE_TIME_MULTIPLE_FIELDS)
    virtual ~TimeInputType();
#endif

private:
    TimeInputType(HTMLInputElement*);
    virtual const AtomicString& formControlType() const OVERRIDE;
    virtual DateComponents::Type dateType() const OVERRIDE;
    virtual Decimal defaultValueForStepUp() const OVERRIDE;
    virtual StepRange createStepRange(AnyStepHandling) const OVERRIDE;
    virtual bool parseToDateComponentsInternal(const UChar*, unsigned length, DateComponents*) const OVERRIDE;
    virtual bool setMillisecondToDateComponents(double, DateComponents*) const OVERRIDE;
    virtual bool isTimeField() const OVERRIDE;

#if ENABLE(INPUT_TYPE_TIME_MULTIPLE_FIELDS)
    class DateTimeEditControlOwnerImpl : public DateTimeEditElement::EditControlOwner {
        WTF_MAKE_NONCOPYABLE(DateTimeEditControlOwnerImpl);

    public:
        DateTimeEditControlOwnerImpl(TimeInputType&);
        virtual ~DateTimeEditControlOwnerImpl();

    private:
        virtual void editControlMouseFocus() OVERRIDE FINAL;
        virtual void editControlValueChanged() OVERRIDE FINAL;
        virtual bool isEditControlOwnerDisabled() const OVERRIDE FINAL;
        virtual bool isEditControlOwnerReadOnly() const OVERRIDE FINAL;

        TimeInputType& m_timeInputType;
    };

    friend class DateTimeEditControlOwnerImpl;

    void updateEditElementLayout();

    // InputType functions
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*) const OVERRIDE FINAL;
    virtual void createShadowSubtree() OVERRIDE FINAL;
    virtual void destroyShadowSubtree() OVERRIDE FINAL;
    virtual void disabledAttributeChanged() OVERRIDE FINAL;
    virtual void forwardEvent(Event*) OVERRIDE FINAL;
    virtual void handleDOMActivateEvent(Event*) OVERRIDE FINAL;
    virtual void handleKeydownEvent(KeyboardEvent*) OVERRIDE FINAL;
    virtual bool isKeyboardFocusable(KeyboardEvent*) const OVERRIDE FINAL;
    virtual bool isMouseFocusable() const OVERRIDE FINAL;
    virtual bool isTextField() const OVERRIDE FINAL;
    virtual void minOrMaxAttributeChanged() OVERRIDE FINAL;
    virtual void readonlyAttributeChanged() OVERRIDE FINAL;
    virtual void setValue(const String&, bool valueChanged, TextFieldEventBehavior) OVERRIDE FINAL;
    virtual bool shouldUseInputMethod() const OVERRIDE FINAL;
    virtual void stepAttributeChanged() OVERRIDE FINAL;
    virtual void updateInnerTextValue() OVERRIDE FINAL;

    DateTimeEditElement* m_dateTimeEditElement;
    DateTimeEditControlOwnerImpl m_dateTimeEditControlOwnerImpl;
#endif
};

} // namespace WebCore

#endif
#endif // TimeInputType_h
