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

#include "config.h"
#include "WebFormControlElement.h"

#include "HTMLFormControlElement.h"
#include <wtf/PassRefPtr.h>

using namespace WebCore;

namespace WebKit {

bool WebFormControlElement::isEnabled() const
{
    return constUnwrap<HTMLFormControlElement>()->isEnabledFormControl();
}

WebString WebFormControlElement::formControlName() const
{
    return constUnwrap<HTMLFormControlElement>()->formControlName();
}

WebString WebFormControlElement::formControlType() const
{
    return constUnwrap<HTMLFormControlElement>()->formControlType();
}

WebString WebFormControlElement::nameForAutofill() const
{
    String name = constUnwrap<HTMLFormControlElement>()->name();
    String trimmedName = name.stripWhiteSpace();
    if (!trimmedName.isEmpty())
        return trimmedName;
    name = constUnwrap<HTMLFormControlElement>()->getAttribute(HTMLNames::idAttr);
    trimmedName = name.stripWhiteSpace();
    if (!trimmedName.isEmpty())
        return trimmedName;
    return String();
}

WebFormControlElement::WebFormControlElement(const PassRefPtr<HTMLFormControlElement>& elem)
    : WebElement(elem)
{
}

WebFormControlElement& WebFormControlElement::operator=(const PassRefPtr<HTMLFormControlElement>& elem)
{
    m_private = elem;
    return *this;
}

WebFormControlElement::operator PassRefPtr<HTMLFormControlElement>() const
{
    return static_cast<HTMLFormControlElement*>(m_private.get());
}

} // namespace WebKit
