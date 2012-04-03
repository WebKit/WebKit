/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef WebTextFieldDecoratorClient_h
#define WebTextFieldDecoratorClient_h

#include "platform/WebCString.h"

namespace WebKit {

class WebInputElement;

class WebTextFieldDecoratorClient {
public:
    // The function should return true if the specified input element should
    // have a decoration icon. This function is called whenever a text field is
    // created, and should not take much time.
    virtual bool shouldAddDecorationTo(const WebInputElement&) = 0;

    // Image resource name for the normal state. The image is stretched to
    // font-size x font-size square. The function must return an existing
    // resource name.
    virtual WebCString imageNameForNormalState() = 0;
    // Image resource name for the disabled state. If this function returns an
    // empty string, imageNameForNormalState() is used even for the disabled
    // state.
    virtual WebCString imageNameForDisabledState() = 0;
    // Image resource name for the disabled state. If this function returns an
    // empty string, the image same as imageNameForDisabledState() is used.
    virtual WebCString imageNameForReadOnlyState() = 0;

    // This is called when the decoration icon is clicked.
    virtual void handleClick(WebInputElement&) = 0;
    // This is called when the input element loses its renderer. An
    // implementation of this function should not do something which updates
    // state of WebKit objects.
    virtual void willDetach(const WebInputElement&) = 0;

    virtual ~WebTextFieldDecoratorClient() { }
};

}

#endif // WebTextFieldDecoratorClient_h
