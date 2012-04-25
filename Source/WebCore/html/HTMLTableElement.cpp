/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2010, 2011 Apple Inc. All rights reserved.
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
#include "HTMLTableElement.h"

#include "Attribute.h"
#include "CSSImageValue.h"
#include "CSSPropertyNames.h"
#include "CSSStyleSheet.h"
#include "CSSValueKeywords.h"
#include "CSSValuePool.h"
#include "ExceptionCode.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLTableCaptionElement.h"
#include "HTMLTableRowElement.h"
#include "HTMLTableRowsCollection.h"
#include "HTMLTableSectionElement.h"
#include "RenderTable.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

HTMLTableElement::HTMLTableElement(const QualifiedName& tagName, Document* document)
    : HTMLElement(tagName, document)
    , m_borderAttr(false)
    , m_borderColorAttr(false)
    , m_frameAttr(false)
    , m_rulesAttr(UnsetRules)
    , m_padding(1)
{
    ASSERT(hasTagName(tableTag));
}

PassRefPtr<HTMLTableElement> HTMLTableElement::create(Document* document)
{
    return adoptRef(new HTMLTableElement(tableTag, document));
}

PassRefPtr<HTMLTableElement> HTMLTableElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new HTMLTableElement(tagName, document));
}

HTMLTableCaptionElement* HTMLTableElement::caption() const
{
    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(captionTag))
            return static_cast<HTMLTableCaptionElement*>(child);
    }
    return 0;
}

void HTMLTableElement::setCaption(PassRefPtr<HTMLTableCaptionElement> newCaption, ExceptionCode& ec)
{
    deleteCaption();
    insertBefore(newCaption, firstChild(), ec);
}

HTMLTableSectionElement* HTMLTableElement::tHead() const
{
    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(theadTag))
            return static_cast<HTMLTableSectionElement*>(child);
    }
    return 0;
}

void HTMLTableElement::setTHead(PassRefPtr<HTMLTableSectionElement> newHead, ExceptionCode& ec)
{
    deleteTHead();

    Node* child;
    for (child = firstChild(); child; child = child->nextSibling())
        if (child->isElementNode() && !child->hasTagName(captionTag) && !child->hasTagName(colgroupTag))
            break;

    insertBefore(newHead, child, ec);
}

HTMLTableSectionElement* HTMLTableElement::tFoot() const
{
    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(tfootTag))
            return static_cast<HTMLTableSectionElement*>(child);
    }
    return 0;
}

void HTMLTableElement::setTFoot(PassRefPtr<HTMLTableSectionElement> newFoot, ExceptionCode& ec)
{
    deleteTFoot();

    Node* child;
    for (child = firstChild(); child; child = child->nextSibling())
        if (child->isElementNode() && !child->hasTagName(captionTag) && !child->hasTagName(colgroupTag) && !child->hasTagName(theadTag))
            break;

    insertBefore(newFoot, child, ec);
}

PassRefPtr<HTMLElement> HTMLTableElement::createTHead()
{
    if (HTMLTableSectionElement* existingHead = tHead())
        return existingHead;
    RefPtr<HTMLTableSectionElement> head = HTMLTableSectionElement::create(theadTag, document());
    ExceptionCode ec;
    setTHead(head, ec);
    return head.release();
}

void HTMLTableElement::deleteTHead()
{
    ExceptionCode ec;
    removeChild(tHead(), ec);
}

PassRefPtr<HTMLElement> HTMLTableElement::createTFoot()
{
    if (HTMLTableSectionElement* existingFoot = tFoot())
        return existingFoot;
    RefPtr<HTMLTableSectionElement> foot = HTMLTableSectionElement::create(tfootTag, document());
    ExceptionCode ec;
    setTFoot(foot, ec);
    return foot.release();
}

void HTMLTableElement::deleteTFoot()
{
    ExceptionCode ec;
    removeChild(tFoot(), ec);
}

