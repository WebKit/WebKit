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

#include "CSSImageValue.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "CSSValuePool.h"
#include "ElementChildIterator.h"
#include "GenericCachedHTMLCollection.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLTableCaptionElement.h"
#include "HTMLTableRowElement.h"
#include "HTMLTableRowsCollection.h"
#include "HTMLTableSectionElement.h"
#include "NodeRareData.h"
#include "RenderTable.h"
#include "StyleProperties.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Ref.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLTableElement);

using namespace HTMLNames;

HTMLTableElement::HTMLTableElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(tableTag));
}

Ref<HTMLTableElement> HTMLTableElement::create(Document& document)
{
    return adoptRef(*new HTMLTableElement(tableTag, document));
}

Ref<HTMLTableElement> HTMLTableElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLTableElement(tagName, document));
}

RefPtr<HTMLTableCaptionElement> HTMLTableElement::caption() const
{
    return childrenOfType<HTMLTableCaptionElement>(const_cast<HTMLTableElement&>(*this)).first();
}

ExceptionOr<void> HTMLTableElement::setCaption(RefPtr<HTMLTableCaptionElement>&& newCaption)
{
    deleteCaption();
    if (!newCaption)
        return { };
    return insertBefore(*newCaption, firstChild());
}

RefPtr<HTMLTableSectionElement> HTMLTableElement::tHead() const
{
    for (RefPtr<Node> child = firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(theadTag))
            return downcast<HTMLTableSectionElement>(child.get());
    }
    return nullptr;
}

ExceptionOr<void> HTMLTableElement::setTHead(RefPtr<HTMLTableSectionElement>&& newHead)
{
    if (UNLIKELY(newHead && !newHead->hasTagName(theadTag)))
        return Exception { HierarchyRequestError };

    deleteTHead();
    if (!newHead)
        return { };

    RefPtr<Node> child;
    for (child = firstChild(); child; child = child->nextSibling()) {
        if (child->isElementNode() && !child->hasTagName(captionTag) && !child->hasTagName(colgroupTag))
            break;
    }

    return insertBefore(*newHead, child.get());
}

RefPtr<HTMLTableSectionElement> HTMLTableElement::tFoot() const
{
    for (RefPtr<Node> child = firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(tfootTag))
            return downcast<HTMLTableSectionElement>(child.get());
    }
    return nullptr;
}

ExceptionOr<void> HTMLTableElement::setTFoot(RefPtr<HTMLTableSectionElement>&& newFoot)
{
    if (UNLIKELY(newFoot && !newFoot->hasTagName(tfootTag)))
        return Exception { HierarchyRequestError };
    deleteTFoot();
    if (!newFoot)
        return { };
    return appendChild(*newFoot);
}

Ref<HTMLTableSectionElement> HTMLTableElement::createTHead()
{
    if (auto existingHead = tHead())
        return existingHead.releaseNonNull();
    auto head = HTMLTableSectionElement::create(theadTag, document());
    setTHead(head.copyRef());
    return head;
}

void HTMLTableElement::deleteTHead()
{
    if (auto head = tHead())
        removeChild(*head);
}

Ref<HTMLTableSectionElement> HTMLTableElement::createTFoot()
{
    if (auto existingFoot = tFoot())
        return existingFoot.releaseNonNull();
    auto foot = HTMLTableSectionElement::create(tfootTag, document());
    setTFoot(foot.copyRef());
    return foot;
}

void HTMLTableElement::deleteTFoot()
{
    if (auto foot = tFoot())
        removeChild(*foot);
}

Ref<HTMLTableSectionElement> HTMLTableElement::createTBody()
{
    auto body = HTMLTableSectionElement::create(tbodyTag, document());
    RefPtr<Node> referenceElement = lastBody() ? lastBody()->nextSibling() : nullptr;
    insertBefore(body, referenceElement.get());
    return body;
}

Ref<HTMLTableCaptionElement> HTMLTableElement::createCaption()
{
    if (auto existingCaption = caption())
        return existingCaption.releaseNonNull();
    auto caption = HTMLTableCaptionElement::create(captionTag, document());
    setCaption(caption.copyRef());
    return caption;
}

void HTMLTableElement::deleteCaption()
{
    if (auto caption = this->caption())
        removeChild(*caption);
}

