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
#include "StyleComputedStyle+GettersInlines.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(HTMLTableElement);

using namespace HTMLNames;

HTMLTableElement::HTMLTableElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(tableTag));
}

HTMLTableElement::~HTMLTableElement() = default;

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
    return insertBefore(*newCaption, protect(firstChild()));
}

RefPtr<HTMLTableSectionElement> HTMLTableElement::tHead() const
{
    for (auto& child : childrenOfType<HTMLTableSectionElement>(const_cast<HTMLTableElement&>(*this))) {
        if (child.hasTagName(theadTag))
            return child;
    }
    return nullptr;
}

ExceptionOr<void> HTMLTableElement::setTHead(RefPtr<HTMLTableSectionElement>&& newHead)
{
    if (newHead && !newHead->hasTagName(theadTag)) [[unlikely]]
        return Exception { ExceptionCode::HierarchyRequestError };

    deleteTHead();
    if (!newHead)
        return { };

    RefPtr<Node> child;
    for (child = firstChild(); child; child = child->nextSibling()) {
        if (child->isElementNode() && !child->hasTagName(captionTag) && !child->hasTagName(colgroupTag))
            break;
    }

    return insertBefore(*newHead, WTF::move(child));
}

RefPtr<HTMLTableSectionElement> HTMLTableElement::tFoot() const
{
    for (auto& child : childrenOfType<HTMLTableSectionElement>(const_cast<HTMLTableElement&>(*this))) {
        if (child.hasTagName(tfootTag))
            return child;
    }
    return nullptr;
}

ExceptionOr<void> HTMLTableElement::setTFoot(RefPtr<HTMLTableSectionElement>&& newFoot)
{
    if (newFoot && !newFoot->hasTagName(tfootTag)) [[unlikely]]
        return Exception { ExceptionCode::HierarchyRequestError };
    deleteTFoot();
    if (!newFoot)
        return { };
    return appendChild(*newFoot);
}

Ref<HTMLTableSectionElement> HTMLTableElement::createTHead()
{
    if (RefPtr existingHead = tHead())
        return existingHead.releaseNonNull();
    Ref head = HTMLTableSectionElement::create(theadTag, protect(document()));
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
    if (RefPtr existingFoot = tFoot())
        return existingFoot.releaseNonNull();
    Ref foot = HTMLTableSectionElement::create(tfootTag, protect(document()));
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
    Ref body = HTMLTableSectionElement::create(tbodyTag, protect(document()));
    RefPtr referenceElement = lastBody() ? lastBody()->nextSibling() : nullptr;
    insertBefore(body, WTF::move(referenceElement));
    return body;
}

Ref<HTMLTableCaptionElement> HTMLTableElement::createCaption()
{
    if (RefPtr existingCaption = caption())
        return existingCaption.releaseNonNull();
    Ref caption = HTMLTableCaptionElement::create(captionTag, protect(document()));
    setCaption(caption.copyRef());
    return caption;
}

void HTMLTableElement::deleteCaption()
{
    if (RefPtr caption = this->caption())
        removeChild(*caption);
}

HTMLTableSectionElement* HTMLTableElement::lastBody() const
{
    for (auto* child = lastChild(); child; child = child->previousSibling()) {
        if (child->hasTagName(tbodyTag))
            return downcast<HTMLTableSectionElement>(child);
    }
    return nullptr;
}

ExceptionOr<Ref<HTMLTableRowElement>> HTMLTableElement::insertRow(int index)
{
    if (index < -1)
        return Exception { ExceptionCode::IndexSizeError };

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
                    return Exception { ExceptionCode::IndexSizeError };
                break;
            }
            lastRow = row;
        }
    }

    RefPtr<ContainerNode> parent;
    Ref document = this->document();
    if (lastRow)
        parent = row ? row->parentNode() : lastRow->parentNode();
    else {
        parent = lastBody();
        if (!parent) {
            Ref newBody = HTMLTableSectionElement::create(tbodyTag, document);
            Ref newRow = HTMLTableRowElement::create(document);
            newBody->appendChild(newRow);
            // FIXME: Why ignore the exception if the first appendChild failed?
            auto result = appendChild(newBody);
            if (result.hasException())
                return result.releaseException();
            return newRow;
        }
    }

    Ref newRow = HTMLTableRowElement::create(document);
    auto result = parent->insertBefore(newRow, WTF::move(row));
    if (result.hasException())
        return result.releaseException();
    return newRow;
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
            return Exception { ExceptionCode::IndexSizeError };
    }
    return row->remove();
}

