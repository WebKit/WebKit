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
#include "html_tableimpl.h"

#include "ExceptionCode.h"
#include "NodeList.h"
#include "RenderTable.h"
#include "RenderTableCell.h"
#include "RenderTableCol.h"
#include "css_stylesheetimpl.h"
#include "css_valueimpl.h"
#include "csshelper.h"
#include "CSSPropertyNames.h"
#include "cssstyleselector.h"
#include "CSSValueKeywords.h"
#include "HTMLDocument.h"

namespace WebCore {

using namespace HTMLNames;

HTMLTableElement::HTMLTableElement(Document *doc)
  : HTMLElement(tableTag, doc)
{
    tCaption = 0;
    head = 0;
    foot = 0;
    firstBody = 0;

 
    padding = 1;
    
    m_noBorder = true;
    m_solid = false;
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

Node* HTMLTableElement::setCaption( HTMLTableCaptionElement *c )
{
    ExceptionCode ec = 0;
    if (Node *oc = tCaption)
        replaceChild(c, oc, ec);
    else
        insertBefore(c, firstChild(), ec);
    tCaption = c;
    return tCaption;
}

Node* HTMLTableElement::setTHead( HTMLTableSectionElement *s )
{
    ExceptionCode ec = 0;
    if (Node *h = head)
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

Node* HTMLTableElement::setTFoot( HTMLTableSectionElement *s )
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

Node* HTMLTableElement::setTBody( HTMLTableSectionElement *s )
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

HTMLElement *HTMLTableElement::createTHead(  )
{
    if(!head)
    {
        ExceptionCode ec = 0;
        head = new HTMLTableSectionElement(theadTag, document(), true /* implicit */);
        if(foot)
            insertBefore( head, foot, ec );
        else if(firstBody)
            insertBefore( head, firstBody, ec);
        else
            appendChild(head, ec);
    }
    return head;
}

void HTMLTableElement::deleteTHead(  )
{
    if(head) {
        ExceptionCode ec = 0;
        head->ref();
        HTMLElement::removeChild(head, ec);
        head->deref();
    }
    head = 0;
}

HTMLElement *HTMLTableElement::createTFoot(  )
{
    if (!foot)
    {
        ExceptionCode ec = 0;
        foot = new HTMLTableSectionElement(tfootTag, document(), true /*implicit */);
        if (firstBody)
            insertBefore( foot, firstBody, ec );
        else
            appendChild(foot, ec);
    }
    return foot;
}

void HTMLTableElement::deleteTFoot(  )
{
    if(foot) {
        ExceptionCode ec = 0;
        foot->ref();
        HTMLElement::removeChild(foot, ec);
        foot->deref();
    }
    foot = 0;
}

HTMLElement *HTMLTableElement::createCaption(  )
{
    if(!tCaption)
    {
        ExceptionCode ec = 0;
        tCaption = new HTMLTableCaptionElement(document());
        insertBefore( tCaption, firstChild(), ec );
    }
    return tCaption;
}

void HTMLTableElement::deleteCaption(  )
{
    if(tCaption) {
        ExceptionCode ec = 0;
        tCaption->ref();
        HTMLElement::removeChild(tCaption, ec);
        tCaption->deref();
    }
    tCaption = 0;
}

HTMLElement *HTMLTableElement::insertRow( int index, ExceptionCode& ec)
{
    // The DOM requires that we create a tbody if the table is empty
    // (cf DOM2TS HTMLTableElement31 test)
    // (note: this is different from "if the table has no sections", since we can have
    // <TABLE><TR>)
    if(!firstBody && !head && !foot)
        setTBody( new HTMLTableSectionElement(tbodyTag, document(), true /* implicit */) );

    // IE treats index=-1 as default value meaning 'append after last'
    // This isn't in the DOM. So, not implemented yet.
    HTMLTableSectionElement* section = 0L;
    HTMLTableSectionElement* lastSection = 0L;
    Node *node = firstChild();
    bool append = (index == -1);
    bool found = false;
    for ( ; node && (index>=0 || append) ; node = node->nextSibling() )
    {
        // there could be 2 tfoot elements in the table. Only the first one is the "foot", that's why we have the more
        // complicated if statement below.
        if (node != foot && (node->hasTagName(theadTag) || node->hasTagName(tfootTag) || node->hasTagName(tbodyTag)))
        {
            section = static_cast<HTMLTableSectionElement *>(node);
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
    if ( !found && foot )
        section = static_cast<HTMLTableSectionElement *>(foot);

    // Index == 0 means "insert before first row in current section"
    // or "append after last row" (if there's no current section anymore)
    if ( !section && ( index == 0 || append ) )
    {
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

void HTMLTableElement::deleteRow( int index, ExceptionCode& ec)
{
    HTMLTableSectionElement* section = 0L;
    Node *node = firstChild();
    bool lastRow = index == -1;
    HTMLTableSectionElement* lastSection = 0L;
    bool found = false;
    for ( ; node ; node = node->nextSibling() )
    {
        if (node != foot && (node->hasTagName(theadTag) || node->hasTagName(tfootTag) || 
            node->hasTagName(tbodyTag))) {
            section = static_cast<HTMLTableSectionElement *>(node);
            lastSection = section;
            int rows = section->numRows();
            if ( !lastRow )
            {
                if ( rows > index ) {
                    found = true;
                    break;
                } else
                    index -= rows;
            }
        }
        section = 0L;
    }
    if ( !found && foot )
        section = static_cast<HTMLTableSectionElement *>(foot);

    if ( lastRow )
        lastSection->deleteRow( -1, ec );
    else if ( section && index >= 0 && index < section->numRows() )
        section->deleteRow( index, ec );
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
            tCaption = static_cast<HTMLTableCaptionElement *>(child.get());
        else if (!head && child->hasTagName(theadTag))
            head = static_cast<HTMLTableSectionElement *>(child.get());
        else if (!foot && child->hasTagName(tfootTag))
            foot = static_cast<HTMLTableSectionElement *>(child.get());
        else if (!firstBody && child->hasTagName(tbodyTag)) {
            firstBody = static_cast<HTMLTableSectionElement *>(child.get());
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
    if (attr->name() == widthAttr) {
        addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    } else if (attr->name() == heightAttr) {
        addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    } else if (attr->name() == borderAttr)  {
        m_noBorder = true;
        if (attr->decl()) {
            RefPtr<CSSValue> val = attr->decl()->getPropertyCSSValue(CSS_PROP_BORDER_LEFT_WIDTH);
            if (val && val->isPrimitiveValue()) {
                CSSPrimitiveValue* primVal = static_cast<CSSPrimitiveValue*>(val.get());
                m_noBorder = !primVal->getFloatValue(CSSPrimitiveValue::CSS_NUMBER);
            }
        }
        else if (!attr->isNull()) {
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
    } else if (attr->name() == bgcolorAttr) {
        addCSSColor(attr, CSS_PROP_BACKGROUND_COLOR, attr->value());
    } else if (attr->name() == bordercolorAttr) {
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
            padding = max( 0, attr->value().toInt() );
        else
            padding = 1;
        if (renderer() && renderer()->isTable()) {
            static_cast<RenderTable *>(renderer())->setCellPadding(padding);
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
        static_cast<RenderTable *>(renderer())->setCellPadding( padding );
}

bool HTMLTableElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == backgroundAttr;
}

RefPtr<HTMLCollection> HTMLTableElement::rows()
{
    return RefPtr<HTMLCollection>(new HTMLCollection(this, HTMLCollection::TABLE_ROWS));
}

RefPtr<HTMLCollection> HTMLTableElement::tBodies()
{
    return RefPtr<HTMLCollection>(new HTMLCollection(this, HTMLCollection::TABLE_TBODIES));
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

// --------------------------------------------------------------------------

bool HTMLTablePartElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == backgroundAttr) {
        result = (MappedAttributeEntry)(eLastEntry + document()->docID());
        return false;
    }
    
    if (attrName == bgcolorAttr ||
        attrName == bordercolorAttr ||
        attrName == valignAttr ||
        attrName == heightAttr) {
        result = eUniversal;
        return false;
    }
    
    if (attrName == alignAttr) {
        result = eCell; // All table parts will just share in the TD space.
        return false;
    }

    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLTablePartElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == bgcolorAttr) {
        addCSSColor(attr, CSS_PROP_BACKGROUND_COLOR, attr->value());
    } else if (attr->name() == backgroundAttr) {
        String url = WebCore::parseURL(attr->value());
        if (!url.isEmpty())
            addCSSImageProperty(attr, CSS_PROP_BACKGROUND_IMAGE, document()->completeURL(url));
    } else if (attr->name() == bordercolorAttr) {
        if (!attr->value().isEmpty()) {
            addCSSColor(attr, CSS_PROP_BORDER_COLOR, attr->value());
            addCSSProperty(attr, CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID);
            addCSSProperty(attr, CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID);
            addCSSProperty(attr, CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID);
            addCSSProperty(attr, CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID);
        }
    } else if (attr->name() == valignAttr) {
        if (!attr->value().isEmpty())
            addCSSProperty(attr, CSS_PROP_VERTICAL_ALIGN, attr->value());
    } else if (attr->name() == alignAttr) {
        const AtomicString& v = attr->value();
        if (equalIgnoringCase(v, "middle") || equalIgnoringCase(v, "center"))
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__WEBKIT_CENTER);
        else if (equalIgnoringCase(v, "absmiddle"))
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL_CENTER);
        else if (equalIgnoringCase(v, "left"))
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__WEBKIT_LEFT);
        else if (equalIgnoringCase(v, "right"))
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__WEBKIT_RIGHT);
        else
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, v);
    } else if (attr->name() == heightAttr) {
        if (!attr->value().isEmpty())
            addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    } else
        HTMLElement::parseMappedAttribute(attr);
}

// -------------------------------------------------------------------------

HTMLTableSectionElement::HTMLTableSectionElement(const QualifiedName& tagName, Document *doc, bool implicit)
    : HTMLTablePartElement(tagName, doc)
{
    m_implicit = implicit;
}

bool HTMLTableSectionElement::checkDTD(const Node* newChild)
{
    return newChild->hasTagName(trTag) || newChild->hasTagName(formTag) ||
           newChild->hasTagName(scriptTag);
}

ContainerNode* HTMLTableSectionElement::addChild(PassRefPtr<Node> child)
{
    if (child->hasTagName(formTag)) {
        // First add the child.
        HTMLTablePartElement::addChild(child);

        // Now simply return ourselves as the container to insert into.
        // This has the effect of demoting the form to a leaf and moving it safely out of the way.
        return this;
    }

    return HTMLTablePartElement::addChild(child);
}

// these functions are rather slow, since we need to get the row at
// the index... but they aren't used during usual HTML parsing anyway
HTMLElement *HTMLTableSectionElement::insertRow( int index, ExceptionCode& ec)
{
    HTMLTableRowElement *r = 0L;
    RefPtr<NodeList> children = childNodes();
    int numRows = children ? (int)children->length() : 0;
    if ( index < -1 || index > numRows ) {
        ec = INDEX_SIZE_ERR; // per the DOM
    }
    else
    {
        r = new HTMLTableRowElement(document());
        if ( numRows == index || index == -1 )
            appendChild(r, ec);
        else {
            Node *n;
            if (index < 1)
                n = firstChild();
            else
                n = children->item(index);
            insertBefore(r, n, ec );
        }
    }
    return r;
}

void HTMLTableSectionElement::deleteRow( int index, ExceptionCode& ec)
{
    RefPtr<NodeList> children = childNodes();
    int numRows = children ? (int)children->length() : 0;
    if (index == -1)
        index = numRows - 1;
    if (index >= 0 && index < numRows) {
        RefPtr<Node> row = children->item(index);
        HTMLElement::removeChild(row.get(), ec);
    } else
        ec = INDEX_SIZE_ERR;
}

int HTMLTableSectionElement::numRows() const
{
    int rows = 0;
    const Node *n = firstChild();
    while (n) {
        if (n->hasTagName(trTag))
            rows++;
        n = n->nextSibling();
    }

    return rows;
}

String HTMLTableSectionElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLTableSectionElement::setAlign(const String &value)
{
    setAttribute(alignAttr, value);
}

String HTMLTableSectionElement::ch() const
{
    return getAttribute(charAttr);
}

void HTMLTableSectionElement::setCh(const String &value)
{
    setAttribute(charAttr, value);
}

String HTMLTableSectionElement::chOff() const
{
    return getAttribute(charoffAttr);
}

void HTMLTableSectionElement::setChOff(const String &value)
{
    setAttribute(charoffAttr, value);
}

String HTMLTableSectionElement::vAlign() const
{
    return getAttribute(valignAttr);
}

void HTMLTableSectionElement::setVAlign(const String &value)
{
    setAttribute(valignAttr, value);
}

RefPtr<HTMLCollection> HTMLTableSectionElement::rows()
{
    return RefPtr<HTMLCollection>(new HTMLCollection(this, HTMLCollection::TABLE_ROWS));
}

// -------------------------------------------------------------------------

bool HTMLTableRowElement::checkDTD(const Node* newChild)
{
    return newChild->hasTagName(tdTag) || newChild->hasTagName(thTag) ||
           newChild->hasTagName(formTag) || newChild->hasTagName(scriptTag);
}

ContainerNode* HTMLTableRowElement::addChild(PassRefPtr<Node> child)
{
    if (child->hasTagName(formTag)) {
        // First add the child.
        HTMLTablePartElement::addChild(child);

        // Now simply return ourselves as the container to insert into.
        // This has the effect of demoting the form to a leaf and moving it safely out of the way.
        return this;
    }

    return HTMLTablePartElement::addChild(child);
}

int HTMLTableRowElement::rowIndex() const
{
    Node *table = parentNode();
    if (!table)
        return -1;
    table = table->parentNode();
    if (!table || !table->hasTagName(tableTag))
        return -1;

    // To match Firefox, the row indices work like this:
    //   Rows from the first <thead> are numbered before all <tbody> rows.
    //   Rows from the first <tfoot> are numbered after all <tbody> rows.
    //   Rows from other <thead> and <tfoot> elements don't get row indices at all.

    int rIndex = 0;

    if (HTMLTableSectionElement* head = static_cast<HTMLTableElement*>(table)->tHead()) {
        for (Node *row = head->firstChild(); row; row = row->nextSibling()) {
            if (row == this)
                return rIndex;
            ++rIndex;
        }
    }
    
    for (Node *node = table->firstChild(); node; node = node->nextSibling()) {
        if (node->hasTagName(tbodyTag)) {
            HTMLTableSectionElement* section = static_cast<HTMLTableSectionElement*>(node);
            for (Node* row = section->firstChild(); row; row = row->nextSibling()) {
                if (row == this)
                    return rIndex;
                ++rIndex;
            }
        }
    }

    if (HTMLTableSectionElement* foot = static_cast<HTMLTableElement *>(table)->tFoot()) {
        for (Node *row = foot->firstChild(); row; row = row->nextSibling()) {
            if (row == this)
                return rIndex;
            ++rIndex;
        }
    }

    // We get here for rows that are in <thead> or <tfoot> sections other than the main header and footer.
    return -1;
}

int HTMLTableRowElement::sectionRowIndex() const
{
    int rIndex = 0;
    const Node *n = this;
    do {
        n = n->previousSibling();
        if (n && n->hasTagName(trTag))
            rIndex++;
    }
    while (n);

    return rIndex;
}

HTMLElement *HTMLTableRowElement::insertCell( int index, ExceptionCode& ec)
{
    HTMLTableCellElement *c = 0L;
    RefPtr<NodeList> children = childNodes();
    int numCells = children ? children->length() : 0;
    if ( index < -1 || index > numCells )
        ec = INDEX_SIZE_ERR; // per the DOM
    else
    {
        c = new HTMLTableCellElement(tdTag, document());
        if(numCells == index || index == -1)
            appendChild(c, ec);
        else {
            Node *n;
            if(index < 1)
                n = firstChild();
            else
                n = children->item(index);
            insertBefore(c, n, ec);
        }
    }
    return c;
}

void HTMLTableRowElement::deleteCell( int index, ExceptionCode& ec)
{
    RefPtr<NodeList> children = childNodes();
    int numCells = children ? children->length() : 0;
    if (index == -1 )
        index = numCells-1;
    if (index >= 0 && index < numCells) {
        RefPtr<Node> row = children->item(index);
        HTMLElement::removeChild(row.get(), ec);
    } else
        ec = INDEX_SIZE_ERR;
}

RefPtr<HTMLCollection> HTMLTableRowElement::cells()
{
    return RefPtr<HTMLCollection>(new HTMLCollection(this, HTMLCollection::TR_CELLS));
}

void HTMLTableRowElement::setCells(HTMLCollection *, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

String HTMLTableRowElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLTableRowElement::setAlign(const String &value)
{
    setAttribute(alignAttr, value);
}

String HTMLTableRowElement::bgColor() const
{
    return getAttribute(bgcolorAttr);
}

void HTMLTableRowElement::setBgColor(const String &value)
{
    setAttribute(bgcolorAttr, value);
}

String HTMLTableRowElement::ch() const
{
    return getAttribute(charAttr);
}

void HTMLTableRowElement::setCh(const String &value)
{
    setAttribute(charAttr, value);
}

String HTMLTableRowElement::chOff() const
{
    return getAttribute(charoffAttr);
}

void HTMLTableRowElement::setChOff(const String &value)
{
    setAttribute(charoffAttr, value);
}

String HTMLTableRowElement::vAlign() const
{
    return getAttribute(valignAttr);
}

void HTMLTableRowElement::setVAlign(const String &value)
{
    setAttribute(valignAttr, value);
}

// -------------------------------------------------------------------------

HTMLTableCellElement::HTMLTableCellElement(const QualifiedName& tagName, Document *doc)
  : HTMLTablePartElement(tagName, doc)
{
    _col = -1;
    _row = -1;
    cSpan = rSpan = 1;
    rowHeight = 0;
    m_solid = false;
}

HTMLTableCellElement::~HTMLTableCellElement()
{
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

bool HTMLTableCellElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == nowrapAttr) {
        result = eUniversal;
        return false;
    }
    
    if (attrName == widthAttr ||
        attrName == heightAttr) {
        result = eCell; // Because of the quirky behavior of ignoring 0 values, cells are special.
        return false;
    }

    return HTMLTablePartElement::mapToEntry(attrName, result);
}

void HTMLTableCellElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == rowspanAttr) {
        rSpan = !attr->isNull() ? attr->value().toInt() : 1;
        if (rSpan < 1) rSpan = 1;
        if (renderer() && renderer()->isTableCell())
            static_cast<RenderTableCell*>(renderer())->updateFromElement();
    } else if (attr->name() == colspanAttr) {
        cSpan = !attr->isNull() ? attr->value().toInt() : 1;
        if (cSpan < 1) cSpan = 1;
        if (renderer() && renderer()->isTableCell())
            static_cast<RenderTableCell*>(renderer())->updateFromElement();
    } else if (attr->name() == nowrapAttr) {
        if (!attr->isNull())
            addCSSProperty(attr, CSS_PROP_WHITE_SPACE, CSS_VAL__WEBKIT_NOWRAP);
    } else if (attr->name() == widthAttr) {
        if (!attr->value().isEmpty()) {
            int widthInt = attr->value().toInt();
            if (widthInt > 0) // width="0" is ignored for compatibility with WinIE.
                addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
        }
    } else if (attr->name() == heightAttr) {
        if (!attr->value().isEmpty()) {
            int heightInt = attr->value().toInt();
            if (heightInt > 0) // height="0" is ignored for compatibility with WinIE.
                addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
        }
    } else
        HTMLTablePartElement::parseMappedAttribute(attr);
}

// used by table cells to share style decls created by the enclosing table.
CSSMutableStyleDeclaration* HTMLTableCellElement::additionalAttributeStyleDecl()
{
    Node* p = parentNode();
    while (p && !p->hasTagName(tableTag))
        p = p->parentNode();

    if (p) {
        HTMLTableElement* table = static_cast<HTMLTableElement*>(p);
        return table->getSharedCellDecl();
    }

    return 0;
}

bool HTMLTableCellElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == backgroundAttr;
}