PassRefPtr<HTMLElement> HTMLTableElement::createTBody()
{
    RefPtr<HTMLTableSectionElement> body = HTMLTableSectionElement::create(tbodyTag, document());
    Node* referenceElement = lastBody() ? lastBody()->nextSibling() : 0;
    insertBefore(body, referenceElement, ASSERT_NO_EXCEPTION);
    return body.release();
}

PassRefPtr<HTMLElement> HTMLTableElement::createCaption()
{
    if (HTMLTableCaptionElement* existingCaption = caption())
        return existingCaption;
    RefPtr<HTMLTableCaptionElement> caption = HTMLTableCaptionElement::create(captionTag, document());
    ExceptionCode ec;
    setCaption(caption, ec);
    return caption.release();
}

void HTMLTableElement::deleteCaption()
{
    ExceptionCode ec;
    removeChild(caption(), ec);
}

HTMLTableSectionElement* HTMLTableElement::lastBody() const
{
    for (Node* child = lastChild(); child; child = child->previousSibling()) {
        if (child->hasTagName(tbodyTag))
            return static_cast<HTMLTableSectionElement*>(child);
    }
    return 0;
}

PassRefPtr<HTMLElement> HTMLTableElement::insertRow(int index, ExceptionCode& ec)
{
    if (index < -1) {
        ec = INDEX_SIZE_ERR;
        return 0;
    }

    RefPtr<Node> protectFromMutationEvents(this);

    RefPtr<HTMLTableRowElement> lastRow = 0;
    RefPtr<HTMLTableRowElement> row = 0;
    if (index == -1)
        lastRow = HTMLTableRowsCollection::lastRow(this);
    else {
        for (int i = 0; i <= index; ++i) {
            row = HTMLTableRowsCollection::rowAfter(this, lastRow.get());
            if (!row) {
                if (i != index) {
                    ec = INDEX_SIZE_ERR;
                    return 0;
                }
                break;
            }
            lastRow = row;
        }
    }

    RefPtr<ContainerNode> parent;
    if (lastRow)
        parent = row ? row->parentNode() : lastRow->parentNode();
    else {
        parent = lastBody();
        if (!parent) {
            RefPtr<HTMLTableSectionElement> newBody = HTMLTableSectionElement::create(tbodyTag, document());
            RefPtr<HTMLTableRowElement> newRow = HTMLTableRowElement::create(document());
            newBody->appendChild(newRow, ec);
            appendChild(newBody.release(), ec);
            return newRow.release();
        }
    }

    RefPtr<HTMLTableRowElement> newRow = HTMLTableRowElement::create(document());
    parent->insertBefore(newRow, row.get(), ec);
    return newRow.release();
}

void HTMLTableElement::deleteRow(int index, ExceptionCode& ec)
{
    HTMLTableRowElement* row = 0;
    if (index == -1)
        row = HTMLTableRowsCollection::lastRow(this);
    else {
        for (int i = 0; i <= index; ++i) {
            row = HTMLTableRowsCollection::rowAfter(this, row);
            if (!row)
                break;
        }
    }
    if (!row) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    row->remove(ec);
}

static inline bool isTableCellAncestor(Node* n)
{
    return n->hasTagName(theadTag) || n->hasTagName(tbodyTag) ||
           n->hasTagName(tfootTag) || n->hasTagName(trTag) ||
           n->hasTagName(thTag);
}

static bool setTableCellsChanged(Node* n)
{
    ASSERT(n);
    bool cellChanged = false;

    if (n->hasTagName(tdTag))
        cellChanged = true;
    else if (isTableCellAncestor(n)) {
        for (Node* child = n->firstChild(); child; child = child->nextSibling())
            cellChanged |= setTableCellsChanged(child);
    }

    if (cellChanged)
       n->setNeedsStyleRecalc();

    return cellChanged;
}

