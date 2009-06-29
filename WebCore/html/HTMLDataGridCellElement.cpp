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

#include "HTMLDataGridCellElement.h"

#include "HTMLNames.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

HTMLDataGridCellElement::HTMLDataGridCellElement(const QualifiedName& name, Document* doc)
    : HTMLElement(name, doc)
{
}

String HTMLDataGridCellElement::label() const
{
    return getAttribute(labelAttr);
}

void HTMLDataGridCellElement::setLabel(const String& label)
{
    setAttribute(labelAttr, label);
}

bool HTMLDataGridCellElement::focused() const
{
    return hasAttribute(focusedAttr);
}

void HTMLDataGridCellElement::setFocused(bool focused)
{
    setAttribute(focusedAttr, focused ? "" : 0);
}

bool HTMLDataGridCellElement::checked() const
{
    return hasAttribute(checkedAttr);
}

void HTMLDataGridCellElement::setChecked(bool checked)
{
    setAttribute(checkedAttr, checked ? "" : 0);
}

bool HTMLDataGridCellElement::indeterminate() const
{
    return hasAttribute(indeterminateAttr);
}

void HTMLDataGridCellElement::setIndeterminate(bool indeterminate)
{
    setAttribute(indeterminateAttr, indeterminate ? "" : 0);
}

float HTMLDataGridCellElement::progress() const
{
    return getAttribute(progressAttr).toInt();
}

void HTMLDataGridCellElement::setProgress(float progress)
{
    setAttribute(progressAttr, String::number(progress));
}

} // namespace WebCore

#endif
