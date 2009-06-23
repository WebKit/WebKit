/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
#include "HTMLTableColElement.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "HTMLTableElement.h"
#include "MappedAttribute.h"
#include "RenderTableCol.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

HTMLTableColElement::HTMLTableColElement(const QualifiedName& tagName, Document *doc)
    : HTMLTablePartElement(tagName, doc)
{
    _span = 1;
}

HTMLTagStatus HTMLTableColElement::endTagRequirement() const
{
    return hasLocalName(colTag) ? TagStatusForbidden : TagStatusOptional;
}

int HTMLTableColElement::tagPriority() const
{
    return hasLocalName(colTag) ? 0 : 1;
}

bool HTMLTableColElement::checkDTD(const Node* newChild)
{
    if (hasLocalName(colTag))
        return false;
    
    if (newChild->isTextNode())
        return static_cast<const Text*>(newChild)->containsOnlyWhitespace();
    return newChild->hasTagName(colTag);
}

bool HTMLTableColElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == widthAttr) {
        result = eUniversal;
        return false;
    }

    return HTMLTablePartElement::mapToEntry(attrName, result);
}

void HTMLTableColElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == spanAttr) {
        _span = !attr->isNull() ? attr->value().toInt() : 1;
        if (renderer() && renderer()->isTableCol())
            static_cast<RenderTableCol*>(renderer())->updateFromElement();
    } else if (attr->name() == widthAttr) {
        if (!attr->value().isEmpty()) {
            addCSSLength(attr, CSSPropertyWidth, attr->value());
            if (renderer() && renderer()->isTableCol()) {
                RenderTableCol* col = static_cast<RenderTableCol*>(renderer());
                int newWidth = width().toInt();
                if (newWidth != col->width())
                    col->setNeedsLayoutAndPrefWidthsRecalc();
            }
        }
    } else
        HTMLTablePartElement::parseMappedAttribute(attr);
}

// used by table columns and column groups to share style decls created by the enclosing table.
void HTMLTableColElement::additionalAttributeStyleDecls(Vector<CSSMutableStyleDeclaration*>& results)
{
    if (!hasLocalName(colgroupTag))
        return;
    Node* p = parentNode();
    while (p && !p->hasTagName(tableTag))
        p = p->parentNode();
    if (!p)
        return;
    static_cast<HTMLTableElement*>(p)->addSharedGroupDecls(false, results);
}

String HTMLTableColElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLTableColElement::setAlign(const String &value)
{
    setAttribute(alignAttr, value);
}

String HTMLTableColElement::ch() const
{
    return getAttribute(charAttr);
}

void HTMLTableColElement::setCh(const String &value)
{
    setAttribute(charAttr, value);
}

String HTMLTableColElement::chOff() const
{
    return getAttribute(charoffAttr);
}

void HTMLTableColElement::setChOff(const String &value)
{
    setAttribute(charoffAttr, value);
}

void HTMLTableColElement::setSpan(int n)
{
    setAttribute(spanAttr, String::number(n));
}

String HTMLTableColElement::vAlign() const
{
    return getAttribute(valignAttr);
}

void HTMLTableColElement::setVAlign(const String &value)
{
    setAttribute(valignAttr, value);
}

String HTMLTableColElement::width() const
{
    return getAttribute(widthAttr);
}

void HTMLTableColElement::setWidth(const String &value)
{
    setAttribute(widthAttr, value);
}

}