String HTMLTableCellElement::abbr() const
{
    return getAttribute(abbrAttr);
}

void HTMLTableCellElement::setAbbr(const String &value)
{
    setAttribute(abbrAttr, value);
}

String HTMLTableCellElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLTableCellElement::setAlign(const String &value)
{
    setAttribute(alignAttr, value);
}

String HTMLTableCellElement::axis() const
{
    return getAttribute(axisAttr);
}

void HTMLTableCellElement::setAxis(const String &value)
{
    setAttribute(axisAttr, value);
}

String HTMLTableCellElement::bgColor() const
{
    return getAttribute(bgcolorAttr);
}

void HTMLTableCellElement::setBgColor(const String &value)
{
    setAttribute(bgcolorAttr, value);
}

String HTMLTableCellElement::ch() const
{
    return getAttribute(charAttr);
}

void HTMLTableCellElement::setCh(const String &value)
{
    setAttribute(charAttr, value);
}

String HTMLTableCellElement::chOff() const
{
    return getAttribute(charoffAttr);
}

void HTMLTableCellElement::setChOff(const String &value)
{
    setAttribute(charoffAttr, value);
}

void HTMLTableCellElement::setColSpan(int n)
{
    setAttribute(colspanAttr, String::number(n));
}

String HTMLTableCellElement::headers() const
{
    return getAttribute(headersAttr);
}