HTMLTableSectionElement* HTMLTableElement::lastBody() const
{
    for (RefPtr<Node> child = lastChild(); child; child = child->previousSibling()) {
        if (child->hasTagName(tbodyTag))
            return downcast<HTMLTableSectionElement>(child.get());
    }
    return nullptr;
}

ExceptionOr<Ref<HTMLElement>> HTMLTableElement::insertRow(int index)
{
    if (index < -1)
        return Exception { IndexSizeError };

    Ref<HTMLTableElement> protectedThis(*this);

    RefPtr<HTMLTableRowElement> lastRow;
    RefPtr<HTMLTableRowElement> row;
    if (index == -1)
        lastRow = HTMLTableRowsCollection::lastRow(*this);
    else {
        for (int i = 0; i <= index; ++i) {
            row = HTMLTableRowsCollection::rowAfter(*this, lastRow.get());
            if (!row) {
                if (i != index)
                    return Exception { IndexSizeError };
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
            auto newBody = HTMLTableSectionElement::create(tbodyTag, document());
            auto newRow = HTMLTableRowElement::create(document());
            newBody->appendChild(newRow);
            // FIXME: Why ignore the exception if the first appendChild failed?
            auto result = appendChild(newBody);
            if (result.hasException())
                return result.releaseException();
            return Ref<HTMLElement> { WTFMove(newRow) };
        }
    }

    auto newRow = HTMLTableRowElement::create(document());
    auto result = parent->insertBefore(newRow, row.get());
    if (result.hasException())
        return result.releaseException();
    return Ref<HTMLElement> { WTFMove(newRow) };
}

ExceptionOr<void> HTMLTableElement::deleteRow(int index)
{
    RefPtr<HTMLTableRowElement> row;
    if (index == -1) {
        row = HTMLTableRowsCollection::lastRow(*this);
        if (!row)
            return { };
    } else {
        for (int i = 0; i <= index; ++i) {
            row = HTMLTableRowsCollection::rowAfter(*this, row.get());
            if (!row)
                break;
        }
        if (!row)
            return Exception { IndexSizeError };
    }
    return row->remove();
}

static inline bool isTableCellAncestor(const Element& element)
{
    return element.hasTagName(theadTag)
        || element.hasTagName(tbodyTag)
        || element.hasTagName(tfootTag)
        || element.hasTagName(trTag)
        || element.hasTagName(thTag);
}

static bool setTableCellsChanged(Element& element)
{
    bool cellChanged = false;

    if (element.hasTagName(tdTag))
        cellChanged = true;
    else if (isTableCellAncestor(element)) {
        for (auto& child : childrenOfType<Element>(element))
            cellChanged |= setTableCellsChanged(child);
    }

    if (cellChanged)
        element.invalidateStyleForSubtree();

    return cellChanged;
}

static bool getBordersFromFrameAttributeValue(const AtomString& value, bool& borderTop, bool& borderRight, bool& borderBottom, bool& borderLeft)
{
    borderTop = false;
    borderRight = false;
    borderBottom = false;
    borderLeft = false;

    if (equalLettersIgnoringASCIICase(value, "above"))
        borderTop = true;
    else if (equalLettersIgnoringASCIICase(value, "below"))
        borderBottom = true;
    else if (equalLettersIgnoringASCIICase(value, "hsides"))
        borderTop = borderBottom = true;
    else if (equalLettersIgnoringASCIICase(value, "vsides"))
        borderLeft = borderRight = true;
    else if (equalLettersIgnoringASCIICase(value, "lhs"))
        borderLeft = true;
    else if (equalLettersIgnoringASCIICase(value, "rhs"))
        borderRight = true;
    else if (equalLettersIgnoringASCIICase(value, "box") || equalLettersIgnoringASCIICase(value, "border"))
        borderTop = borderBottom = borderLeft = borderRight = true;
    else if (!equalLettersIgnoringASCIICase(value, "void"))
        return false;
    return true;
}

void HTMLTableElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == widthAttr)
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    else if (name == heightAttr)
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    else if (name == borderAttr) 
        addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderWidth, parseBorderWidthAttribute(value), CSSUnitType::CSS_PX);
    else if (name == bordercolorAttr) {
        if (!value.isEmpty())
            addHTMLColorToStyle(style, CSSPropertyBorderColor, value);
    } else if (name == bgcolorAttr)
        addHTMLColorToStyle(style, CSSPropertyBackgroundColor, value);
    else if (name == backgroundAttr) {
        String url = stripLeadingAndTrailingHTMLSpaces(value);
        if (!url.isEmpty())
            style.setProperty(CSSProperty(CSSPropertyBackgroundImage, CSSImageValue::create(document().completeURL(url), LoadedFromOpaqueSource::No)));
    } else if (name == valignAttr) {
        if (!value.isEmpty())
            addPropertyToPresentationAttributeStyle(style, CSSPropertyVerticalAlign, value);
    } else if (name == cellspacingAttr) {
        if (!value.isEmpty())
            addHTMLLengthToStyle(style, CSSPropertyBorderSpacing, value);
    } else if (name == vspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
    } else if (name == hspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
    } else if (name == alignAttr) {
        if (!value.isEmpty()) {
            if (equalLettersIgnoringASCIICase(value, "center")) {
                addPropertyToPresentationAttributeStyle(style, CSSPropertyMarginInlineStart, CSSValueAuto);
                addPropertyToPresentationAttributeStyle(style, CSSPropertyMarginInlineEnd, CSSValueAuto);
            } else
                addPropertyToPresentationAttributeStyle(style, CSSPropertyFloat, value);
        }
    } else if (name == rulesAttr) {
        // The presence of a valid rules attribute causes border collapsing to be enabled.
        if (m_rulesAttr != UnsetRules)
            addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderCollapse, CSSValueCollapse);
    } else if (name == frameAttr) {
        bool borderTop;
        bool borderRight;
        bool borderBottom;
        bool borderLeft;
        if (getBordersFromFrameAttributeValue(value, borderTop, borderRight, borderBottom, borderLeft)) {
            addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderWidth, CSSValueThin);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderTopStyle, borderTop ? CSSValueSolid : CSSValueHidden);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderBottomStyle, borderBottom ? CSSValueSolid : CSSValueHidden);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderLeftStyle, borderLeft ? CSSValueSolid : CSSValueHidden);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderRightStyle, borderRight ? CSSValueSolid : CSSValueHidden);
        }
    } else
        HTMLElement::collectStyleForPresentationAttribute(name, value, style);
}

