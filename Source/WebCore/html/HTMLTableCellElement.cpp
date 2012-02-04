/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
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
#include "HTMLTableCellElement.h"

#include "Attribute.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLNames.h"
#include "HTMLTableElement.h"
#include "RenderTableCell.h"

using std::max;
using std::min;

namespace WebCore {

// Clamp rowspan at 8k to match Firefox.
static const int maxRowspan = 8190;

using namespace HTMLNames;

inline HTMLTableCellElement::HTMLTableCellElement(const QualifiedName& tagName, Document* document)
    : HTMLTablePartElement(tagName, document)
{
}

PassRefPtr<HTMLTableCellElement> HTMLTableCellElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new HTMLTableCellElement(tagName, document));
}

int HTMLTableCellElement::colSpan() const
{
    const AtomicString& colSpanValue = fastGetAttribute(colspanAttr);
    return max(1, colSpanValue.toInt());
}

int HTMLTableCellElement::rowSpan() const
{
    const AtomicString& rowSpanValue = fastGetAttribute(rowspanAttr);
    return max(1, min(rowSpanValue.toInt(), maxRowspan));
}

int HTMLTableCellElement::cellIndex() const
{
    int index = 0;
    for (const Node * node = previousSibling(); node; node = node->previousSibling()) {
        if (node->hasTagName(tdTag) || node->hasTagName(thTag))
            index++;
    }
    
    return index;
}

void HTMLTableCellElement::parseMappedAttribute(Attribute* attr)
{
    if (attr->name() == rowspanAttr) {
        if (renderer() && renderer()->isTableCell())
            toRenderTableCell(renderer())->colSpanOrRowSpanChanged();
    } else if (attr->name() == colspanAttr) {
        if (renderer() && renderer()->isTableCell())
            toRenderTableCell(renderer())->colSpanOrRowSpanChanged();
    } else if (attr->name() == nowrapAttr) {
        if (attr->isNull())
            removeCSSProperty(CSSPropertyWhiteSpace);
        else
            addCSSProperty(CSSPropertyWhiteSpace, CSSValueWebkitNowrap);

    } else if (attr->name() == widthAttr) {
        if (!attr->value().isEmpty()) {
            int widthInt = attr->value().toInt();
            if (widthInt > 0) // width="0" is ignored for compatibility with WinIE.
                addCSSLength(CSSPropertyWidth, attr->value());
        } else
            removeCSSProperty(CSSPropertyWidth);
    } else if (attr->name() == heightAttr) {
        if (!attr->value().isEmpty()) {
            int heightInt = attr->value().toInt();
            if (heightInt > 0) // height="0" is ignored for compatibility with WinIE.
                addCSSLength(CSSPropertyHeight, attr->value());
        } else
            removeCSSProperty(CSSPropertyHeight);
    } else
        HTMLTablePartElement::parseMappedAttribute(attr);
}

PassRefPtr<StylePropertySet> HTMLTableCellElement::additionalAttributeStyle()
{
    ContainerNode* p = parentNode();
    while (p && !p->hasTagName(tableTag))
        p = p->parentNode();
    if (!p)
        return 0;
    return static_cast<HTMLTableElement*>(p)->additionalCellStyle();
}

bool HTMLTableCellElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == backgroundAttr || HTMLTablePartElement::isURLAttribute(attr);
}

String HTMLTableCellElement::abbr() const
{
    return getAttribute(abbrAttr);
}

String HTMLTableCellElement::axis() const
{
    return getAttribute(axisAttr);
}

void HTMLTableCellElement::setColSpan(int n)
{
    setAttribute(colspanAttr, String::number(n));
}

String HTMLTableCellElement::headers() const
{
    return getAttribute(headersAttr);
}

void HTMLTableCellElement::setRowSpan(int n)
{
    setAttribute(rowspanAttr, String::number(n));
}

String HTMLTableCellElement::scope() const
{
    return getAttribute(scopeAttr);
}

void HTMLTableCellElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    HTMLTablePartElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document()->completeURL(getAttribute(backgroundAttr)));
}

HTMLTableCellElement* HTMLTableCellElement::cellAbove() const
{
    RenderObject* cellRenderer = renderer();
    if (!cellRenderer)
        return 0;
    if (!cellRenderer->isTableCell())
        return 0;

    RenderTableCell* tableCellRenderer = toRenderTableCell(cellRenderer);
    RenderTableCell* cellAboveRenderer = tableCellRenderer->table()->cellAbove(tableCellRenderer);
    if (!cellAboveRenderer)
        return 0;

    return static_cast<HTMLTableCellElement*>(cellAboveRenderer->node());
}

#ifndef NDEBUG

HTMLTableCellElement* toHTMLTableCellElement(Node* node)
{
    ASSERT(!node || node->hasTagName(HTMLNames::tdTag) || node->hasTagName(HTMLNames::thTag));
    return static_cast<HTMLTableCellElement*>(node);
}

const HTMLTableCellElement* toHTMLTableCellElement(const Node* node)
{
    ASSERT(!node || node->hasTagName(HTMLNames::tdTag) || node->hasTagName(HTMLNames::thTag));
    return static_cast<const HTMLTableCellElement*>(node);
}

#endif

} // namespace WebCore
