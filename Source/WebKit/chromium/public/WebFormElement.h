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

#ifndef WebFormElement_h
#define WebFormElement_h

#include "WebElement.h"
#include "platform/WebVector.h"

#if WEBKIT_IMPLEMENTATION
namespace WebCore { class HTMLFormElement; }
#endif

namespace WebKit {

    class WebInputElement;
    class WebFormControlElement;

    // A container for passing around a reference to a form element.  Provides
    // some information about the form.
    class WebFormElement : public WebElement {
    public:
        ~WebFormElement() { reset(); }

        WebFormElement() : WebElement() { }
        WebFormElement(const WebFormElement& element) : WebElement(element) { }

        WebFormElement& operator=(const WebFormElement& element)
        {
            WebElement::assign(element);
            return *this;
        }
        void assign(const WebFormElement& element) { WebElement::assign(element); }

        WEBKIT_EXPORT bool autoComplete() const;
        WEBKIT_EXPORT WebString action() const;
        WEBKIT_EXPORT WebString name() const;
        WEBKIT_EXPORT WebString method() const;
        WEBKIT_EXPORT bool wasUserSubmitted() const;
        WEBKIT_EXPORT void submit();
        // FIXME: Deprecate and replace with WebVector<WebElement>.
        WEBKIT_EXPORT void getNamedElements(const WebString&, WebVector<WebNode>&);
        WEBKIT_EXPORT void getFormControlElements(WebVector<WebFormControlElement>&) const;

        enum AutocompleteResult {
            AutocompleteResultSuccess,
            AutocompleteResultError, // FIXME: Remove when not used any more.
            AutocompleteResultErrorDisabled,
            AutocompleteResultErrorCancel,
            AutocompleteResultErrorInvalid,
        };
        WEBKIT_EXPORT void finishRequestAutocomplete(WebFormElement::AutocompleteResult);

#if WEBKIT_IMPLEMENTATION
        WebFormElement(const WTF::PassRefPtr<WebCore::HTMLFormElement>&);
        WebFormElement& operator=(const WTF::PassRefPtr<WebCore::HTMLFormElement>&);
        operator WTF::PassRefPtr<WebCore::HTMLFormElement>() const;
#endif
    };

} // namespace WebKit

#endif