bool HTMLTableElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr || name == bgcolorAttr || name == backgroundAttr || name == valignAttr || name == vspaceAttr || name == hspaceAttr || name == cellspacingAttr || name == borderAttr || name == bordercolorAttr || name == frameAttr || name == rulesAttr)
        return true;
    return HTMLElement::isPresentationAttribute(name);
}

void HTMLTableElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    CellBorders bordersBefore = cellBorders();
    unsigned short oldPadding = m_padding;

    if (name == borderAttr)  {
        // FIXME: This attribute is a mess.
        m_borderAttr = parseBorderWidthAttribute(value);
    } else if (name == bordercolorAttr) {
        m_borderColorAttr = !value.isEmpty();
    } else if (name == frameAttr) {
        // FIXME: This attribute is a mess.
        bool borderTop;
        bool borderRight;
        bool borderBottom;
        bool borderLeft;
        m_frameAttr = getBordersFromFrameAttributeValue(value, borderTop, borderRight, borderBottom, borderLeft);
    } else if (name == rulesAttr) {
        m_rulesAttr = UnsetRules;
        if (equalLettersIgnoringASCIICase(value, "none"))
            m_rulesAttr = NoneRules;
        else if (equalLettersIgnoringASCIICase(value, "groups"))
            m_rulesAttr = GroupsRules;
        else if (equalLettersIgnoringASCIICase(value, "rows"))
            m_rulesAttr = RowsRules;
        else if (equalLettersIgnoringASCIICase(value, "cols"))
            m_rulesAttr = ColsRules;
        else if (equalLettersIgnoringASCIICase(value, "all"))
            m_rulesAttr = AllRules;
    } else if (name == cellpaddingAttr) {
        if (!value.isEmpty())
            m_padding = std::max(0, value.toInt());
        else
            m_padding = 1;
    } else if (name == colsAttr) {
        // ###
    } else
        HTMLElement::parseAttribute(name, value);

    if (bordersBefore != cellBorders() || oldPadding != m_padding) {
        m_sharedCellStyle = nullptr;
        bool cellChanged = false;
        for (auto& child : childrenOfType<Element>(*this))
            cellChanged |= setTableCellsChanged(child);
        if (cellChanged)
            invalidateStyleForSubtree();
    }
}