static bool getBordersFromFrameAttributeValue(const AtomicString& value, bool& borderTop, bool& borderRight, bool& borderBottom, bool& borderLeft)
{
    borderTop = false;
    borderRight = false;
    borderBottom = false;
    borderLeft = false;

    if (equalIgnoringCase(value, "above"))
        borderTop = true;
    else if (equalIgnoringCase(value, "below"))
        borderBottom = true;
    else if (equalIgnoringCase(value, "hsides"))
        borderTop = borderBottom = true;
    else if (equalIgnoringCase(value, "vsides"))
        borderLeft = borderRight = true;
    else if (equalIgnoringCase(value, "lhs"))
        borderLeft = true;
    else if (equalIgnoringCase(value, "rhs"))
        borderRight = true;
    else if (equalIgnoringCase(value, "box") || equalIgnoringCase(value, "border"))
        borderTop = borderBottom = borderLeft = borderRight = true;
    else if (!equalIgnoringCase(value, "void"))
        return false;
    return true;
}

void HTMLTableElement::collectStyleForAttribute(Attribute* attr, StylePropertySet* style)
{
    if (attr->name() == widthAttr)
        addHTMLLengthToStyle(style, CSSPropertyWidth, attr->value());
    else if (attr->name() == heightAttr)
        addHTMLLengthToStyle(style, CSSPropertyHeight, attr->value());
    else if (attr->name() == borderAttr) {
        int borderWidth = attr->isEmpty() ? 1 : attr->value().toInt();
        addPropertyToAttributeStyle(style, CSSPropertyBorderWidth, borderWidth, CSSPrimitiveValue::CSS_PX);
    } else if (attr->name() == bordercolorAttr) {
        if (!attr->isEmpty())
            addHTMLColorToStyle(style, CSSPropertyBorderColor, attr->value());
    } else if (attr->name() == bgcolorAttr)
        addHTMLColorToStyle(style, CSSPropertyBackgroundColor, attr->value());
    else if (attr->name() == backgroundAttr) {
        String url = stripLeadingAndTrailingHTMLSpaces(attr->value());
        if (!url.isEmpty())
            style->setProperty(CSSProperty(CSSPropertyBackgroundImage, CSSImageValue::create(document()->completeURL(url).string())));
    } else if (attr->name() == valignAttr) {
        if (!attr->isEmpty())
            addPropertyToAttributeStyle(style, CSSPropertyVerticalAlign, attr->value());
    } else if (attr->name() == cellspacingAttr) {
        if (!attr->isEmpty())
            addHTMLLengthToStyle(style, CSSPropertyBorderSpacing, attr->value());
    } else if (attr->name() == vspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginTop, attr->value());
        addHTMLLengthToStyle(style, CSSPropertyMarginBottom, attr->value());
    } else if (attr->name() == hspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginLeft, attr->value());
        addHTMLLengthToStyle(style, CSSPropertyMarginRight, attr->value());
    } else if (attr->name() == alignAttr) {
        if (!attr->value().isEmpty()) {
            if (equalIgnoringCase(attr->value(), "center")) {
                addPropertyToAttributeStyle(style, CSSPropertyWebkitMarginStart, CSSValueAuto);
                addPropertyToAttributeStyle(style, CSSPropertyWebkitMarginEnd, CSSValueAuto);
            } else
                addPropertyToAttributeStyle(style, CSSPropertyFloat, attr->value());
        }
    } else if (attr->name() == rulesAttr) {
        // The presence of a valid rules attribute causes border collapsing to be enabled.
        if (m_rulesAttr != UnsetRules)
            addPropertyToAttributeStyle(style, CSSPropertyBorderCollapse, CSSValueCollapse);
    } else if (attr->name() == frameAttr) {
        bool borderTop;
        bool borderRight;
        bool borderBottom;
        bool borderLeft;
        if (getBordersFromFrameAttributeValue(attr->value(), borderTop, borderRight, borderBottom, borderLeft)) {
            addPropertyToAttributeStyle(style, CSSPropertyBorderWidth, CSSValueThin);
            addPropertyToAttributeStyle(style, CSSPropertyBorderTopStyle, borderTop ? CSSValueSolid : CSSValueHidden);
            addPropertyToAttributeStyle(style, CSSPropertyBorderBottomStyle, borderBottom ? CSSValueSolid : CSSValueHidden);
            addPropertyToAttributeStyle(style, CSSPropertyBorderLeftStyle, borderLeft ? CSSValueSolid : CSSValueHidden);
            addPropertyToAttributeStyle(style, CSSPropertyBorderRightStyle, borderRight ? CSSValueSolid : CSSValueHidden);
        }
    } else
        HTMLElement::collectStyleForAttribute(attr, style);
}

