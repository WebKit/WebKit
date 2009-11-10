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

#include "config.h"
#include "WebInputElement.h"

#include "HTMLInputElement.h"
#include "WebString.h"
#include <wtf/PassRefPtr.h>

using namespace WebCore;

namespace WebKit {

WebInputElement::WebInputElement(const WTF::PassRefPtr<HTMLInputElement>& elem)
    : WebElement(elem.releaseRef())
{
}

WebInputElement& WebInputElement::operator=(const WTF::PassRefPtr<HTMLInputElement>& elem)
{
    WebNode::assign(elem.releaseRef());
    return *this;
}

WebInputElement::operator WTF::PassRefPtr<HTMLInputElement>() const
{
    return PassRefPtr<HTMLInputElement>(static_cast<HTMLInputElement*>(m_private));
}

void WebInputElement::setActivatedSubmit(bool activated)
{
    unwrap<HTMLInputElement>()->setActivatedSubmit(activated);
}


void WebInputElement::setValue(const WebString& value)
{
    unwrap<HTMLInputElement>()->setValue(value);
}

WebString WebInputElement::value()
{
    return unwrap<HTMLInputElement>()->value();
}


void WebInputElement::setAutofilled(bool autoFilled)
{
    unwrap<HTMLInputElement>()->setAutofilled(autoFilled);
}

void WebInputElement::dispatchFormControlChangeEvent()
{
    unwrap<HTMLInputElement>()->dispatchFormControlChangeEvent();
} // namespace WebKit

void WebInputElement::setSelectionRange(size_t start, size_t end)
{
    unwrap<HTMLInputElement>()->setSelectionRange(start, end);
}
} // namespace WebKit
