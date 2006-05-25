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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "config.h"
#include "HTMLTableElement.h"

#include "csshelper.h"
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

namespace WebCore {

using namespace HTMLNames;

HTMLTableElement::HTMLTableElement(Document *doc)
    : HTMLElement(tableTag, doc)
    , head(0)
    , foot(0)
    , firstBody(0)
    , tCaption(0)
    , m_noBorder(true)
    , m_solid(false)
    , padding(1)
{
}

HTMLTableElement::~HTMLTableElement()
{
    if (firstBody)
        firstBody->deref();
}

bool HTMLTableElement::checkDTD(const Node* newChild)
{
    return newChild->isTextNode() || newChild->hasTagName(captionTag) ||
           newChild->hasTagName(colTag) || newChild->hasTagName(colgroupTag) ||
           newChild->hasTagName(theadTag) || newChild->hasTagName(tfootTag) ||
           newChild->hasTagName(tbodyTag) || newChild->hasTagName(formTag) ||
           newChild->hasTagName(scriptTag);
}

Node* HTMLTableElement::setCaption(HTMLTableCaptionElement* c)
{
    ExceptionCode ec = 0;
    if (Node* oc = tCaption)
        replaceChild(c, oc, ec);
    else
        insertBefore(c, firstChild(), ec);
    tCaption = c;
    return tCaption;
}

Node* HTMLTableElement::setTHead(HTMLTableSectionElement* s)
{
    ExceptionCode ec = 0;
    if (Node* h = head)
        replaceChild(s, h, ec);
    else if (foot)
        insertBefore(s, foot, ec);
    else if (firstBody)
        insertBefore(s, firstBody, ec);
    else
        appendChild(s, ec);
    head = s;
    return head;
}

Node* HTMLTableElement::setTFoot(HTMLTableSectionElement *s)
{
    ExceptionCode ec = 0;
    if (Node *f = foot)
        replaceChild(s, f, ec);
    else if (firstBody)
        insertBefore(s, firstBody, ec);
    else
        appendChild(s, ec);
    foot = s;
    return foot;
}

Node* HTMLTableElement::setTBody(HTMLTableSectionElement *s)
{
    ExceptionCode ec = 0;
    Node* r;
    s->ref();
    if (Node *fb = firstBody) {
        replaceChild(s, fb, ec);
        fb->deref();
        r = s;
    } else
        appendChild(s, ec);
    firstBody = s;
    return firstBody;
}

HTMLElement *HTMLTableElement::createTHead()
{
    if (!head) {
        ExceptionCode ec = 0;
        head = new HTMLTableSectionElement(theadTag, document(), true /* implicit */);
        if (foot)
            insertBefore(head, foot, ec);
        else if (firstBody)
            insertBefore(head, firstBody, ec);
        else
            appendChild(head, ec);
    }
    return head;
}

void HTMLTableElement::deleteTHead()
{
    if (head) {
        ExceptionCode ec = 0;
        head->ref();
        HTMLElement::removeChild(head, ec);
        head->deref();
    }
    head = 0;
}

HTMLElement *HTMLTableElement::createTFoot()
{
    if (!foot) {
        ExceptionCode ec = 0;
        foot = new HTMLTableSectionElement(tfootTag, document(), true /*implicit */);
        if (firstBody)
            insertBefore(foot, firstBody, ec);
        else
            appendChild(foot, ec);
    }
    return foot;
}

void HTMLTableElement::deleteTFoot()
{
    if (foot) {
        ExceptionCode ec = 0;
        foot->ref();
        HTMLElement::removeChild(foot, ec);
        foot->deref();
    }
    foot = 0;
}

HTMLElement *HTMLTableElement::createCaption()
{
    if (!tCaption) {
        ExceptionCode ec = 0;
        tCaption = new HTMLTableCaptionElement(document());
        insertBefore(tCaption, firstChild(), ec);
    }
    return tCaption;
}

void HTMLTableElement::deleteCaption()
{
    if (tCaption) {
        ExceptionCode ec = 0;
        tCaption->ref();
        HTMLElement::removeChild(tCaption, ec);
        tCaption->deref();
    }
    tCaption = 0;
}

HTMLElement *HTMLTableElement::insertRow(int index, ExceptionCode& ec)
{
    // The DOM requires that we create a tbody if the table is empty
    // (cf DOM2TS HTMLTableElement31 test)
    // (note: this is different from "if the table has no sections", since we can have
    // <TABLE><TR>)
    if (!firstBody && !head && !foot)
        setTBody(new HTMLTableSectionElement(tbodyTag, document(), true /* implicit */));

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
        if (node != foot && (node->hasTagName(theadTag) || node->hasTagName(tfootTag) || node->hasTagName(tbodyTag)))
        {
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
    if (!found && foot)
        section = static_cast<HTMLTableSectionElement*>(foot);

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
        return 0L;
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
        if (node != foot && (node->hasTagName(theadTag) || node->hasTagName(tfootTag) || 
            node->hasTagName(tbodyTag))) {
            section = static_cast<HTMLTableSectionElement*>(node);
            lastSection = section;
            int rows = section->numRows();
            if (!lastRow)
            {
                if (rows > index) {
                    found = true;
                    break;
                } else
                    index -= rows;
            }
        }
        section = 0L;
    }
    if (!found && foot)
        section = static_cast<HTMLTableSectionElement*>(foot);

    if (lastRow)
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
    assert(child->nodeType() != DOCUMENT_FRAGMENT_NODE);
    if (!document()->isHTMLDocument() && !childAllowed(child.get()))
        return 0;

    ContainerNode* container = HTMLElement::addChild(child.get());
    if (container) {
        if (!tCaption && child->hasTagName(captionTag))
            tCaption = static_cast<HTMLTableCaptionElement*>(child.get());
        else if (!head && child->hasTagName(theadTag))
            head = static_cast<HTMLTableSectionElement*>(child.get());
        else if (!foot && child->hasTagName(tfootTag))
            foot = static_cast<HTMLTableSectionElement*>(child.get());
        else if (!firstBody && child->hasTagName(tbodyTag)) {
            firstBody = static_cast<HTMLTableSectionElement*>(child.get());
            firstBody->ref();
        }
    }
    return container;
}

void HTMLTableElement::childrenChanged()
{
    HTMLElement::childrenChanged();
    
    if (firstBody && firstBody->parentNode() != this) {
        firstBody->deref();
        firstBody = 0;
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
    
    if (attrName == bordercolorAttr) {
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

void HTMLTableElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == widthAttr)
        addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    else if (attr->name() == heightAttr)
        addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    else if (attr->name() == borderAttr)  {
        m_noBorder = true;
        if (attr->decl()) {
            RefPtr<CSSValue> val = attr->decl()->getPropertyCSSValue(CSS_PROP_BORDER_LEFT_WIDTH);
            if (val && val->isPrimitiveValue()) {
                CSSPrimitiveValue* primVal = static_cast<CSSPrimitiveValue*>(val.get());
                m_noBorder = !primVal->getFloatValue(CSSPrimitiveValue::CSS_NUMBER);
            }
        } else if (!attr->isNull()) {
            // ### this needs more work, as the border value is not only
            //     the border of the box, but also between the cells
            int border = 0;
            if (attr->isEmpty())
                border = 1;
            else
                border = attr->value().toInt();
#ifdef DEBUG_DRAW_BORDER
            border=1;
#endif
            m_noBorder = !border;
            addCSSLength(attr, CSS_PROP_BORDER_WIDTH, String::number(border));
        }
    } else if (attr->name() == bgcolorAttr)
        addCSSColor(attr, CSS_PROP_BACKGROUND_COLOR, attr->value());
    else if (attr->name() == bordercolorAttr) {
        m_solid = attr->decl();
        if (!attr->decl() && !attr->isEmpty()) {
            addCSSColor(attr, CSS_PROP_BORDER_COLOR, attr->value());
            addCSSProperty(attr, CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID);
            addCSSProperty(attr, CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID);
            addCSSProperty(attr, CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID);
            addCSSProperty(attr, CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID);
            m_solid = true;
        }
    } else if (attr->name() == backgroundAttr) {
        String url = WebCore::parseURL(attr->value());
        if (!url.isEmpty())
            addCSSImageProperty(attr, CSS_PROP_BACKGROUND_IMAGE, document()->completeURL(url));
    } else if (attr->name() == frameAttr) {
    } else if (attr->name() == rulesAttr) {
    } else if (attr->name() == cellspacingAttr) {
        if (!attr->value().isEmpty())
            addCSSLength(attr, CSS_PROP_BORDER_SPACING, attr->value());
    } else if (attr->name() == cellpaddingAttr) {
        if (!attr->value().isEmpty())
            padding = max(0, attr->value().toInt());
        else
            padding = 1;
        if (renderer() && renderer()->isTable()) {
            static_cast<RenderTable*>(renderer())->setCellPadding(padding);
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
    if (m_noBorder)
        return 0;
    MappedAttribute attr(tableborderAttr, m_solid ? "solid" : "outset");
    CSSMappedAttributeDeclaration* decl = getMappedAttributeDecl(ePersistent, &attr);
    if (!decl) {
        decl = new CSSMappedAttributeDeclaration(0);
        decl->setParent(document()->elementSheet());
        decl->setNode(this);
        decl->setStrictParsing(false); // Mapped attributes are just always quirky.
        
        decl->ref(); // This single ref pins us in the table until the document dies.

        int v = m_solid ? CSS_VAL_SOLID : CSS_VAL_OUTSET;
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
    MappedAttribute attr(cellborderAttr, m_noBorder ? "none" : (m_solid ? "solid" : "inset"));
    CSSMappedAttributeDeclaration* decl = getMappedAttributeDecl(ePersistent, &attr);
    if (!decl) {
        decl = new CSSMappedAttributeDeclaration(0);
        decl->setParent(document()->elementSheet());
        decl->setNode(this);
        decl->setStrictParsing(false); // Mapped attributes are just always quirky.
        
        decl->ref(); // This single ref pins us in the table until the table dies.
        
        if (m_noBorder)
            decl->setProperty(CSS_PROP_BORDER_WIDTH, "0", false);
        else {
            decl->setProperty(CSS_PROP_BORDER_WIDTH, "1px", false);
            int v = m_solid ? CSS_VAL_SOLID : CSS_VAL_INSET;
            decl->setProperty(CSS_PROP_BORDER_TOP_STYLE, v, false);
            decl->setProperty(CSS_PROP_BORDER_BOTTOM_STYLE, v, false);
            decl->setProperty(CSS_PROP_BORDER_LEFT_STYLE, v, false);
            decl->setProperty(CSS_PROP_BORDER_RIGHT_STYLE, v, false);
            decl->setProperty(CSS_PROP_BORDER_COLOR, "inherit", false);
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
    assert(!m_attached);
    HTMLElement::attach();
    if (renderer() && renderer()->isTable())
        static_cast<RenderTable*>(renderer())->setCellPadding(padding);
}

bool HTMLTableElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == backgroundAttr;
}

PassRefPtr<HTMLCollection> HTMLTableElement::rows()
{
    return new HTMLCollection(this, HTMLCollection::TABLE_ROWS);
}

PassRefPtr<HTMLCollection> HTMLTableElement::tBodies()
{
    return new HTMLCollection(this, HTMLCollection::TABLE_TBODIES);
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
