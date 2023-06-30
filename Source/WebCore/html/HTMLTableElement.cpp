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
#include "ElementChildIteratorInlines.h"
#include "GenericCachedHTMLCollection.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLTableCaptionElement.h"
#include "HTMLTableRowElement.h"
#include "HTMLTableRowsCollection.h"
#include "HTMLTableSectionElement.h"
#include "MutableStyleProperties.h"
#include "NodeName.h"
#include "NodeRareData.h"
#include "RenderTable.h"
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

    if (equalLettersIgnoringASCIICase(value, "above"_s))
        borderTop = true;
    else if (equalLettersIgnoringASCIICase(value, "below"_s))
        borderBottom = true;
    else if (equalLettersIgnoringASCIICase(value, "hsides"_s))
        borderTop = borderBottom = true;
    else if (equalLettersIgnoringASCIICase(value, "vsides"_s))
        borderLeft = borderRight = true;
    else if (equalLettersIgnoringASCIICase(value, "lhs"_s))
        borderLeft = true;
    else if (equalLettersIgnoringASCIICase(value, "rhs"_s))
        borderRight = true;
    else if (equalLettersIgnoringASCIICase(value, "box"_s) || equalLettersIgnoringASCIICase(value, "border"_s))
        borderTop = borderBottom = borderLeft = borderRight = true;
    else if (!equalLettersIgnoringASCIICase(value, "void"_s))
        return false;
    return true;
}

void HTMLTableElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    switch (name.nodeName()) {
    case AttributeNames::widthAttr:
        addHTMLLengthToStyle(style, CSSPropertyWidth, value, AllowZeroValue::No);
        break;
    case AttributeNames::heightAttr:
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
        break;
    case AttributeNames::borderAttr:
        addPropertyToPresentationalHintStyle(style, CSSPropertyBorderWidth, parseBorderWidthAttribute(value), CSSUnitType::CSS_PX);
        break;
    case AttributeNames::bordercolorAttr:
        if (!value.isEmpty())
            addHTMLColorToStyle(style, CSSPropertyBorderColor, value);
        break;
    case AttributeNames::bgcolorAttr:
        addHTMLColorToStyle(style, CSSPropertyBackgroundColor, value);
        break;
    case AttributeNames::backgroundAttr:
        if (auto url = value.string().trim(isASCIIWhitespace); !url.isEmpty())
            style.setProperty(CSSProperty(CSSPropertyBackgroundImage, CSSImageValue::create(document().completeURL(url), LoadedFromOpaqueSource::No)));
        break;
    case AttributeNames::valignAttr:
        if (!value.isEmpty())
            addPropertyToPresentationalHintStyle(style, CSSPropertyVerticalAlign, value);
        break;
    case AttributeNames::cellspacingAttr:
        if (!value.isEmpty())
            addHTMLPixelsToStyle(style, CSSPropertyBorderSpacing, value);
        break;
    case AttributeNames::alignAttr:
        if (!value.isEmpty()) {
            if (equalLettersIgnoringASCIICase(value, "center"_s)) {
                addPropertyToPresentationalHintStyle(style, CSSPropertyMarginInlineStart, CSSValueAuto);
                addPropertyToPresentationalHintStyle(style, CSSPropertyMarginInlineEnd, CSSValueAuto);
            } else
                addPropertyToPresentationalHintStyle(style, CSSPropertyFloat, value);
        }
        break;
    case AttributeNames::rulesAttr:
        // The presence of a valid rules attribute causes border collapsing to be enabled.
        if (m_rulesAttr != UnsetRules)
            addPropertyToPresentationalHintStyle(style, CSSPropertyBorderCollapse, CSSValueCollapse);
        break;
    case AttributeNames::frameAttr: {
        bool borderTop;
        bool borderRight;
        bool borderBottom;
        bool borderLeft;
        if (getBordersFromFrameAttributeValue(value, borderTop, borderRight, borderBottom, borderLeft)) {
            addPropertyToPresentationalHintStyle(style, CSSPropertyBorderWidth, CSSValueThin);
            addPropertyToPresentationalHintStyle(style, CSSPropertyBorderTopStyle, borderTop ? CSSValueSolid : CSSValueHidden);
            addPropertyToPresentationalHintStyle(style, CSSPropertyBorderBottomStyle, borderBottom ? CSSValueSolid : CSSValueHidden);
            addPropertyToPresentationalHintStyle(style, CSSPropertyBorderLeftStyle, borderLeft ? CSSValueSolid : CSSValueHidden);
            addPropertyToPresentationalHintStyle(style, CSSPropertyBorderRightStyle, borderRight ? CSSValueSolid : CSSValueHidden);
        }
        break;
    }
    default:
        HTMLElement::collectPresentationalHintsForAttribute(name, value, style);
        break;
    }
}

bool HTMLTableElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    switch (name.nodeName()) {
    case AttributeNames::widthAttr:
    case AttributeNames::heightAttr:
    case AttributeNames::bgcolorAttr:
    case AttributeNames::backgroundAttr:
    case AttributeNames::valignAttr:
    case AttributeNames::vspaceAttr:
    case AttributeNames::hspaceAttr:
    case AttributeNames::cellspacingAttr:
    case AttributeNames::borderAttr:
    case AttributeNames::bordercolorAttr:
    case AttributeNames::frameAttr:
    case AttributeNames::rulesAttr:
        return true;
    default:
        break;
    }
    return HTMLElement::hasPresentationalHintsForAttribute(name);
}

void HTMLTableElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    HTMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);

    CellBorders bordersBefore = cellBorders();
    unsigned short oldPadding = m_padding;

    switch (name.nodeName()) {
    case AttributeNames::borderAttr:
        // FIXME: This attribute is a mess.
        m_borderAttr = parseBorderWidthAttribute(newValue);
        break;
    case AttributeNames::frameAttr: {
        // FIXME: This attribute is a mess.
        bool borderTop;
        bool borderRight;
        bool borderBottom;
        bool borderLeft;
        m_frameAttr = getBordersFromFrameAttributeValue(newValue, borderTop, borderRight, borderBottom, borderLeft);
        break;
    }
    case AttributeNames::rulesAttr:
        m_rulesAttr = UnsetRules;
        if (equalLettersIgnoringASCIICase(newValue, "none"_s))
            m_rulesAttr = NoneRules;
        else if (equalLettersIgnoringASCIICase(newValue, "groups"_s))
            m_rulesAttr = GroupsRules;
        else if (equalLettersIgnoringASCIICase(newValue, "rows"_s))
            m_rulesAttr = RowsRules;
        else if (equalLettersIgnoringASCIICase(newValue, "cols"_s))
            m_rulesAttr = ColsRules;
        else if (equalLettersIgnoringASCIICase(newValue, "all"_s))
            m_rulesAttr = AllRules;
        break;
    case AttributeNames::cellpaddingAttr:
        if (!newValue.isEmpty())
            m_padding = std::max(0, parseHTMLInteger(newValue).value_or(0));
        else
            m_padding = 1;
        break;
    default:
        break;
    }

    if (bordersBefore != cellBorders() || oldPadding != m_padding) {
        m_sharedCellStyle = nullptr;
        bool cellChanged = false;
        for (auto& child : childrenOfType<Element>(*this))
            cellChanged |= setTableCellsChanged(child);
        if (cellChanged)
            invalidateStyleForSubtree();
    }
}

static MutableStyleProperties* leakBorderStyle(CSSValueID value)
{
    auto style = MutableStyleProperties::create();
    style->setProperty(CSSPropertyBorderTopStyle, value);
    style->setProperty(CSSPropertyBorderBottomStyle, value);
    style->setProperty(CSSPropertyBorderLeftStyle, value);
    style->setProperty(CSSPropertyBorderRightStyle, value);
    return &style.leakRef();
}

const MutableStyleProperties* HTMLTableElement::additionalPresentationalHintStyle() const
{
    if (m_frameAttr)
        return nullptr;
    
    if (!m_borderAttr) {
        // Setting the border to 'hidden' allows it to win over any border
        // set on the table's cells during border-conflict resolution.
        if (m_rulesAttr != UnsetRules) {
            static auto* solidBorderStyle = leakBorderStyle(CSSValueHidden);
            return solidBorderStyle;
        }
        return nullptr;
    }

    static auto* outsetBorderStyle = leakBorderStyle(CSSValueOutset);
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
            return InsetBorders;
    }
    ASSERT_NOT_REACHED();
    return NoBorders;
}

Ref<MutableStyleProperties> HTMLTableElement::createSharedCellStyle() const
{
    auto style = MutableStyleProperties::create();

    switch (cellBorders()) {
    case SolidBordersColsOnly:
        style->setProperty(CSSPropertyBorderLeftWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderRightWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderLeftStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderRightStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderColor, CSSPrimitiveValue::create(CSSValueInherit));
        break;
    case SolidBordersRowsOnly:
        style->setProperty(CSSPropertyBorderTopWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderBottomWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderTopStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderBottomStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderColor, CSSPrimitiveValue::create(CSSValueInherit));
        break;
    case SolidBorders:
        style->setProperty(CSSPropertyBorderWidth, CSSPrimitiveValue::create(1, CSSUnitType::CSS_PX));
        style->setProperty(CSSPropertyBorderStyle, CSSPrimitiveValue::create(CSSValueSolid));
        style->setProperty(CSSPropertyBorderColor, CSSPrimitiveValue::create(CSSValueInherit));
        break;
    case InsetBorders:
        style->setProperty(CSSPropertyBorderWidth, CSSPrimitiveValue::create(1, CSSUnitType::CSS_PX));
        style->setProperty(CSSPropertyBorderStyle, CSSPrimitiveValue::create(CSSValueInset));
        style->setProperty(CSSPropertyBorderColor, CSSPrimitiveValue::create(CSSValueInherit));
        break;
    case NoBorders:
        // If 'rules=none' then allow any borders set at cell level to take effect. 
        break;
    }

    if (m_padding)
        style->setProperty(CSSPropertyPadding, CSSPrimitiveValue::create(m_padding, CSSUnitType::CSS_PX));

    return style;
}

const MutableStyleProperties* HTMLTableElement::additionalCellStyle() const
{
    if (!m_sharedCellStyle)
        m_sharedCellStyle = createSharedCellStyle();
    return m_sharedCellStyle.get();
}

static MutableStyleProperties* leakGroupBorderStyle(int rows)
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

const MutableStyleProperties* HTMLTableElement::additionalGroupStyle(bool rows) const
{
    if (m_rulesAttr != GroupsRules)
        return nullptr;
    if (rows) {
        static auto* rowBorderStyle = leakGroupBorderStyle(true);
        return rowBorderStyle;
    }
    static auto* columnBorderStyle = leakGroupBorderStyle(false);
    return columnBorderStyle;
}

bool HTMLTableElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == backgroundAttr || HTMLElement::isURLAttribute(attribute);
}

Ref<HTMLCollection> HTMLTableElement::rows()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<HTMLTableRowsCollection>(*this, CollectionType::TableRows);
}

Ref<HTMLCollection> HTMLTableElement::tBodies()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<GenericCachedHTMLCollection<CollectionTypeTraits<CollectionType::TableTBodies>::traversalType>>(*this, CollectionType::TableTBodies);
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
