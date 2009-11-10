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
#include "DOMUtilitiesPrivate.h"

#include "Element.h"
#include "HTMLInputElement.h"
#include "HTMLLinkElement.h"
#include "HTMLMetaElement.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "Node.h"

using namespace WebCore;

namespace {

template <class HTMLNodeType>
HTMLNodeType* toHTMLElement(Node* node, const QualifiedName& name)
{
    if (node->isHTMLElement()
        && static_cast<HTMLElement*>(node)->hasTagName(name)) {
        return static_cast<HTMLNodeType*>(node);
    }
    return 0;
}

} // namespace

namespace WebKit {

HTMLInputElement* toHTMLInputElement(Node* node)
{
    return toHTMLElement<HTMLInputElement>(node, HTMLNames::inputTag);
}

HTMLLinkElement* toHTMLLinkElement(Node* node)
{
    return toHTMLElement<HTMLLinkElement>(node, HTMLNames::linkTag);
}

HTMLMetaElement* toHTMLMetaElement(Node* node)
{
    return toHTMLElement<HTMLMetaElement>(node, HTMLNames::metaTag);
}

HTMLOptionElement* toHTMLOptionElement(Node* node)
{
    return toHTMLElement<HTMLOptionElement>(node, HTMLNames::optionTag);
}

String nameOfInputElement(HTMLInputElement* element)
{
    String name = element->name();
    String trimmedName = name.stripWhiteSpace();
    if (!trimmedName.isEmpty())
        return trimmedName;

    name = element->getAttribute(HTMLNames::idAttr);
    trimmedName = name.stripWhiteSpace();
    if (!trimmedName.isEmpty())
        return trimmedName;

    return String();
}

} // namespace WebKit
