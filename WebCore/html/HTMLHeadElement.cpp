/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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
 */
#include "config.h"
#include "HTMLHeadElement.h"

#include "HTMLNames.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

HTMLHeadElement::HTMLHeadElement(const QualifiedName& qName, Document* doc)
    : HTMLElement(qName, doc)
{
    ASSERT(hasTagName(headTag));
}

HTMLHeadElement::~HTMLHeadElement()
{
}

String HTMLHeadElement::profile() const
{
    return getAttribute(profileAttr);
}

void HTMLHeadElement::setProfile(const String &value)
{
    setAttribute(profileAttr, value);
}

bool HTMLHeadElement::childAllowed(Node* newChild)
{
    // Do not allow non-whitespace text nodes in the head
    if (newChild->isTextNode())
        return static_cast<Text*>(newChild)->containsOnlyWhitespace();
    
    return HTMLElement::childAllowed(newChild);
}

bool HTMLHeadElement::checkDTD(const Node* newChild)
{
    return newChild->hasTagName(noscriptTag) || newChild->hasTagName(titleTag) || newChild->hasTagName(isindexTag) ||
           newChild->hasTagName(baseTag) || newChild->hasTagName(scriptTag) ||
           newChild->hasTagName(styleTag) || newChild->hasTagName(metaTag) ||
           newChild->hasTagName(linkTag) || newChild->isTextNode();
}

}
