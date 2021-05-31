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

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLTableElement.h"
#include "RenderTableCell.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLTableCellElement);

using namespace HTMLNames;

// These limits are defined in the HTML specification:
// - https://html.spec.whatwg.org/#dom-tdth-colspan
// - https://html.spec.whatwg.org/#dom-tdth-rowspan
static const unsigned minColspan = 1;
static const unsigned maxColspan = 1000;
static const unsigned defaultColspan = 1;
static const unsigned minRowspan = 0;
static const unsigned maxRowspan = 65534;
static const unsigned defaultRowspan = 1;

Ref<HTMLTableCellElement> HTMLTableCellElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLTableCellElement(tagName, document));
}

HTMLTableCellElement::HTMLTableCellElement(const QualifiedName& tagName, Document& document)
    : HTMLTablePartElement(tagName, document)
{
    ASSERT(hasLocalName(thTag->localName()) || hasLocalName(tdTag->localName()));
}

unsigned HTMLTableCellElement::colSpan() const
{
    return clampHTMLNonNegativeIntegerToRange(attributeWithoutSynchronization(colspanAttr), minColspan, maxColspan, defaultColspan);
}

unsigned HTMLTableCellElement::rowSpan() const
{
    // FIXME: a rowSpan equal to 0 should be allowed, and mean that the cell is to span all the remaining rows in the row group.
    return std::max(1u, rowSpanForBindings());
}

unsigned HTMLTableCellElement::rowSpanForBindings() const
{
    return clampHTMLNonNegativeIntegerToRange(attributeWithoutSynchronization(rowspanAttr), minRowspan, maxRowspan, defaultRowspan);
}

int HTMLTableCellElement::cellIndex() const
{
    int index = 0;
    if (!parentElement() || !parentElement()->hasTagName(trTag))
        return -1;

    for (const Node * node = previousSibling(); node; node = node->previousSibling()) {
        if (node->hasTagName(tdTag) || node->hasTagName(thTag))
            index++;
    }
    
    return index;
}

bool HTMLTableCellElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    if (name == nowrapAttr || name == widthAttr || name == heightAttr)
        return true;
    return HTMLTablePartElement::hasPresentationalHintsForAttribute(name);
}

void HTMLTableCellElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == nowrapAttr)
        addPropertyToPresentationalHintStyle(style, CSSPropertyWhiteSpace, CSSValueWebkitNowrap);
    else if (name == widthAttr) {
        if (parseHTMLInteger(value).value_or(0) > 0) // width="0" is ignored for compatibility with WinIE.
            addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    } else if (name == heightAttr) {
        if (parseHTMLInteger(value).value_or(0) > 0) // height="0" is ignored for compatibility with WinIE.
            addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    } else
        HTMLTablePartElement::collectPresentationalHintsForAttribute(name, value, style);
}

void HTMLTableCellElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == rowspanAttr) {
        if (is<RenderTableCell>(renderer()))
            downcast<RenderTableCell>(*renderer()).colSpanOrRowSpanChanged();
    } else if (name == colspanAttr) {
        if (is<RenderTableCell>(renderer()))
            downcast<RenderTableCell>(*renderer()).colSpanOrRowSpanChanged();
    } else
        HTMLTablePartElement::parseAttribute(name, value);
}

const StyleProperties* HTMLTableCellElement::additionalPresentationalHintStyle() const
{
    if (auto table = findParentTable())
        return table->additionalCellStyle();
    return nullptr;
}

bool HTMLTableCellElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == backgroundAttr || HTMLTablePartElement::isURLAttribute(attribute);
}

String HTMLTableCellElement::abbr() const
{
    return attributeWithoutSynchronization(abbrAttr);
}

String HTMLTableCellElement::axis() const
{
    return attributeWithoutSynchronization(axisAttr);
}

void HTMLTableCellElement::setColSpan(unsigned n)
{
    setAttributeWithoutSynchronization(colspanAttr, AtomString::number(limitToOnlyHTMLNonNegative(n, 1)));
}

String HTMLTableCellElement::headers() const
{
    return attributeWithoutSynchronization(headersAttr);
}

void HTMLTableCellElement::setRowSpanForBindings(unsigned n)
{
    setAttributeWithoutSynchronization(rowspanAttr, AtomString::number(limitToOnlyHTMLNonNegative(n, 1)));
}

const AtomString& HTMLTableCellElement::scope() const
{
    // https://html.spec.whatwg.org/multipage/tables.html#attr-th-scope
    static MainThreadNeverDestroyed<const AtomString> row("row", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> col("col", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> rowgroup("rowgroup", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> colgroup("colgroup", AtomString::ConstructFromLiteral);

    const AtomString& value = attributeWithoutSynchronization(HTMLNames::scopeAttr);

    if (equalIgnoringASCIICase(value, row))
        return row;
    if (equalIgnoringASCIICase(value, col))
        return col;
    if (equalIgnoringASCIICase(value, rowgroup))
        return rowgroup;
    if (equalIgnoringASCIICase(value, colgroup))
        return colgroup;
    return emptyAtom();
}

void HTMLTableCellElement::setScope(const AtomString& scope)
{
    setAttributeWithoutSynchronization(scopeAttr, scope);
}

void HTMLTableCellElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    HTMLTablePartElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document().completeURL(attributeWithoutSynchronization(backgroundAttr)));
}

HTMLTableCellElement* HTMLTableCellElement::cellAbove() const
{
    auto* cellRenderer = renderer();
    if (!is<RenderTableCell>(cellRenderer))
        return nullptr;

    auto& tableCellRenderer = downcast<RenderTableCell>(*cellRenderer);
    auto* cellAboveRenderer = tableCellRenderer.table()->cellAbove(&tableCellRenderer);
    if (!cellAboveRenderer)
        return nullptr;

    return downcast<HTMLTableCellElement>(cellAboveRenderer->element());
}

} // namespace WebCore
