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

    // Provides readonly access to some properties of a DOM input element node.
    class WebInputElement : public WebFormControlElement {
    public:
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
        WEBKIT_API bool isTextField() const;
        // This returns true only for type=text.
        WEBKIT_API bool isText() const;
        WEBKIT_API bool isPasswordField() const;
        WEBKIT_API bool isImageButton() const;
        WEBKIT_API bool autoComplete() const;
        WEBKIT_API int maxLength() const;
        WEBKIT_API bool isActivatedSubmit() const;
        WEBKIT_API void setActivatedSubmit(bool);
        WEBKIT_API int size() const;
        WEBKIT_API void setValue(const WebString&, bool sendChangeEvent = false);
        WEBKIT_API WebString value() const;
        WEBKIT_API void setSuggestedValue(const WebString&);
        WEBKIT_API WebString suggestedValue() const;
        WEBKIT_API void setPlaceholder(const WebString&);
        WEBKIT_API WebString placeholder() const;
        WEBKIT_API bool isAutofilled() const;
        WEBKIT_API void setAutofilled(bool);
        WEBKIT_API void setSelectionRange(int, int);
        WEBKIT_API int selectionStart() const;
        WEBKIT_API int selectionEnd() const;
        WEBKIT_API bool isValidValue(const WebString&) const;
        WEBKIT_API bool isChecked() const;

        // Exposes the default value of the maxLength attribute.
        WEBKIT_API static int defaultMaxLength();

#if WEBKIT_IMPLEMENTATION
        WebInputElement(const WTF::PassRefPtr<WebCore::HTMLInputElement>&);
        WebInputElement& operator=(const WTF::PassRefPtr<WebCore::HTMLInputElement>&);
        operator WTF::PassRefPtr<WebCore::HTMLInputElement>() const;
#endif
    };

    WEBKIT_API WebInputElement* toWebInputElement(WebElement*);

    inline const WebInputElement* toWebInputElement(const WebElement* element)
    {
        return toWebInputElement(const_cast<WebElement*>(element));
    }

} // namespace WebKit

#endif
