/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(DATAGRID)

#include "HTMLDataGridRowElement.h"

#include "HTMLNames.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

HTMLDataGridRowElement::HTMLDataGridRowElement(const QualifiedName& name, Document* doc)
    : HTMLElement(name, doc)
{
}

bool HTMLDataGridRowElement::checkDTD(const Node* newChild)
{
    if (newChild->isTextNode())
        return static_cast<const Text*>(newChild)->containsOnlyWhitespace();
    return newChild->hasTagName(drowTag) || newChild->hasTagName(dcellTag);
}

bool HTMLDataGridRowElement::selected() const
{
    return hasAttribute(selectedAttr);
}

void HTMLDataGridRowElement::setSelected(bool selected)
{
    setAttribute(selectedAttr, selected ? "" : 0);
}

bool HTMLDataGridRowElement::focused() const
{
    return hasAttribute(focusedAttr);
}

void HTMLDataGridRowElement::setFocused(bool focused)
{
    setAttribute(focusedAttr, focused ? "" : 0);
}

bool HTMLDataGridRowElement::expanded() const
{
    return hasAttribute(expandedAttr);
}

void HTMLDataGridRowElement::setExpanded(bool expanded)
{
    setAttribute(expandedAttr, expanded ? "" : 0);
}

} // namespace WebCore

#endif
