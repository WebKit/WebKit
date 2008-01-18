/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "CSSPropertyNames.h"
#include "CSSStyleSheet.h"
#include "CSSValueKeywords.h"
#include "ExceptionCode.h"
#include "HTMLNames.h"
#include "HTMLTableCaptionElement.h"
#include "HTMLTableRowsCollection.h"
#include "HTMLTableRowElement.h"
#include "HTMLTableSectionElement.h"
#include "RenderTable.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

HTMLTableElement::HTMLTableElement(Document *doc)
    : HTMLElement(tableTag, doc)
    , m_borderAttr(false)
    , m_borderColorAttr(false)
    , m_frameAttr(false)
    , m_rulesAttr(UnsetRules)
    , m_padding(1)
{
}

bool HTMLTableElement::checkDTD(const Node* newChild)
{
    if (newChild->isTextNode())
        return static_cast<const Text*>(newChild)->containsOnlyWhitespace();
    return newChild->hasTagName(captionTag) ||
           newChild->hasTagName(colTag) || newChild->hasTagName(colgroupTag) ||
           newChild->hasTagName(theadTag) || newChild->hasTagName(tfootTag) ||
           newChild->hasTagName(tbodyTag) || newChild->hasTagName(formTag) ||
           newChild->hasTagName(scriptTag);
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
    RefPtr<HTMLTableSectionElement> head = new HTMLTableSectionElement(theadTag, document());
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
    RefPtr<HTMLTableSectionElement> foot = new HTMLTableSectionElement(tfootTag, document());
    ExceptionCode ec;
    setTFoot(foot, ec);
    return foot.release();
}

void HTMLTableElement::deleteTFoot()
{
    ExceptionCode ec;
    removeChild(tFoot(), ec);
}

