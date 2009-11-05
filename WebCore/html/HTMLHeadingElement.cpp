/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
#include "HTMLHeadingElement.h"

#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

HTMLHeadingElement::HTMLHeadingElement(const QualifiedName& tagName, Document *doc)
    : HTMLElement(tagName, doc)
{
}

bool HTMLHeadingElement::checkDTD(const Node* newChild)
{
    if (newChild->hasTagName(h1Tag) || newChild->hasTagName(h2Tag) ||
        newChild->hasTagName(h3Tag) || newChild->hasTagName(h4Tag) ||
        newChild->hasTagName(h5Tag) || newChild->hasTagName(h6Tag))
        return false;

    return inEitherTagList(newChild);
}

String HTMLHeadingElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLHeadingElement::setAlign(const String &value)
{
    setAttribute(alignAttr, value);
}

}
