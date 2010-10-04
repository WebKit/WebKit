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

#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class DateComponents;
class HTMLInputElement;

class InputType : public Noncopyable {
public:
    static PassOwnPtr<InputType> create(HTMLInputElement*, const AtomicString&);
    static PassOwnPtr<InputType> createText(HTMLInputElement*);
    virtual ~InputType();

    virtual bool isTextField() const;
    virtual bool isTextType() const;
    virtual const AtomicString& formControlType() const = 0;

    virtual bool patternMismatch(const String&) const;

    // Parses the specified string for the type, and return
    // the double value for the parsing result if the parsing
    // succeeds; Returns defaultValue otherwise. This function can
    // return NaN or Infinity only if defaultValue is NaN or Infinity.
    virtual double parseToDouble(const String&, double defaultValue) const;
    // Parses the specified string for this InputType, and returns true if it
    // is successfully parsed. An instance pointed by the DateComponents*
    // parameter will have parsed values and be modified even if the parsing
    // fails. The DateComponents* parameter may be 0.
    virtual bool parseToDateComponents(const String&, DateComponents*) const;

protected:
    InputType(HTMLInputElement* element) : m_element(element) { }
    HTMLInputElement* element() const { return m_element; }

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