bool HTMLTableElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr || name == bgcolorAttr || name == backgroundAttr || name == valignAttr || name == vspaceAttr || name == hspaceAttr || name == alignAttr || name == cellspacingAttr || name == borderAttr || name == bordercolorAttr || name == frameAttr || name == rulesAttr)
        return true;
    return HTMLElement::isPresentationAttribute(name);
}

void HTMLTableElement::parseAttribute(Attribute* attr)
{
    CellBorders bordersBefore = cellBorders();
    unsigned short oldPadding = m_padding;

    if (attr->name() == borderAttr)  {
        // FIXME: This attribute is a mess.
        m_borderAttr = true;
        if (!attr->isNull()) {
            int border = attr->isEmpty() ? 1 : attr->value().toInt();
            m_borderAttr = border;
        }
    } else if (attr->name() == bordercolorAttr) {
        m_borderColorAttr = !attr->isEmpty();
    } else if (attr->name() == frameAttr) {
        // FIXME: This attribute is a mess.
        bool borderTop;
        bool borderRight;
        bool borderBottom;
        bool borderLeft;
        m_frameAttr = getBordersFromFrameAttributeValue(attr->value(), borderTop, borderRight, borderBottom, borderLeft);
    } else if (attr->name() == rulesAttr) {
        m_rulesAttr = UnsetRules;
        if (equalIgnoringCase(attr->value(), "none"))
            m_rulesAttr = NoneRules;
        else if (equalIgnoringCase(attr->value(), "groups"))
            m_rulesAttr = GroupsRules;
        else if (equalIgnoringCase(attr->value(), "rows"))
            m_rulesAttr = RowsRules;
        if (equalIgnoringCase(attr->value(), "cols"))
            m_rulesAttr = ColsRules;
        if (equalIgnoringCase(attr->value(), "all"))
            m_rulesAttr = AllRules;
    } else if (attr->name() == cellpaddingAttr) {
        if (!attr->value().isEmpty())
            m_padding = max(0, attr->value().toInt());
        else
            m_padding = 1;
    } else if (attr->name() == colsAttr) {
        // ###
    } else
        HTMLElement::parseAttribute(attr);

    if (bordersBefore != cellBorders() || oldPadding != m_padding) {
        m_sharedCellStyle = 0;
        bool cellChanged = false;
        for (Node* child = firstChild(); child; child = child->nextSibling())
            cellChanged |= setTableCellsChanged(child);
        if (cellChanged)
            setNeedsStyleRecalc();
    }
}

static StylePropertySet* leakBorderStyle(int value)
{
    RefPtr<StylePropertySet> style = StylePropertySet::create();
    style->setProperty(CSSPropertyBorderTopStyle, value);
    style->setProperty(CSSPropertyBorderBottomStyle, value);
    style->setProperty(CSSPropertyBorderLeftStyle, value);
    style->setProperty(CSSPropertyBorderRightStyle, value);
    return style.release().leakRef();
}

StylePropertySet* HTMLTableElement::additionalAttributeStyle()
{
    if (m_frameAttr)
        return 0;
    
    if (!m_borderAttr && !m_borderColorAttr) {
        // Setting the border to 'hidden' allows it to win over any border
        // set on the table's cells during border-conflict resolution.
        if (m_rulesAttr != UnsetRules) {
            static StylePropertySet* solidBorderStyle = leakBorderStyle(CSSValueHidden);
            return solidBorderStyle;
        }
        return 0;
    }

    if (m_borderColorAttr) {
        static StylePropertySet* solidBorderStyle = leakBorderStyle(CSSValueSolid);
        return solidBorderStyle;
    }
    static StylePropertySet* outsetBorderStyle = leakBorderStyle(CSSValueOutset);
    return outsetBorderStyle;
}

