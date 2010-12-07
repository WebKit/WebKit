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
#include "WebFormElement.h"

#include "FormState.h"
#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "WebFormControlElement.h"
#include "WebInputElement.h"
#include "WebString.h"
#include "WebURL.h"
#include <wtf/PassRefPtr.h>

using namespace WebCore;

namespace WebKit {

bool WebFormElement::autoComplete() const
{
    return constUnwrap<HTMLFormElement>()->autoComplete();
}

WebString WebFormElement::action() const
{
    return constUnwrap<HTMLFormElement>()->action();
}

WebString WebFormElement::name() const
{
    return constUnwrap<HTMLFormElement>()->name();
}

WebString WebFormElement::method() const
{
    return constUnwrap<HTMLFormElement>()->method();
}

bool WebFormElement::wasUserSubmitted() const
{
    return constUnwrap<HTMLFormElement>()->wasUserSubmitted();
}

void WebFormElement::submit()
{
    unwrap<HTMLFormElement>()->submit();
}

void WebFormElement::getNamedElements(const WebString& name,
                                      WebVector<WebNode>& result)
{
    Vector<RefPtr<Node> > tempVector;
    unwrap<HTMLFormElement>()->getNamedElements(name, tempVector);
    result.assign(tempVector);
}

void WebFormElement::getFormControlElements(WebVector<WebFormControlElement>& result) const
{
    const HTMLFormElement* form = constUnwrap<HTMLFormElement>();
    Vector<RefPtr<HTMLFormControlElement> > tempVector;
    // FIXME: We should move the for-loop condition into a variable instead of
    // re-evaluating size each time. Also, consider refactoring this code so that
    // we don't call form->associatedElements() multiple times.
    for (size_t i = 0; i < form->associatedElements().size(); i++) {
        if (!form->associatedElements()[i]->isFormControlElement())
            continue;
        HTMLFormControlElement* element = static_cast<HTMLFormControlElement*>(form->associatedElements()[i]);
        if (element->hasLocalName(HTMLNames::inputTag)
            || element->hasLocalName(HTMLNames::selectTag))
            tempVector.append(element);
    }
    result.assign(tempVector);
}

WebFormElement::WebFormElement(const PassRefPtr<HTMLFormElement>& e)
    : WebElement(e)
{
}

WebFormElement& WebFormElement::operator=(const PassRefPtr<HTMLFormElement>& e)
{
    m_private = e;
    return *this;
}

WebFormElement::operator PassRefPtr<HTMLFormElement>() const
{
    return static_cast<HTMLFormElement*>(m_private.get());
}

} // namespace WebKit
