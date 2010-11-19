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

#ifndef InputType_h
#define InputType_h

#include "ExceptionCode.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class DateComponents;
class Event;
class FormDataList;
class HTMLInputElement;
class KeyboardEvent;
class MouseEvent;
class RenderArena;
class RenderObject;
class RenderStyle;

// An InputType object represents the type-specific part of an HTMLInputElement.
// Do not expose instances of InputType and classes derived from it to classes
// other than HTMLInputElement.
class InputType : public Noncopyable {
public:
    static PassOwnPtr<InputType> create(HTMLInputElement*, const AtomicString&);
    static PassOwnPtr<InputType> createText(HTMLInputElement*);
    virtual ~InputType();

    // Type query functions

    virtual bool isTextField() const;
    virtual bool isTextType() const;
    virtual const AtomicString& formControlType() const = 0;

    // Form value functions

    virtual bool saveFormControlState(String&) const;
    virtual void restoreFormControlState(const String&) const;
    virtual bool isFormDataAppendable() const;
    virtual bool appendFormData(FormDataList&, bool multipart) const;

    // DOM property functions

    virtual double valueAsDate() const;
    virtual void setValueAsDate(double, ExceptionCode&) const;
    virtual double valueAsNumber() const;
    virtual void setValueAsNumber(double, ExceptionCode&) const;

    // Validation functions

    virtual bool supportsValidation() const;
    virtual bool typeMismatchFor(const String&) const;
    // Type check for the current input value. We do nothing for some types
    // though typeMismatchFor() does something for them because of value
    // sanitization.
    virtual bool typeMismatch() const;
    virtual bool supportsRequired() const;
    virtual bool valueMissing(const String&) const;
    virtual bool patternMismatch(const String&) const;
    virtual bool rangeUnderflow(const String&) const;
    virtual bool rangeOverflow(const String&) const;
    virtual double minimum() const;
    virtual double maximum() const;
    virtual bool stepMismatch(const String&, double) const;
    virtual double stepBase() const;
    virtual double stepBaseWithDecimalPlaces(unsigned*) const;
    virtual double defaultStep() const;
    virtual double stepScaleFactor() const;
    virtual bool parsedStepValueShouldBeInteger() const;
    virtual bool scaledStepValeuShouldBeInteger() const;
    virtual double acceptableError(double) const;
    virtual String typeMismatchText() const;
    virtual String valueMissingText() const;

    // Event handlers
    // If the return value is true, do no further default event handling in the
    // default event handler. If an event handler calls Event::setDefaultHandled(),
    // its return value must be true.

    virtual bool handleClickEvent(MouseEvent*);
    virtual bool handleDOMActivateEvent(Event*);
    virtual bool handleKeydownEvent(KeyboardEvent*);

    // Miscellaneous functions

    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*) const;

    // Parses the specified string for the type, and return
    // the double value for the parsing result if the parsing
    // succeeds; Returns defaultValue otherwise. This function can
    // return NaN or Infinity only if defaultValue is NaN or Infinity.
    virtual double parseToDouble(const String&, double defaultValue) const;
    // Parses the specified string for the type as parseToDouble() does.
    // In addition, it stores the number of digits after the decimal point
    // into *decimalPlaces.
    virtual double parseToDoubleWithDecimalPlaces(const String& src, double defaultValue, unsigned *decimalPlaces) const;
    // Parses the specified string for this InputType, and returns true if it
    // is successfully parsed. An instance pointed by the DateComponents*
    // parameter will have parsed values and be modified even if the parsing
    // fails. The DateComponents* parameter may be 0.
    virtual bool parseToDateComponents(const String&, DateComponents*) const;
    // Create a string representation of the specified double value for the
    // input type. If NaN or Infinity is specified, this returns an empty
    // string. This should not be called for types without valueAsNumber.
    virtual String serialize(double) const;

protected:
    InputType(HTMLInputElement* element) : m_element(element) { }
    HTMLInputElement* element() const { return m_element; }
    // We can't make this a static const data member because VC++ doesn't like it.
    static double defaultStepBase() { return 0.0; }

private:
    // Raw pointer because the HTMLInputElement object owns this InputType object.
    HTMLInputElement* m_element;
};

namespace InputTypeNames {

const AtomicString& button();
const AtomicString& checkbox();
const AtomicString& color();
const AtomicString& date();
const AtomicString& datetime();
const AtomicString& datetimelocal();
const AtomicString& email();
const AtomicString& file();
const AtomicString& hidden();
const AtomicString& image();
const AtomicString& isindex();
const AtomicString& month();
const AtomicString& number();
const AtomicString& password();
const AtomicString& radio();
const AtomicString& range();
const AtomicString& reset();
const AtomicString& search();
const AtomicString& submit();
const AtomicString& telephone();
const AtomicString& text();
const AtomicString& time();
const AtomicString& url();
const AtomicString& week();

} // namespace WebCore::InputTypeNames

} // namespace WebCore

#endif