PassRefPtr<HTMLElement> HTMLTableElement::createCaption()
{
    if (HTMLTableCaptionElement* existingCaption = caption())
        return existingCaption;
    RefPtr<HTMLTableCaptionElement> caption = new HTMLTableCaptionElement(document());
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

    HTMLTableRowElement* lastRow = 0;
    HTMLTableRowElement* row = 0;
    if (index == -1)
        lastRow = HTMLTableRowsCollection::lastRow(this);
    else {
        for (int i = 0; i <= index; ++i) {
            row = HTMLTableRowsCollection::rowAfter(this, lastRow);
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

    Node* parent;
    if (lastRow)
        parent = row ? row->parent() : lastRow->parent();
    else {
        parent = lastBody();
        if (!parent) {
            RefPtr<HTMLTableSectionElement> newBody = new HTMLTableSectionElement(tbodyTag, document());
            RefPtr<HTMLTableRowElement> newRow = new HTMLTableRowElement(document());
            newBody->appendChild(newRow, ec);
            appendChild(newBody.release(), ec);
            return newRow.release();
        }
    }

    RefPtr<HTMLTableRowElement> newRow = new HTMLTableRowElement(document());
    parent->insertBefore(newRow, row, ec);
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

ContainerNode* HTMLTableElement::addChild(PassRefPtr<Node> child)
{
    if (child->hasTagName(formTag)) {
        // First add the child.
        HTMLElement::addChild(child);

        // Now simply return ourselves as the container to insert into.
        // This has the effect of demoting the form to a leaf and moving it safely out of the way.
        return this;
    }

    return HTMLElement::addChild(child.get());
}

bool HTMLTableElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == backgroundAttr) {
        result = (MappedAttributeEntry)(eLastEntry + document()->docID());
        return false;
    }
    
    if (attrName == widthAttr ||
        attrName == heightAttr ||
        attrName == bgcolorAttr ||
        attrName == cellspacingAttr ||
        attrName == vspaceAttr ||
        attrName == hspaceAttr ||
        attrName == valignAttr) {
        result = eUniversal;
        return false;
    }
    
    if (attrName == bordercolorAttr || attrName == frameAttr || attrName == rulesAttr) {
        result = eUniversal;
        return true;
    }
    
    if (attrName == borderAttr) {
        result = eTable;
        return true;
    }
    
    if (attrName == alignAttr) {
        result = eTable;
        return false;
    }

    return HTMLElement::mapToEntry(attrName, result);
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
       n->setChanged();

    return cellChanged;
}

void HTMLTableElement::parseMappedAttribute(MappedAttribute* attr)
{
    CellBorders bordersBefore = cellBorders();
    unsigned short oldPadding = m_padding;

    if (attr->name() == widthAttr)
        addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    else if (attr->name() == heightAttr)
        addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    else if (attr->name() == borderAttr)  {
        m_borderAttr = true;
        if (attr->decl()) {
            RefPtr<CSSValue> val = attr->decl()->getPropertyCSSValue(CSS_PROP_BORDER_LEFT_WIDTH);
            if (val && val->isPrimitiveValue()) {
                CSSPrimitiveValue* primVal = static_cast<CSSPrimitiveValue*>(val.get());
                m_borderAttr = primVal->getDoubleValue(CSSPrimitiveValue::CSS_NUMBER);
            }
        } else if (!attr->isNull()) {
            int border = 0;
            if (attr->isEmpty())
                border = 1;
            else
                border = attr->value().toInt();
            m_borderAttr = border;
            addCSSLength(attr, CSS_PROP_BORDER_WIDTH, String::number(border));
        }
    } else if (attr->name() == bgcolorAttr)
        addCSSColor(attr, CSS_PROP_BACKGROUND_COLOR, attr->value());
    else if (attr->name() == bordercolorAttr) {
        m_borderColorAttr = attr->decl();
        if (!attr->decl() && !attr->isEmpty()) {
            addCSSColor(attr, CSS_PROP_BORDER_COLOR, attr->value());
            m_borderColorAttr = true;
        }
    } else if (attr->name() == backgroundAttr) {
        String url = parseURL(attr->value());
        if (!url.isEmpty())
            addCSSImageProperty(attr, CSS_PROP_BACKGROUND_IMAGE, document()->completeURL(url));
    } else if (attr->name() == frameAttr) {
        // Cache the value of "frame" so that the table can examine it later.
        m_frameAttr = false;
        
        // Whether or not to hide the top/right/bottom/left borders.
        const int cTop = 0;
        const int cRight = 1;
        const int cBottom = 2;
        const int cLeft = 3;
        bool borders[4] = { false, false, false, false };
        
        // void, above, below, hsides, vsides, lhs, rhs, box, border
        if (equalIgnoringCase(attr->value(), "void"))
            m_frameAttr = true;
        else if (equalIgnoringCase(attr->value(), "above")) {
            m_frameAttr = true;
            borders[cTop] = true;
        } else if (equalIgnoringCase(attr->value(), "below")) {
            m_frameAttr = true;
            borders[cBottom] = true;
        } else if (equalIgnoringCase(attr->value(), "hsides")) {
            m_frameAttr = true;
            borders[cTop] = borders[cBottom] = true;
        } else if (equalIgnoringCase(attr->value(), "vsides")) {
            m_frameAttr = true;
            borders[cLeft] = borders[cRight] = true;
        } else if (equalIgnoringCase(attr->value(), "lhs")) {
            m_frameAttr = true;
            borders[cLeft] = true;
        } else if (equalIgnoringCase(attr->value(), "rhs")) {
            m_frameAttr = true;
            borders[cRight] = true;
        } else if (equalIgnoringCase(attr->value(), "box") ||
                   equalIgnoringCase(attr->value(), "border")) {
            m_frameAttr = true;
            borders[cTop] = borders[cBottom] = borders[cLeft] = borders[cRight] = true;
        }
        
        // Now map in the border styles of solid and hidden respectively.
        if (m_frameAttr) {
            addCSSProperty(attr, CSS_PROP_BORDER_TOP_WIDTH, CSS_VAL_THIN);
            addCSSProperty(attr, CSS_PROP_BORDER_BOTTOM_WIDTH, CSS_VAL_THIN);
            addCSSProperty(attr, CSS_PROP_BORDER_LEFT_WIDTH, CSS_VAL_THIN);
            addCSSProperty(attr, CSS_PROP_BORDER_RIGHT_WIDTH, CSS_VAL_THIN);
            addCSSProperty(attr, CSS_PROP_BORDER_TOP_STYLE, borders[cTop] ? CSS_VAL_SOLID : CSS_VAL_HIDDEN);
            addCSSProperty(attr, CSS_PROP_BORDER_BOTTOM_STYLE, borders[cBottom] ? CSS_VAL_SOLID : CSS_VAL_HIDDEN);
            addCSSProperty(attr, CSS_PROP_BORDER_LEFT_STYLE, borders[cLeft] ? CSS_VAL_SOLID : CSS_VAL_HIDDEN);
            addCSSProperty(attr, CSS_PROP_BORDER_RIGHT_STYLE, borders[cRight] ? CSS_VAL_SOLID : CSS_VAL_HIDDEN);
        }
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
        
        // The presence of a valid rules attribute causes border collapsing to be enabled.
        if (m_rulesAttr != UnsetRules)
            addCSSProperty(attr, CSS_PROP_BORDER_COLLAPSE, CSS_VAL_COLLAPSE);
    } else if (attr->name() == cellspacingAttr) {
        if (!attr->value().isEmpty())
            addCSSLength(attr, CSS_PROP_BORDER_SPACING, attr->value());
    } else if (attr->name() == cellpaddingAttr) {
        if (!attr->value().isEmpty())
            m_padding = max(0, attr->value().toInt());
        else
            m_padding = 1;
    } else if (attr->name() == colsAttr) {
        // ###
    } else if (attr->name() == vspaceAttr) {
        addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
    } else if (attr->name() == hspaceAttr) {
        addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
    } else if (attr->name() == alignAttr) {
        if (!attr->value().isEmpty()) {
            if (equalIgnoringCase(attr->value(), "center")) {
                addCSSProperty(attr, CSS_PROP_MARGIN_LEFT, CSS_VAL_AUTO);
                addCSSProperty(attr, CSS_PROP_MARGIN_RIGHT, CSS_VAL_AUTO);
            } else
                addCSSProperty(attr, CSS_PROP_FLOAT, attr->value());
        }
    } else if (attr->name() == valignAttr) {
        if (!attr->value().isEmpty())
            addCSSProperty(attr, CSS_PROP_VERTICAL_ALIGN, attr->value());
    } else
        HTMLElement::parseMappedAttribute(attr);

    if (bordersBefore != cellBorders() || oldPadding != m_padding) {
        if (oldPadding != m_padding)
            m_paddingDecl = 0;
        bool cellChanged = false;
        for (Node* child = firstChild(); child; child = child->nextSibling())
            cellChanged |= setTableCellsChanged(child);
        if (cellChanged)
            setChanged();
    }
}

void HTMLTableElement::additionalAttributeStyleDecls(Vector<CSSMutableStyleDeclaration*>& results)
{
    if ((!m_borderAttr && !m_borderColorAttr) || m_frameAttr)
        return;
        
    MappedAttribute attr(tableborderAttr, m_borderColorAttr ? "solid" : "outset");
    CSSMappedAttributeDeclaration* decl = getMappedAttributeDecl(ePersistent, &attr);
    if (!decl) {
        decl = new CSSMappedAttributeDeclaration(0);
        decl->setParent(document()->elementSheet());
        decl->setNode(this);
        decl->setStrictParsing(false); // Mapped attributes are just always quirky.
        
        decl->ref(); // This single ref pins us in the table until the document dies.

        int v = m_borderColorAttr ? CSS_VAL_SOLID : CSS_VAL_OUTSET;
        decl->setProperty(CSS_PROP_BORDER_TOP_STYLE, v, false);
        decl->setProperty(CSS_PROP_BORDER_BOTTOM_STYLE, v, false);
        decl->setProperty(CSS_PROP_BORDER_LEFT_STYLE, v, false);
        decl->setProperty(CSS_PROP_BORDER_RIGHT_STYLE, v, false);

        setMappedAttributeDecl(ePersistent, &attr, decl);
        decl->setParent(0);
        decl->setNode(0);
        decl->setMappedState(ePersistent, attr.name(), attr.value());
    }
    
    
    results.append(decl);
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

void HTMLTableElement::addSharedCellDecls(Vector<CSSMutableStyleDeclaration*>& results)
{
    addSharedCellBordersDecl(results);
    addSharedCellPaddingDecl(results);
}

void HTMLTableElement::addSharedCellBordersDecl(Vector<CSSMutableStyleDeclaration*>& results)
{
    CellBorders borders = cellBorders();

    static AtomicString cellBorderNames[] = { "none", "solid", "inset", "solid-cols", "solid-rows" };
    MappedAttribute attr(cellborderAttr, cellBorderNames[borders]);

    CSSMappedAttributeDeclaration* decl = getMappedAttributeDecl(ePersistent, &attr);
    if (!decl) {
        decl = new CSSMappedAttributeDeclaration(0);
        decl->setParent(document()->elementSheet());
        decl->setNode(this);
        decl->setStrictParsing(false); // Mapped attributes are just always quirky.
        
        decl->ref(); // This single ref pins us in the table until the document dies.
        
        switch (borders) {
            case SolidBordersColsOnly:
                decl->setProperty(CSS_PROP_BORDER_LEFT_WIDTH, CSS_VAL_THIN, false);
                decl->setProperty(CSS_PROP_BORDER_RIGHT_WIDTH, CSS_VAL_THIN, false);
                decl->setProperty(CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID, false);
                decl->setProperty(CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID, false);
                decl->setProperty(CSS_PROP_BORDER_COLOR, "inherit", false);
                break;
            case SolidBordersRowsOnly:
                decl->setProperty(CSS_PROP_BORDER_TOP_WIDTH, CSS_VAL_THIN, false);
                decl->setProperty(CSS_PROP_BORDER_BOTTOM_WIDTH, CSS_VAL_THIN, false);
                decl->setProperty(CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID, false);
                decl->setProperty(CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID, false);
                decl->setProperty(CSS_PROP_BORDER_COLOR, "inherit", false);
                break;
            case SolidBorders:
                decl->setProperty(CSS_PROP_BORDER_WIDTH, "1px", false);
                decl->setProperty(CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID, false);
                decl->setProperty(CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID, false);
                decl->setProperty(CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID, false);
                decl->setProperty(CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID, false);
                decl->setProperty(CSS_PROP_BORDER_COLOR, "inherit", false);
                break;
            case InsetBorders:
                decl->setProperty(CSS_PROP_BORDER_WIDTH, "1px", false);
                decl->setProperty(CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_INSET, false);
                decl->setProperty(CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_INSET, false);
                decl->setProperty(CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_INSET, false);
                decl->setProperty(CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_INSET, false);
                decl->setProperty(CSS_PROP_BORDER_COLOR, "inherit", false);
                break;
            case NoBorders:
                decl->setProperty(CSS_PROP_BORDER_WIDTH, "0", false);
                break;
        }

        setMappedAttributeDecl(ePersistent, &attr, decl);
        decl->setParent(0);
        decl->setNode(0);
        decl->setMappedState(ePersistent, attr.name(), attr.value());
    }
    
    results.append(decl);
}

void HTMLTableElement::addSharedCellPaddingDecl(Vector<CSSMutableStyleDeclaration*>& results)
{
    if (m_padding == 0)
        return;

    if (!m_paddingDecl) {
        String numericStr = String::number(m_padding);
        MappedAttribute attr(cellpaddingAttr, numericStr);
        m_paddingDecl = getMappedAttributeDecl(eUniversal, &attr);
        if (!m_paddingDecl) {
            m_paddingDecl = new CSSMappedAttributeDeclaration(0);
            m_paddingDecl->setParent(document()->elementSheet());
            m_paddingDecl->setNode(this);
            m_paddingDecl->setStrictParsing(false); // Mapped attributes are just always quirky.
            
            m_paddingDecl->setProperty(CSS_PROP_PADDING_TOP, numericStr, false);
            m_paddingDecl->setProperty(CSS_PROP_PADDING_RIGHT, numericStr, false);
            m_paddingDecl->setProperty(CSS_PROP_PADDING_BOTTOM, numericStr, false);
            m_paddingDecl->setProperty(CSS_PROP_PADDING_LEFT, numericStr, false);
        }
        setMappedAttributeDecl(eUniversal, &attr, m_paddingDecl.get());
        m_paddingDecl->setParent(0);
        m_paddingDecl->setNode(0);
        m_paddingDecl->setMappedState(eUniversal, attr.name(), attr.value());
    }
    
    results.append(m_paddingDecl.get());
}

void HTMLTableElement::addSharedGroupDecls(bool rows, Vector<CSSMutableStyleDeclaration*>& results)

{
    if (m_rulesAttr != GroupsRules)
        return;

    MappedAttribute attr(rulesAttr, rows ? "rowgroups" : "colgroups");
    CSSMappedAttributeDeclaration* decl = getMappedAttributeDecl(ePersistent, &attr);
    if (!decl) {
        decl = new CSSMappedAttributeDeclaration(0);
        decl->setParent(document()->elementSheet());
        decl->setNode(this);
        decl->setStrictParsing(false); // Mapped attributes are just always quirky.
        
        decl->ref(); // This single ref pins us in the table until the document dies.
        
        if (rows) {
            decl->setProperty(CSS_PROP_BORDER_TOP_WIDTH, CSS_VAL_THIN, false);
            decl->setProperty(CSS_PROP_BORDER_BOTTOM_WIDTH, CSS_VAL_THIN, false);
            decl->setProperty(CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID, false);
            decl->setProperty(CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID, false);
        } else {
            decl->setProperty(CSS_PROP_BORDER_LEFT_WIDTH, CSS_VAL_THIN, false);
            decl->setProperty(CSS_PROP_BORDER_RIGHT_WIDTH, CSS_VAL_THIN, false);
            decl->setProperty(CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID, false);
            decl->setProperty(CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID, false);
        }

        setMappedAttributeDecl(ePersistent, &attr, decl);
        decl->setParent(0);
        decl->setNode(0);
        decl->setMappedState(ePersistent, attr.name(), attr.value());
    }

    results.append(decl);
}

void HTMLTableElement::attach()
{
    ASSERT(!m_attached);
    HTMLElement::attach();
}

bool HTMLTableElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == backgroundAttr;
}

PassRefPtr<HTMLCollection> HTMLTableElement::rows()
{
    return new HTMLTableRowsCollection(this);
}

PassRefPtr<HTMLCollection> HTMLTableElement::tBodies()
{
    return new HTMLCollection(this, HTMLCollection::TableTBodies);
}

String HTMLTableElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLTableElement::setAlign(const String &value)
{
    setAttribute(alignAttr, value);
}

String HTMLTableElement::bgColor() const
{
    return getAttribute(bgcolorAttr);
}

void HTMLTableElement::setBgColor(const String &value)
{
    setAttribute(bgcolorAttr, value);
}

String HTMLTableElement::border() const
{
    return getAttribute(borderAttr);
}

void HTMLTableElement::setBorder(const String &value)
{
    setAttribute(borderAttr, value);
}

String HTMLTableElement::cellPadding() const
{
    return getAttribute(cellpaddingAttr);
}

void HTMLTableElement::setCellPadding(const String &value)
{
    setAttribute(cellpaddingAttr, value);
}

String HTMLTableElement::cellSpacing() const
{
    return getAttribute(cellspacingAttr);
}

void HTMLTableElement::setCellSpacing(const String &value)
{
    setAttribute(cellspacingAttr, value);
}

String HTMLTableElement::frame() const
{
    return getAttribute(frameAttr);
}

void HTMLTableElement::setFrame(const String &value)
{
    setAttribute(frameAttr, value);
}

String HTMLTableElement::rules() const
{
    return getAttribute(rulesAttr);
}

void HTMLTableElement::setRules(const String &value)
{
    setAttribute(rulesAttr, value);
}

String HTMLTableElement::summary() const
{
    return getAttribute(summaryAttr);
}

void HTMLTableElement::setSummary(const String &value)
{
    setAttribute(summaryAttr, value);
}

String HTMLTableElement::width() const
{
    return getAttribute(widthAttr);
}

void HTMLTableElement::setWidth(const String &value)
{
    setAttribute(widthAttr, value);
}

}
