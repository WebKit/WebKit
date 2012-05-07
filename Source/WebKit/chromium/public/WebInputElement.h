/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebInputElement_h
#define WebInputElement_h

#include "WebFormControlElement.h"

#if WEBKIT_IMPLEMENTATION
namespace WebCore { class HTMLInputElement; }
#endif

namespace WebKit {

    class WebNodeCollection;

    // Provides readonly access to some properties of a DOM input element node.
    class WebInputElement : public WebFormControlElement {
    public:
        enum SpeechInputState {
            Idle,
            Recording,
            Recognizing,
        };

        WebInputElement() : WebFormControlElement() { }
        WebInputElement(const WebInputElement& element) : WebFormControlElement(element) { }

        WebInputElement& operator=(const WebInputElement& element)
        {
            WebFormControlElement::assign(element);
            return *this;
        }
        void assign(const WebInputElement& element) { WebFormControlElement::assign(element); }

        // This returns true for all of textfield-looking types such as text,
        // password, search, email, url, and number.
        WEBKIT_EXPORT bool isTextField() const;
        // This returns true only for type=text.
        WEBKIT_EXPORT bool isText() const;
        WEBKIT_EXPORT bool isPasswordField() const;
        WEBKIT_EXPORT bool isImageButton() const;
        WEBKIT_EXPORT bool autoComplete() const;
        WEBKIT_EXPORT int maxLength() const;
        WEBKIT_EXPORT bool isActivatedSubmit() const;
        WEBKIT_EXPORT void setActivatedSubmit(bool);
        WEBKIT_EXPORT int size() const;
        WEBKIT_EXPORT void setValue(const WebString&, bool sendChangeEvent = false);
        WEBKIT_EXPORT WebString value() const;
        WEBKIT_EXPORT void setSuggestedValue(const WebString&);
        WEBKIT_EXPORT WebString suggestedValue() const;
        WEBKIT_EXPORT void setPlaceholder(const WebString&);
        WEBKIT_EXPORT WebString placeholder() const;
        WEBKIT_EXPORT bool isAutofilled() const;
        WEBKIT_EXPORT void setAutofilled(bool);
        WEBKIT_EXPORT void setSelectionRange(int, int);
        WEBKIT_EXPORT int selectionStart() const;
        WEBKIT_EXPORT int selectionEnd() const;
        WEBKIT_EXPORT bool isValidValue(const WebString&) const;
        WEBKIT_EXPORT bool isChecked() const;
        WEBKIT_EXPORT bool isMultiple() const;

        WEBKIT_EXPORT WebNodeCollection dataListOptions() const;

        WEBKIT_EXPORT bool isSpeechInputEnabled() const;
        WEBKIT_EXPORT SpeechInputState getSpeechInputState() const;
        WEBKIT_EXPORT void startSpeechInput();
        WEBKIT_EXPORT void stopSpeechInput();

        // Exposes the default value of the maxLength attribute.
        WEBKIT_EXPORT static int defaultMaxLength();

#if WEBKIT_IMPLEMENTATION
        WebInputElement(const WTF::PassRefPtr<WebCore::HTMLInputElement>&);
        WebInputElement& operator=(const WTF::PassRefPtr<WebCore::HTMLInputElement>&);
        operator WTF::PassRefPtr<WebCore::HTMLInputElement>() const;
#endif
    };

    WEBKIT_EXPORT WebInputElement* toWebInputElement(WebElement*);

    inline const WebInputElement* toWebInputElement(const WebElement* element)
    {
        return toWebInputElement(const_cast<WebElement*>(element));
    }

} // namespace WebKit

#endif