void HTMLTableCellElement::setHeaders(const String &value)
{
    setAttribute(headersAttr, value);
}

String HTMLTableCellElement::height() const
{
    return getAttribute(heightAttr);
}

void HTMLTableCellElement::setHeight(const String &value)
{
    setAttribute(heightAttr, value);
}

bool HTMLTableCellElement::noWrap() const
{
    return !getAttribute(nowrapAttr).isNull();
}

void HTMLTableCellElement::setNoWrap(bool b)
{
    setAttribute(nowrapAttr, b ? "" : 0);
}

void HTMLTableCellElement::setRowSpan(int n)
{
    setAttribute(rowspanAttr, String::number(n));
}

String HTMLTableCellElement::scope() const
{
    return getAttribute(scopeAttr);
}

void HTMLTableCellElement::setScope(const String &value)
{
    setAttribute(scopeAttr, value);
}

String HTMLTableCellElement::vAlign() const
{
    return getAttribute(valignAttr);
}

void HTMLTableCellElement::setVAlign(const String &value)
{
    setAttribute(valignAttr, value);
}

String HTMLTableCellElement::width() const
{
    return getAttribute(widthAttr);
}

void HTMLTableCellElement::setWidth(const String &value)
{
    setAttribute(widthAttr, value);
}

// -------------------------------------------------------------------------

HTMLTableColElement::HTMLTableColElement(const QualifiedName& tagName, Document *doc)
    : HTMLTablePartElement(tagName, doc)
{
    _span = (tagName.matches(colgroupTag) ? 0 : 1);
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
        if (!attr->value().isEmpty())
            addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    } else
        HTMLTablePartElement::parseMappedAttribute(attr);
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

// -------------------------------------------------------------------------

bool HTMLTableCaptionElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == alignAttr) {
        result = eCaption;
        return false;
    }

    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLTableCaptionElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == alignAttr) {
        if (!attr->value().isEmpty())
            addCSSProperty(attr, CSS_PROP_CAPTION_SIDE, attr->value());
    } else
        HTMLElement::parseMappedAttribute(attr);
}

String HTMLTableCaptionElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLTableCaptionElement::setAlign(const String &value)
{
    setAttribute(alignAttr, value);
}

}
