/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "HTMLFieldSetElement.h"

#include "HTMLNames.h"
#include "RenderFieldset.h"

namespace WebCore {

using namespace HTMLNames;

HTMLFieldSetElement::HTMLFieldSetElement(Document *doc, HTMLFormElement *f)
   : HTMLFormControlElement(fieldsetTag, doc, f)
{
}

HTMLFieldSetElement::~HTMLFieldSetElement()
{
}

bool HTMLFieldSetElement::checkDTD(const Node* newChild)
{
    return newChild->hasTagName(legendTag) || HTMLElement::checkDTD(newChild);
}

bool HTMLFieldSetElement::isFocusable() const
{
    return HTMLElement::isFocusable();
}

const AtomicString& HTMLFieldSetElement::type() const
{
    static const AtomicString fieldset("fieldset");
    return fieldset;
}

RenderObject* HTMLFieldSetElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    return new (arena) RenderFieldset(this);
}

} // namespace