static StyleProperties* leakBorderStyle(CSSValueID value)
{
    auto style = MutableStyleProperties::create();
    style->setProperty(CSSPropertyBorderTopStyle, value);
    style->setProperty(CSSPropertyBorderBottomStyle, value);
    style->setProperty(CSSPropertyBorderLeftStyle, value);
    style->setProperty(CSSPropertyBorderRightStyle, value);
    return &style.leakRef();
}

const StyleProperties* HTMLTableElement::additionalPresentationAttributeStyle() const
{
    if (m_frameAttr)
        return 0;
    
    if (!m_borderAttr && !m_borderColorAttr) {
        // Setting the border to 'hidden' allows it to win over any border
        // set on the table's cells during border-conflict resolution.
        if (m_rulesAttr != UnsetRules) {
            static StyleProperties* solidBorderStyle = leakBorderStyle(CSSValueHidden);
            return solidBorderStyle;
        }
        return 0;
    }

    if (m_borderColorAttr) {
        static StyleProperties* solidBorderStyle = leakBorderStyle(CSSValueSolid);
        return solidBorderStyle;
    }
    static StyleProperties* outsetBorderStyle = leakBorderStyle(CSSValueOutset);
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

Ref<StyleProperties> HTMLTableElement::createSharedCellStyle()
{
    auto style = MutableStyleProperties::create();

    auto& cssValuePool = CSSValuePool::singleton();
    switch (cellBorders()) {
    case SolidBordersColsOnly:
        style->setProperty(CSSPropertyBorderLeftWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderRightWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderLeftStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderRightStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderColor, cssValuePool.createInheritedValue());
        break;
    case SolidBordersRowsOnly:
        style->setProperty(CSSPropertyBorderTopWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderBottomWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderTopStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderBottomStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderColor, cssValuePool.createInheritedValue());
        break;
    case SolidBorders:
        style->setProperty(CSSPropertyBorderWidth, cssValuePool.createValue(1, CSSUnitType::CSS_PX));
        style->setProperty(CSSPropertyBorderStyle, cssValuePool.createIdentifierValue(CSSValueSolid));
        style->setProperty(CSSPropertyBorderColor, cssValuePool.createInheritedValue());
        break;
    case InsetBorders:
        style->setProperty(CSSPropertyBorderWidth, cssValuePool.createValue(1, CSSUnitType::CSS_PX));
        style->setProperty(CSSPropertyBorderStyle, cssValuePool.createIdentifierValue(CSSValueInset));
        style->setProperty(CSSPropertyBorderColor, cssValuePool.createInheritedValue());
        break;
    case NoBorders:
        // If 'rules=none' then allow any borders set at cell level to take effect. 
        break;
    }

    if (m_padding)
        style->setProperty(CSSPropertyPadding, cssValuePool.createValue(m_padding, CSSUnitType::CSS_PX));

    return style;
}

const StyleProperties* HTMLTableElement::additionalCellStyle()
{
    if (!m_sharedCellStyle)
        m_sharedCellStyle = createSharedCellStyle();
    return m_sharedCellStyle.get();
}

static StyleProperties* leakGroupBorderStyle(int rows)
{
    auto style = MutableStyleProperties::create();
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
    return &style.leakRef();
}

const StyleProperties* HTMLTableElement::additionalGroupStyle(bool rows)
{
    if (m_rulesAttr != GroupsRules)
        return 0;

    if (rows) {
        static StyleProperties* rowBorderStyle = leakGroupBorderStyle(true);
        return rowBorderStyle;
    }
    static StyleProperties* columnBorderStyle = leakGroupBorderStyle(false);
    return columnBorderStyle;
}

bool HTMLTableElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == backgroundAttr || HTMLElement::isURLAttribute(attribute);
}

Ref<HTMLCollection> HTMLTableElement::rows()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<HTMLTableRowsCollection>(*this, TableRows);
}

Ref<HTMLCollection> HTMLTableElement::tBodies()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<GenericCachedHTMLCollection<CollectionTypeTraits<TableTBodies>::traversalType>>(*this, TableTBodies);
}

const AtomString& HTMLTableElement::rules() const
{
    return attributeWithoutSynchronization(rulesAttr);
}

const AtomString& HTMLTableElement::summary() const
{
    return attributeWithoutSynchronization(summaryAttr);
}

void HTMLTableElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    HTMLElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document().completeURL(attributeWithoutSynchronization(backgroundAttr)));
}

}
