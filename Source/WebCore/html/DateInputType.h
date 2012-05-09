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

#ifndef DateInputType_h
#define DateInputType_h

#include "BaseDateAndTimeInputType.h"
#include <wtf/RefPtr.h>

#if ENABLE(INPUT_TYPE_DATE)

namespace WebCore {

class CalendarPickerElement;

class DateInputType : public BaseDateAndTimeInputType {
public:
    static PassOwnPtr<InputType> create(HTMLInputElement*);

private:
    DateInputType(HTMLInputElement*);
    virtual const AtomicString& formControlType() const OVERRIDE;
    virtual DateComponents::Type dateType() const OVERRIDE;
    virtual double minimum() const OVERRIDE;
    virtual double maximum() const OVERRIDE;
    virtual double defaultStep() const OVERRIDE;
    virtual double stepScaleFactor() const OVERRIDE;
    virtual bool parsedStepValueShouldBeInteger() const OVERRIDE;
    virtual bool parseToDateComponentsInternal(const UChar*, unsigned length, DateComponents*) const OVERRIDE;
    virtual bool setMillisecondToDateComponents(double, DateComponents*) const OVERRIDE;
    virtual bool isDateField() const OVERRIDE;
#if ENABLE(CALENDAR_PICKER)
    virtual void createShadowSubtree() OVERRIDE;
    virtual void destroyShadowSubtree() OVERRIDE;
    virtual void handleKeydownEvent(KeyboardEvent*) OVERRIDE;
    virtual void handleBlurEvent() OVERRIDE;
    virtual bool supportsPlaceholder() const OVERRIDE;
    virtual bool usesFixedPlaceholder() const OVERRIDE;
    virtual String fixedPlaceholder() OVERRIDE;

    // TextFieldInputType functions
    virtual bool needsContainer() const OVERRIDE;
    virtual bool shouldHaveSpinButton() const OVERRIDE;

    RefPtr<CalendarPickerElement> m_pickerElement;
#endif
};

} // namespace WebCore
#endif
#endif // DateInputType_h
