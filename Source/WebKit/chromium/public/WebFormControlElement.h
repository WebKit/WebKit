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

#ifndef WebFormControlElement_h
#define WebFormControlElement_h

#include "WebElement.h"
#include "WebString.h"

#if WEBKIT_IMPLEMENTATION
namespace WebCore { class HTMLFormControlElement; }
#endif

namespace WebKit {

// Provides readonly access to some properties of a DOM form control element node.
class WebFormControlElement : public WebElement {
public:
    WebFormControlElement() : WebElement() { }
    WebFormControlElement(const WebFormControlElement& e) : WebElement(e) { }

    WebFormControlElement& operator=(const WebFormControlElement& e)
    {
        WebElement::assign(e);
        return *this;
    }
    void assign(const WebFormControlElement& e) { WebElement::assign(e); }

    WEBKIT_API bool isEnabled() const;
    WEBKIT_API bool isReadOnly() const;
    WEBKIT_API WebString formControlName() const;
    WEBKIT_API WebString formControlType() const;

    WEBKIT_API void dispatchFormControlChangeEvent();

    // Returns the name that should be used for the specified |element| when
    // storing autofill data.  This is either the field name or its id, an empty
    // string if it has no name and no id.
    WEBKIT_API WebString nameForAutofill() const;

#if WEBKIT_IMPLEMENTATION
    WebFormControlElement(const WTF::PassRefPtr<WebCore::HTMLFormControlElement>&);
    WebFormControlElement& operator=(const WTF::PassRefPtr<WebCore::HTMLFormControlElement>&);
    operator WTF::PassRefPtr<WebCore::HTMLFormControlElement>() const;
#endif
};

} // namespace WebKit

#endif