HTMLTableElement::CellBorders HTMLTableElement::cellBorders() const
{
    switch (m_rulesAttr) {
        case NoneRules:
        case GroupsRules:
            return NoBorders;
        case AllRules:
            return SolidBorders;
        case ColsRules:
            return SolidBordersColsOnly;
        case RowsRules:
            return SolidBordersRowsOnly;
        case UnsetRules:
            if (!m_borderAttr)
                return NoBorders;
            if (m_borderColorAttr)
                return SolidBorders;
            return InsetBorders;
    }
    ASSERT_NOT_REACHED();
    return NoBorders;
}

PassRefPtr<StylePropertySet> HTMLTableElement::createSharedCellStyle()
{
    RefPtr<StylePropertySet> style = StylePropertySet::create();

    switch (cellBorders()) {
    case SolidBordersColsOnly:
        style->setProperty(CSSPropertyBorderLeftWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderRightWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderLeftStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderRightStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderColor, cssValuePool().createInheritedValue());
        break;
    case SolidBordersRowsOnly:
        style->setProperty(CSSPropertyBorderTopWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderBottomWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderTopStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderBottomStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderColor, cssValuePool().createInheritedValue());
        break;
    case SolidBorders:
        style->setProperty(CSSPropertyBorderWidth, cssValuePool().createValue(1, CSSPrimitiveValue::CSS_PX));
        style->setProperty(CSSPropertyBorderStyle, cssValuePool().createIdentifierValue(CSSValueSolid));
        style->setProperty(CSSPropertyBorderColor, cssValuePool().createInheritedValue());
        break;
    case InsetBorders:
        style->setProperty(CSSPropertyBorderWidth, cssValuePool().createValue(1, CSSPrimitiveValue::CSS_PX));
        style->setProperty(CSSPropertyBorderStyle, cssValuePool().createIdentifierValue(CSSValueInset));
        style->setProperty(CSSPropertyBorderColor, cssValuePool().createInheritedValue());
        break;
    case NoBorders:
        style->setProperty(CSSPropertyBorderWidth, cssValuePool().createValue(0, CSSPrimitiveValue::CSS_PX));
        break;
    }

    if (m_padding)
        style->setProperty(CSSPropertyPadding, cssValuePool().createValue(m_padding, CSSPrimitiveValue::CSS_PX));

    return style.release();
}

StylePropertySet* HTMLTableElement::additionalCellStyle()
{
    if (!m_sharedCellStyle)
        m_sharedCellStyle = createSharedCellStyle();
    return m_sharedCellStyle.get();
}

static StylePropertySet* leakGroupBorderStyle(int rows)
{
    RefPtr<StylePropertySet> style = StylePropertySet::create();
    if (rows) {
        style->setProperty(CSSPropertyBorderTopWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderBottomWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderTopStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderBottomStyle, CSSValueSolid);
    } else {
        style->setProperty(CSSPropertyBorderLeftWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderRightWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderLeftStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderRightStyle, CSSValueSolid);
    }
    return style.release().leakRef();
}

StylePropertySet* HTMLTableElement::additionalGroupStyle(bool rows)
{
    if (m_rulesAttr != GroupsRules)
        return 0;

    if (rows) {
        static StylePropertySet* rowBorderStyle = leakGroupBorderStyle(true);
        return rowBorderStyle;
    }
    static StylePropertySet* columnBorderStyle = leakGroupBorderStyle(false);
    return columnBorderStyle;
}

bool HTMLTableElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == backgroundAttr || HTMLElement::isURLAttribute(attr);
}

HTMLCollection* HTMLTableElement::rows()
{
    if (!m_rowsCollection)
        m_rowsCollection = HTMLTableRowsCollection::create(this);
    return m_rowsCollection.get();
}

HTMLCollection* HTMLTableElement::tBodies()
{
    return ensureCachedHTMLCollection(TableTBodies);
}

String HTMLTableElement::rules() const
{
    return getAttribute(rulesAttr);
}

String HTMLTableElement::summary() const
{
    return getAttribute(summaryAttr);
}

void HTMLTableElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    HTMLElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document()->completeURL(getAttribute(backgroundAttr)));
}

}