static inline bool NODELETE isTableCellAncestor(const Element& element)
{
    return element.hasTagName(theadTag)
        || element.hasTagName(tbodyTag)
        || element.hasTagName(tfootTag)
        || element.hasTagName(trTag);
}

static bool setTableCellsChanged(Element& element)
{
    bool cellChanged = false;

    if (element.hasTagName(tdTag) || element.hasTagName(thTag))
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
        addHTMLLengthToStyle(style, CSSPropertyID::Width, value, AllowZeroValue::No);
        break;
    case AttributeNames::heightAttr:
        addHTMLLengthToStyle(style, CSSPropertyID::Height, value);
        break;
    case AttributeNames::borderAttr:
        addPropertyToPresentationalHintStyle(style, CSSPropertyID::BorderWidth, parseBorderWidthAttribute(value), CSSUnitType::Px);
        break;
    case AttributeNames::bordercolorAttr:
        if (!value.isEmpty())
            addHTMLColorToStyle(style, CSSPropertyID::BorderColor, value);
        break;
    case AttributeNames::bgcolorAttr:
        addHTMLColorToStyle(style, CSSPropertyID::BackgroundColor, value);
        break;
    case AttributeNames::backgroundAttr:
        if (auto url = value.string().trim(isASCIIWhitespace); !url.isEmpty())
            style.setProperty(CSSProperty(CSSPropertyID::BackgroundImage, CSSImageValue::create(protect(document())->encodingParseURL(url))));
        break;
    case AttributeNames::valignAttr:
        if (!value.isEmpty())
            addPropertyToPresentationalHintStyle(style, CSSPropertyID::VerticalAlign, value);
        break;
    case AttributeNames::cellspacingAttr:
        if (!value.isEmpty())
            addHTMLPixelsToStyle(style, CSSPropertyID::BorderSpacing, value);
        break;
    case AttributeNames::alignAttr:
        if (!value.isEmpty()) {
            if (equalLettersIgnoringASCIICase(value, "center"_s)) {
                addPropertyToPresentationalHintStyle(style, CSSPropertyID::MarginInlineStart, CSSValueID::Auto);
                addPropertyToPresentationalHintStyle(style, CSSPropertyID::MarginInlineEnd, CSSValueID::Auto);
            } else
                addPropertyToPresentationalHintStyle(style, CSSPropertyID::Float, value);
        }
        break;
    case AttributeNames::rulesAttr:
        // The presence of a valid rules attribute causes border collapsing to be enabled.
        if (m_rulesAttr != TableRules::Unset)
            addPropertyToPresentationalHintStyle(style, CSSPropertyID::BorderCollapse, CSSValueID::Collapse);
        break;
    case AttributeNames::frameAttr: {
        bool borderTop;
        bool borderRight;
        bool borderBottom;
        bool borderLeft;
        if (getBordersFromFrameAttributeValue(value, borderTop, borderRight, borderBottom, borderLeft)) {
            addPropertyToPresentationalHintStyle(style, CSSPropertyID::BorderWidth, CSSValueID::Thin);
            addPropertyToPresentationalHintStyle(style, CSSPropertyID::BorderTopStyle, borderTop ? CSSValueID::Solid : CSSValueID::Hidden);
            addPropertyToPresentationalHintStyle(style, CSSPropertyID::BorderBottomStyle, borderBottom ? CSSValueID::Solid : CSSValueID::Hidden);
            addPropertyToPresentationalHintStyle(style, CSSPropertyID::BorderLeftStyle, borderLeft ? CSSValueID::Solid : CSSValueID::Hidden);
            addPropertyToPresentationalHintStyle(style, CSSPropertyID::BorderRightStyle, borderRight ? CSSValueID::Solid : CSSValueID::Hidden);
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
        m_rulesAttr = TableRules::Unset;
        if (equalLettersIgnoringASCIICase(newValue, "none"_s))
            m_rulesAttr = TableRules::None;
        else if (equalLettersIgnoringASCIICase(newValue, "groups"_s))
            m_rulesAttr = TableRules::Groups;
        else if (equalLettersIgnoringASCIICase(newValue, "rows"_s))
            m_rulesAttr = TableRules::Rows;
        else if (equalLettersIgnoringASCIICase(newValue, "cols"_s))
            m_rulesAttr = TableRules::Cols;
        else if (equalLettersIgnoringASCIICase(newValue, "all"_s))
            m_rulesAttr = TableRules::All;
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

static Ref<MutableStyleProperties> createBorderStyle(CSSValueID value)
{
    Ref style = MutableStyleProperties::create();
    style->setProperty(CSSPropertyID::BorderTopStyle, value);
    style->setProperty(CSSPropertyID::BorderBottomStyle, value);
    style->setProperty(CSSPropertyID::BorderLeftStyle, value);
    style->setProperty(CSSPropertyID::BorderRightStyle, value);
    return style;
}

const MutableStyleProperties* HTMLTableElement::additionalPresentationalHintStyle() const
{
    if (m_frameAttr)
        return nullptr;
    
    if (!m_borderAttr) {
        // Setting the border to 'hidden' allows it to win over any border
        // set on the table's cells during border-conflict resolution.
        if (m_rulesAttr != TableRules::Unset) {
            static NeverDestroyed<Ref<MutableStyleProperties>> solidBorderStyle = createBorderStyle(CSSValueID::Hidden);
            return solidBorderStyle.get().ptr();
        }
        return nullptr;
    }

    static NeverDestroyed<Ref<MutableStyleProperties>> outsetBorderStyle = createBorderStyle(CSSValueID::Outset);
    return outsetBorderStyle.get().ptr();
}

HTMLTableElement::CellBorders HTMLTableElement::cellBorders() const
{
    switch (m_rulesAttr) {
    case TableRules::None:
    case TableRules::Groups:
        return CellBorders::None;
    case TableRules::All:
        return CellBorders::Solid;
    case TableRules::Cols:
        return CellBorders::SolidColsOnly;
    case TableRules::Rows:
        return CellBorders::SolidRowsOnly;
    case TableRules::Unset:
        if (!m_borderAttr)
            return CellBorders::None;
        return CellBorders::Inset;
    }
    ASSERT_NOT_REACHED();
    return CellBorders::None;
}

Ref<MutableStyleProperties> HTMLTableElement::createSharedCellStyle() const
{
    auto style = MutableStyleProperties::create();

    switch (cellBorders()) {
    case CellBorders::SolidColsOnly:
        style->setProperty(CSSPropertyID::BorderLeftWidth, CSSValueID::Thin);
        style->setProperty(CSSPropertyID::BorderRightWidth, CSSValueID::Thin);
        style->setProperty(CSSPropertyID::BorderLeftStyle, CSSValueID::Solid);
        style->setProperty(CSSPropertyID::BorderRightStyle, CSSValueID::Solid);
        style->setProperty(CSSPropertyID::BorderColor, CSSKeywordValue::create(CSSValueID::Inherit));
        break;
    case CellBorders::SolidRowsOnly:
        style->setProperty(CSSPropertyID::BorderTopWidth, CSSValueID::Thin);
        style->setProperty(CSSPropertyID::BorderBottomWidth, CSSValueID::Thin);
        style->setProperty(CSSPropertyID::BorderTopStyle, CSSValueID::Solid);
        style->setProperty(CSSPropertyID::BorderBottomStyle, CSSValueID::Solid);
        style->setProperty(CSSPropertyID::BorderColor, CSSKeywordValue::create(CSSValueID::Inherit));
        break;
    case CellBorders::Solid:
        style->setProperty(CSSPropertyID::BorderWidth, CSSPrimitiveValue::create(1, CSSUnitType::Px));
        style->setProperty(CSSPropertyID::BorderStyle, CSSKeywordValue::create(CSSValueID::Solid));
        style->setProperty(CSSPropertyID::BorderColor, CSSKeywordValue::create(CSSValueID::Inherit));
        break;
    case CellBorders::Inset:
        style->setProperty(CSSPropertyID::BorderWidth, CSSPrimitiveValue::create(1, CSSUnitType::Px));
        style->setProperty(CSSPropertyID::BorderStyle, CSSKeywordValue::create(CSSValueID::Inset));
        style->setProperty(CSSPropertyID::BorderColor, CSSKeywordValue::create(CSSValueID::Inherit));
        break;
    case CellBorders::None:
        // If 'rules=none' then allow any borders set at cell level to take effect. 
        break;
    }

    if (m_padding)
        style->setProperty(CSSPropertyID::Padding, CSSPrimitiveValue::create(m_padding, CSSUnitType::Px));

    return style;
}

const MutableStyleProperties* HTMLTableElement::additionalCellStyle() const
{
    if (!m_sharedCellStyle)
        m_sharedCellStyle = createSharedCellStyle();
    return m_sharedCellStyle.get();
}

static Ref<MutableStyleProperties> makeGroupBorderStyle(bool rows)
{
    auto style = MutableStyleProperties::create();
    if (rows) {
        style->setProperty(CSSPropertyID::BorderTopWidth, CSSValueID::Thin);
        style->setProperty(CSSPropertyID::BorderBottomWidth, CSSValueID::Thin);
        style->setProperty(CSSPropertyID::BorderTopStyle, CSSValueID::Solid);
        style->setProperty(CSSPropertyID::BorderBottomStyle, CSSValueID::Solid);
    } else {
        style->setProperty(CSSPropertyID::BorderLeftWidth, CSSValueID::Thin);
        style->setProperty(CSSPropertyID::BorderRightWidth, CSSValueID::Thin);
        style->setProperty(CSSPropertyID::BorderLeftStyle, CSSValueID::Solid);
        style->setProperty(CSSPropertyID::BorderRightStyle, CSSValueID::Solid);
    }
    return style;
}

const MutableStyleProperties* HTMLTableElement::additionalGroupStyle(bool rows) const
{
    if (m_rulesAttr != TableRules::Groups)
        return nullptr;
    if (rows) {
        static NeverDestroyed<Ref<MutableStyleProperties>> rowBorderStyle = makeGroupBorderStyle(true);
        return rowBorderStyle->ptr();
    }
    static NeverDestroyed<Ref<MutableStyleProperties>> columnBorderStyle = makeGroupBorderStyle(false);
    return columnBorderStyle->ptr();
}

bool HTMLTableElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == backgroundAttr || HTMLElement::isURLAttribute(attribute);
}

Ref<HTMLCollection> HTMLTableElement::rows()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<HTMLTableRowsCollection>(*this);
}

Ref<HTMLCollection> HTMLTableElement::tBodies()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<HTMLTableTBodiesCollection>(*this);
}

const AtomString& HTMLTableElement::rules() const
{
    return attributeWithoutSynchronization(rulesAttr);
}

const AtomString& HTMLTableElement::summary() const
{
    return attributeWithoutSynchronization(summaryAttr);
}

void HTMLTableElement::addSubresourceAttributeURLs(OrderedHashSet<URL>& urls) const
{
    HTMLElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, protect(document())->encodingParseURL(attributeWithoutSynchronization(backgroundAttr)));
}

}
