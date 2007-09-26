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
#include "HTMLTableElement.h"

#include "CSSHelper.h"
#include "CSSPropertyNames.h"
#include "CSSStyleSheet.h"
#include "CSSValueKeywords.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "HTMLCollection.h"
#include "HTMLNames.h"
#include "HTMLTableCaptionElement.h"
#include "HTMLTableSectionElement.h"
#include "RenderTable.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

HTMLTableElement::HTMLTableElement(Document *doc)
    : HTMLElement(tableTag, doc)
    , m_head(0)
    , m_foot(0)
    , m_firstBody(0)
    , m_caption(0)
    , m_borderAttr(false)
    , m_borderColorAttr(false)
    , m_frameAttr(false)
    , m_rulesAttr(UnsetRules)
    , m_padding(1)
{
}

HTMLTableElement::~HTMLTableElement()
{
    if (m_firstBody)
        m_firstBody->deref();
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

Node* HTMLTableElement::setCaption(HTMLTableCaptionElement* c)
{
    ExceptionCode ec = 0;
    if (Node* oc = m_caption)
        replaceChild(c, oc, ec);
    else
        insertBefore(c, firstChild(), ec);
    m_caption = c;
    return m_caption;
}

Node* HTMLTableElement::setTHead(HTMLTableSectionElement* s)
{
    ExceptionCode ec = 0;
    if (Node* h = m_head)
        replaceChild(s, h, ec);
    else if (m_foot)
        insertBefore(s, m_foot, ec);
    else if (m_firstBody)
        insertBefore(s, m_firstBody, ec);
    else
        appendChild(s, ec);
    m_head = s;
    return m_head;
}

Node* HTMLTableElement::setTFoot(HTMLTableSectionElement *s)
{
    ExceptionCode ec = 0;
    if (Node *f = m_foot)
        replaceChild(s, f, ec);
    else if (m_firstBody)
        insertBefore(s, m_firstBody, ec);
    else
        appendChild(s, ec);
    m_foot = s;
    return m_foot;
}

Node* HTMLTableElement::setTBody(HTMLTableSectionElement *s)
{
    ExceptionCode ec = 0;
    Node* r;
    s->ref();
    if (Node *fb = m_firstBody) {
        replaceChild(s, fb, ec);
        fb->deref();
        r = s;
    } else
        appendChild(s, ec);
    m_firstBody = s;
    return m_firstBody;
}

HTMLElement *HTMLTableElement::createTHead()
{
    if (!m_head) {
        ExceptionCode ec = 0;
        m_head = new HTMLTableSectionElement(theadTag, document());
        if (m_foot)
            insertBefore(m_head, m_foot, ec);
        else if (m_firstBody)
            insertBefore(m_head, m_firstBody, ec);
        else
            appendChild(m_head, ec);
    }
    return m_head;
}

void HTMLTableElement::deleteTHead()
{
    if (m_head) {
        ExceptionCode ec = 0;
        m_head->ref();
        HTMLElement::removeChild(m_head, ec);
        m_head->deref();
    }
    m_head = 0;
}

HTMLElement *HTMLTableElement::createTFoot()
{
    if (!m_foot) {
        ExceptionCode ec = 0;
        m_foot = new HTMLTableSectionElement(tfootTag, document());
        if (m_firstBody)
            insertBefore(m_foot, m_firstBody, ec);
        else
            appendChild(m_foot, ec);
    }
    return m_foot;
}

void HTMLTableElement::deleteTFoot()
{
    if (m_foot) {
        ExceptionCode ec = 0;
        m_foot->ref();
        HTMLElement::removeChild(m_foot, ec);
        m_foot->deref();
    }
    m_foot = 0;
}

HTMLElement *HTMLTableElement::createCaption()
{
    if (!m_caption) {
        ExceptionCode ec = 0;
        m_caption = new HTMLTableCaptionElement(document());
        insertBefore(m_caption, firstChild(), ec);
    }
    return m_caption;
}

void HTMLTableElement::deleteCaption()
{
    if (m_caption) {
        ExceptionCode ec = 0;
        m_caption->ref();
        HTMLElement::removeChild(m_caption, ec);
        m_caption->deref();
    }
    m_caption = 0;
}

PassRefPtr<HTMLElement> HTMLTableElement::insertRow(int index, ExceptionCode& ec)
{
    // The DOM requires that we create a tbody if the table is empty
    // (cf DOM2TS HTMLTableElement31 test)
    // (note: this is different from "if the table has no sections", since we can have
    // <TABLE><TR>)
    if (!m_firstBody && !m_head && !m_foot)
        setTBody(new HTMLTableSectionElement(tbodyTag, document()));

    // IE treats index=-1 as default value meaning 'append after last'
    // This isn't in the DOM. So, not implemented yet.
    HTMLTableSectionElement* section = 0L;
    HTMLTableSectionElement* lastSection = 0L;
    Node *node = firstChild();
    bool append = (index == -1);
    bool found = false;
    for (; node && (index>=0 || append) ; node = node->nextSibling())
    {
        // there could be 2 tfoot elements in the table. Only the first one is the "foot", that's why we have the more
        // complicated if statement below.
        if (node != m_foot && (node->hasTagName(theadTag) || node->hasTagName(tfootTag) || node->hasTagName(tbodyTag))) {
            section = static_cast<HTMLTableSectionElement*>(node);
            lastSection = section;
            if (!append) {
                int rows = section->numRows();
                if (rows >= index) {
                    found = true;
                    break;
                } else
                    index -= rows;
            }
        }
    }
    if (!found && m_foot)
        section = static_cast<HTMLTableSectionElement*>(m_foot);

    // Index == 0 means "insert before first row in current section"
    // or "append after last row" (if there's no current section anymore)
    if (!section && (index == 0 || append)) {
        section = lastSection;
        index = section ? section->numRows() : 0;
    }
    if (section && (index >= 0 || append))
        return section->insertRow(index, ec);
    else {
        // No more sections => index is too big
        ec = INDEX_SIZE_ERR;
        return 0;
    }
}

void HTMLTableElement::deleteRow(int index, ExceptionCode& ec)
{
    HTMLTableSectionElement* section = 0L;
    Node *node = firstChild();
    bool lastRow = index == -1;
    HTMLTableSectionElement* lastSection = 0L;
    bool found = false;
    for (; node ; node = node->nextSibling())
    {
        if (node != m_foot && (node->hasTagName(theadTag) || node->hasTagName(tfootTag) || 
            node->hasTagName(tbodyTag))) {
            section = static_cast<HTMLTableSectionElement*>(node);
            lastSection = section;
            int rows = section->numRows();
            if (!lastRow) {
                if (rows > index) {
                    found = true;
                    break;
                } else
                    index -= rows;
            }
        }
        section = 0L;
    }
    if (!found && m_foot)
        section = static_cast<HTMLTableSectionElement*>(m_foot);

    if (lastRow && lastSection)
        lastSection->deleteRow(-1, ec);
    else if (section && index >= 0 && index < section->numRows())
        section->deleteRow(index, ec);
    else
        ec = INDEX_SIZE_ERR;
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

    // The creation of <tbody> elements relies on the "childAllowed" check,
    // so we need to do it even for XML documents.
    ASSERT(child->nodeType() != DOCUMENT_FRAGMENT_NODE);
    if (!document()->isHTMLDocument() && !childAllowed(child.get()))
        return 0;

    ContainerNode* container = HTMLElement::addChild(child.get());
    if (container) {
        if (!m_caption && child->hasTagName(captionTag))
            m_caption = static_cast<HTMLTableCaptionElement*>(child.get());
        else if (!m_head && child->hasTagName(theadTag))
            m_head = static_cast<HTMLTableSectionElement*>(child.get());
        else if (!m_foot && child->hasTagName(tfootTag))
            m_foot = static_cast<HTMLTableSectionElement*>(child.get());
        else if (!m_firstBody && child->hasTagName(tbodyTag)) {
            m_firstBody = static_cast<HTMLTableSectionElement*>(child.get());
            m_firstBody->ref();
        }
    }
    return container;
}

void HTMLTableElement::childrenChanged()
{
    HTMLElement::childrenChanged();
    
    if (m_firstBody && m_firstBody->parentNode() != this) {
        m_firstBody->deref();
        m_firstBody = 0;
    }
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

void HTMLTableElement::parseMappedAttribute(MappedAttribute *attr)
{
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
        // Cache the value of "rules" so that the pieces of the table can examine it later.
        TableRules oldRules = m_rulesAttr;
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
        if (oldRules != m_rulesAttr && m_firstBody)
            if (setTableCellsChanged(m_firstBody))
                setChanged();
    } else if (attr->name() == cellspacingAttr) {
        if (!attr->value().isEmpty())
            addCSSLength(attr, CSS_PROP_BORDER_SPACING, attr->value());
    } else if (attr->name() == cellpaddingAttr) {
        if (!attr->value().isEmpty())
            m_padding = max(0, attr->value().toInt());
        else
            m_padding = 1;
        if (renderer() && renderer()->isTable()) {
            static_cast<RenderTable*>(renderer())->setCellPadding(m_padding);
            if (!renderer()->needsLayout())
                renderer()->setNeedsLayout(true);
        }
    } else if (attr->name() == colsAttr) {
        // ###
    } else if (attr->name() == vspaceAttr) {
        addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
    } else if (attr->name() == hspaceAttr) {
        addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
    } else if (attr->name() == alignAttr) {
        if (!attr->value().isEmpty())
            addCSSProperty(attr, CSS_PROP_FLOAT, attr->value());
    } else if (attr->name() == valignAttr) {
        if (!attr->value().isEmpty())
            addCSSProperty(attr, CSS_PROP_VERTICAL_ALIGN, attr->value());
    } else
        HTMLElement::parseMappedAttribute(attr);
}

CSSMutableStyleDeclaration* HTMLTableElement::additionalAttributeStyleDecl()
{
    if ((!m_borderAttr && !m_borderColorAttr) || m_frameAttr)
        return 0;
        
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
    return decl;
}

CSSMutableStyleDeclaration* HTMLTableElement::getSharedCellDecl()
{
    MappedAttribute attr(cellborderAttr, m_rulesAttr == AllRules ? "solid-all" : 
                                         (m_rulesAttr == ColsRules ? "solid-cols" : 
                                         (m_rulesAttr == RowsRules ? "solid-rows" : (!m_borderAttr || m_rulesAttr == GroupsRules || m_rulesAttr == NoneRules ? "none" : (m_borderColorAttr ? "solid" : "inset")))));

    CSSMappedAttributeDeclaration* decl = getMappedAttributeDecl(ePersistent, &attr);
    if (!decl) {
        decl = new CSSMappedAttributeDeclaration(0);
        decl->setParent(document()->elementSheet());
        decl->setNode(this);
        decl->setStrictParsing(false); // Mapped attributes are just always quirky.
        
        decl->ref(); // This single ref pins us in the table until the document dies.
        
        if (m_rulesAttr == ColsRules) {
            decl->setProperty(CSS_PROP_BORDER_LEFT_WIDTH, CSS_VAL_THIN, false);
            decl->setProperty(CSS_PROP_BORDER_RIGHT_WIDTH, CSS_VAL_THIN, false);
            decl->setProperty(CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID, false);
            decl->setProperty(CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID, false);
            decl->setProperty(CSS_PROP_BORDER_COLOR, "inherit", false);
        } else if (m_rulesAttr == RowsRules) {
            decl->setProperty(CSS_PROP_BORDER_TOP_WIDTH, CSS_VAL_THIN, false);
            decl->setProperty(CSS_PROP_BORDER_BOTTOM_WIDTH, CSS_VAL_THIN, false);
            decl->setProperty(CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID, false);
            decl->setProperty(CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID, false);
            decl->setProperty(CSS_PROP_BORDER_COLOR, "inherit", false);
        } else if (m_rulesAttr != GroupsRules && m_rulesAttr != NoneRules && (m_borderAttr || m_rulesAttr == AllRules)) {
            decl->setProperty(CSS_PROP_BORDER_WIDTH, "1px", false);
             int v = (m_borderColorAttr || m_rulesAttr == AllRules) ? CSS_VAL_SOLID : CSS_VAL_INSET;
            decl->setProperty(CSS_PROP_BORDER_TOP_STYLE, v, false);
            decl->setProperty(CSS_PROP_BORDER_BOTTOM_STYLE, v, false);
            decl->setProperty(CSS_PROP_BORDER_LEFT_STYLE, v, false);
            decl->setProperty(CSS_PROP_BORDER_RIGHT_STYLE, v, false);
            decl->setProperty(CSS_PROP_BORDER_COLOR, "inherit", false);
        }
        else
            decl->setProperty(CSS_PROP_BORDER_WIDTH, "0", false);

        setMappedAttributeDecl(ePersistent, &attr, decl);
        decl->setParent(0);
        decl->setNode(0);
        decl->setMappedState(ePersistent, attr.name(), attr.value());
    }
    return decl;
}

CSSMutableStyleDeclaration* HTMLTableElement::getSharedGroupDecl(bool rows)
{
    if (m_rulesAttr != GroupsRules)
        return 0;

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

    return decl;
}

void HTMLTableElement::attach()
{
    ASSERT(!m_attached);
    HTMLElement::attach();
    if (renderer() && renderer()->isTable())
        static_cast<RenderTable*>(renderer())->setCellPadding(m_padding);
}

bool HTMLTableElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == backgroundAttr;
}

PassRefPtr<HTMLCollection> HTMLTableElement::rows()
{
    return new HTMLCollection(this, HTMLCollection::TableRows);
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
