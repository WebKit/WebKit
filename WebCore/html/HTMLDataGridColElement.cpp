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
#include "HTMLDataGridColElement.h"

#include "HTMLNames.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

HTMLDataGridColElement::HTMLDataGridColElement(const QualifiedName& name, Document* doc)
    : HTMLElement(name, doc)
{
}

String HTMLDataGridColElement::label() const
{
    return getAttribute(labelAttr);
}

void HTMLDataGridColElement::setLabel(const String& label)
{
    setAttribute(labelAttr, label);
}

String HTMLDataGridColElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLDataGridColElement::setType(const String& type)
{
    setAttribute(typeAttr, type);
}

unsigned short HTMLDataGridColElement::sortable() const
{
    return hasAttribute(sortableAttr);
}

void HTMLDataGridColElement::setSortable(unsigned short sortable)
{
    setAttribute(sortableAttr, sortable ? "" : 0);
}

unsigned short HTMLDataGridColElement::sortDirection() const
{
    String sortDirection = getAttribute(sortdirectionAttr);
    if (equalIgnoringCase(sortDirection, "ascending"))
        return 1;
    if (equalIgnoringCase(sortDirection, "descending"))
        return 2;
    return 0;
}

void HTMLDataGridColElement::setSortDirection(unsigned short sortDirection)
{
    // FIXME: Check sortable rules.
    if (sortDirection == 0)
        setAttribute(sortdirectionAttr, "natural");
    else if (sortDirection == 1)
        setAttribute(sortdirectionAttr, "ascending");
    else if (sortDirection == 2)
        setAttribute(sortdirectionAttr, "descending");
}

bool HTMLDataGridColElement::primary() const
{
    return hasAttribute(primaryAttr);
}

void HTMLDataGridColElement::setPrimary(bool primary)
{
    setAttribute(primaryAttr, primary ? "" : 0);
}

}

