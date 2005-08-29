/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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

#include "html/html_documentimpl.h"
#include "html/html_tableimpl.h"

#include "dom/dom_exception.h"
#include "dom/dom_node.h"

#include "khtmlview.h"
#include "khtml_part.h"

#include "css/cssstyleselector.h"
#include "css/cssproperties.h"
#include "css/cssvalues.h"
#include "css/csshelper.h"
#include "css_valueimpl.h"
#include "css/css_stylesheetimpl.h"

#include "rendering/render_table.h"

#include <kdebug.h>
#include <kglobal.h>

using namespace khtml;

namespace DOM {

using namespace HTMLNames;

HTMLTableElementImpl::HTMLTableElementImpl(DocumentPtr *doc)
  : HTMLElementImpl(tableTag, doc)
{
    tCaption = 0;
    head = 0;
    foot = 0;
    firstBody = 0;

#if 0
    rules = None;
    frame = Void;
#endif
 
    padding = 1;
    
    m_noBorder = true;
    m_solid = false;
}

HTMLTableElementImpl::~HTMLTableElementImpl()
{
    if (firstBody)
        firstBody->deref();
}

bool HTMLTableElementImpl::checkDTD(const NodeImpl* newChild)
{
    return newChild->isTextNode() || newChild->hasTagName(captionTag) ||
           newChild->hasTagName(colTag) || newChild->hasTagName(colgroupTag) ||
           newChild->hasTagName(theadTag) || newChild->hasTagName(tfootTag) ||
           newChild->hasTagName(tbodyTag) || newChild->hasTagName(formTag) ||
           newChild->hasTagName(scriptTag);
}

NodeImpl* HTMLTableElementImpl::setCaption( HTMLTableCaptionElementImpl *c )
{
    int exceptioncode = 0;
    NodeImpl* r;
    if(tCaption) {
        replaceChild ( c, tCaption, exceptioncode );
        r = c;
    }
    else
        r = insertBefore( c, firstChild(), exceptioncode );
    tCaption = c;
    return r;
}

NodeImpl* HTMLTableElementImpl::setTHead( HTMLTableSectionElementImpl *s )
{
    int exceptioncode = 0;
    NodeImpl* r;
    if(head) {
        replaceChild( s, head, exceptioncode );
        r = s;
    }
    else if( foot )
        r = insertBefore( s, foot, exceptioncode );
    else if( firstBody )
        r = insertBefore( s, firstBody, exceptioncode );
    else
        r = appendChild( s, exceptioncode );

    head = s;
    return r;
}

NodeImpl* HTMLTableElementImpl::setTFoot( HTMLTableSectionElementImpl *s )
{
    int exceptioncode = 0;
    NodeImpl* r;
    if(foot) {
        replaceChild ( s, foot, exceptioncode );
        r = s;
    } else if( firstBody )
        r = insertBefore( s, firstBody, exceptioncode );
    else
        r = appendChild( s, exceptioncode );
    foot = s;
    return r;
}

NodeImpl* HTMLTableElementImpl::setTBody( HTMLTableSectionElementImpl *s )
{
    int exceptioncode = 0;
    NodeImpl* r;

    s->ref();
    if(firstBody) {
        replaceChild ( s, firstBody, exceptioncode );
        firstBody->deref();
        r = s;
    } else
        r = appendChild( s, exceptioncode );
    firstBody = s;

    return r;
}

HTMLElementImpl *HTMLTableElementImpl::createTHead(  )
{
    if(!head)
    {
        int exceptioncode = 0;
        head = new HTMLTableSectionElementImpl(theadTag, docPtr(), true /* implicit */);
        if(foot)
            insertBefore( head, foot, exceptioncode );
        else if(firstBody)
            insertBefore( head, firstBody, exceptioncode);
        else
            appendChild(head, exceptioncode);
    }
    return head;
}

void HTMLTableElementImpl::deleteTHead(  )
{
    if(head) {
        int exceptioncode = 0;
        HTMLElementImpl::removeChild(head, exceptioncode);
    }
    head = 0;
}

HTMLElementImpl *HTMLTableElementImpl::createTFoot(  )
{
    if(!foot)
    {
        int exceptioncode = 0;
        foot = new HTMLTableSectionElementImpl(tfootTag, docPtr(), true /*implicit */);
        if(firstBody)
            insertBefore( foot, firstBody, exceptioncode );
        else
            appendChild(foot, exceptioncode);
    }
    return foot;
}

void HTMLTableElementImpl::deleteTFoot(  )
{
    if(foot) {
        int exceptioncode = 0;
        HTMLElementImpl::removeChild(foot, exceptioncode);
    }
    foot = 0;
}

HTMLElementImpl *HTMLTableElementImpl::createCaption(  )
{
    if(!tCaption)
    {
        int exceptioncode = 0;
        tCaption = new HTMLTableCaptionElementImpl(docPtr());
        insertBefore( tCaption, firstChild(), exceptioncode );
    }
    return tCaption;
}

void HTMLTableElementImpl::deleteCaption(  )
{
    if(tCaption) {
        int exceptioncode = 0;
        HTMLElementImpl::removeChild(tCaption, exceptioncode);
    }
    tCaption = 0;
}

HTMLElementImpl *HTMLTableElementImpl::insertRow( long index, int &exceptioncode )
{
    // The DOM requires that we create a tbody if the table is empty
    // (cf DOM2TS HTMLTableElement31 test)
    // (note: this is different from "if the table has no sections", since we can have
    // <TABLE><TR>)
    if(!firstBody && !head && !foot)
        setTBody( new HTMLTableSectionElementImpl(tbodyTag, docPtr(), true /* implicit */) );

    //kdDebug(6030) << k_funcinfo << index << endl;
    // IE treats index=-1 as default value meaning 'append after last'
    // This isn't in the DOM. So, not implemented yet.
    HTMLTableSectionElementImpl* section = 0L;
    HTMLTableSectionElementImpl* lastSection = 0L;
    NodeImpl *node = firstChild();
    bool append = (index == -1);
    bool found = false;
    for ( ; node && (index>=0 || append) ; node = node->nextSibling() )
    {
	// there could be 2 tfoot elements in the table. Only the first one is the "foot", that's why we have the more
	// complicated if statement below.
        if (node != foot && (node->hasTagName(theadTag) || node->hasTagName(tfootTag) || 
            node->hasTagName(tbodyTag)))
        {
            section = static_cast<HTMLTableSectionElementImpl *>(node);
            lastSection = section;
            //kdDebug(6030) << k_funcinfo << "section id=" << node->id() << " rows:" << section->numRows() << endl;
            if ( !append )
            {
                int rows = section->numRows();
                if ( rows > index ) {
		    found = true;
                    break;
                } else
                    index -= rows;
                //kdDebug(6030) << "       index is now " << index << endl;
            }
        }
    }
    if ( !found && foot )
        section = static_cast<HTMLTableSectionElementImpl *>(foot);

    // Index == 0 means "insert before first row in current section"
    // or "append after last row" (if there's no current section anymore)
    if ( !section && ( index == 0 || append ) )
    {
        section = lastSection;
        index = section ? section->numRows() : 0;
    }
    if ( section && (index >= 0 || append) ) {
        //kdDebug(6030) << "Inserting row into section " << section << " at index " << index << endl;
        return section->insertRow( index, exceptioncode );
    } else {
        // No more sections => index is too big
        exceptioncode = DOMException::INDEX_SIZE_ERR;
        return 0L;
    }
}

void HTMLTableElementImpl::deleteRow( long index, int &exceptioncode )
{
    HTMLTableSectionElementImpl* section = 0L;
    NodeImpl *node = firstChild();
    bool lastRow = index == -1;
    HTMLTableSectionElementImpl* lastSection = 0L;
    bool found = false;
    for ( ; node ; node = node->nextSibling() )
    {
        if (node != foot && (node->hasTagName(theadTag) || node->hasTagName(tfootTag) || 
            node->hasTagName(tbodyTag))) {
            section = static_cast<HTMLTableSectionElementImpl *>(node);
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
        section = static_cast<HTMLTableSectionElementImpl *>(foot);

    if ( lastRow )
        lastSection->deleteRow( -1, exceptioncode );
    else if ( section && index >= 0 && index < section->numRows() )
        section->deleteRow( index, exceptioncode );
    else
        exceptioncode = DOMException::INDEX_SIZE_ERR;
}

NodeImpl *HTMLTableElementImpl::addChild(NodeImpl *child)
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6030 ) << nodeName().qstring() << "(Table)::addChild( " << child->nodeName().qstring() << " )" << endl;
#endif

    if (child->hasTagName(formTag)) {
        // First add the child.
        HTMLElementImpl::addChild(child);
        // Now simply return ourselves as the newnode.  This has the effect of
        // demoting the form to a leaf and moving it safely out of the way.
        return this;
    }
    
    // We do not want this check-node-allowed test here, however the code to
    // build up tables relies on childAllowed failing to make sure that the
    // table is well-formed, the primary work being to add a tbody element.
    // As 99.9999% of tables on the weeb do not have tbody elements, it seems
    // odd to traverse an "error" case for the most common table markup.
    // See <rdar://problem/3719373> Table parsing and construction relies on 
    // childAllowed failures to build up proper DOM
    if (child->nodeType() == Node::DOCUMENT_FRAGMENT_NODE) {
        // child is a DocumentFragment... check all its children instead of child itself
        for (NodeImpl *c = child->firstChild(); c; c = c->nextSibling())
            if (!childAllowed(c))
                return 0;
    }
    else if (!childAllowed(child)) {
        // child is not a DocumentFragment... check if it's allowed directly
        return 0;
    }

    int exceptioncode = 0;
    NodeImpl *retval = appendChild( child, exceptioncode );
    if (retval) {
        if (!tCaption && child->hasTagName(captionTag))
            tCaption = static_cast<HTMLTableCaptionElementImpl *>(child);
        else if (!head && child->hasTagName(theadTag))
            head = static_cast<HTMLTableSectionElementImpl *>(child);
        else if (!foot && child->hasTagName(tfootTag))
            foot = static_cast<HTMLTableSectionElementImpl *>(child);
	else if (!firstBody && child->hasTagName(tbodyTag)) {
	    firstBody = static_cast<HTMLTableSectionElementImpl *>(child);
            firstBody->ref();
        }
    }
    return retval;
}

void HTMLTableElementImpl::childrenChanged()
{
    HTMLElementImpl::childrenChanged();
    
    if (firstBody && firstBody->parentNode() != this) {
        firstBody->deref();
        firstBody = 0;
    }
} 

bool HTMLTableElementImpl::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == backgroundAttr) {
        result = (MappedAttributeEntry)(eLastEntry + getDocument()->docID());
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
    
    return HTMLElementImpl::mapToEntry(attrName, result);
}

void HTMLTableElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == widthAttr) {
        addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    } else if (attr->name() == heightAttr) {
        addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    } else if (attr->name() == borderAttr)  {
        m_noBorder = true;
        if (attr->decl()) {
            CSSValueImpl* val = attr->decl()->getPropertyCSSValue(CSS_PROP_BORDER_LEFT_WIDTH);
            if (val) {
                val->ref();
                if (val->isPrimitiveValue()) {
                    CSSPrimitiveValueImpl* primVal = static_cast<CSSPrimitiveValueImpl*>(val);
                    m_noBorder = !primVal->getFloatValue(CSSPrimitiveValue::CSS_NUMBER);
                }
                val->deref();
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
            DOMString v = QString::number( border );
            addCSSLength(attr, CSS_PROP_BORDER_WIDTH, v );
        }
#if 0
        // wanted by HTML4 specs
        if (m_noBorder)
            frame = Void, rules = None;
        else
            frame = Box, rules = All;
#endif
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
        QString url = khtml::parseURL( attr->value() ).qstring();
        if (!url.isEmpty())
            addCSSImageProperty(attr, CSS_PROP_BACKGROUND_IMAGE, getDocument()->completeURL(url));
    } else if (attr->name() == frameAttr) {
#if 0
        if ( strcasecmp( attr->value(), "void" ) == 0 )
            frame = Void;
        else if ( strcasecmp( attr->value(), "border" ) == 0 )
            frame = Box;
        else if ( strcasecmp( attr->value(), "box" ) == 0 )
            frame = Box;
        else if ( strcasecmp( attr->value(), "hsides" ) == 0 )
            frame = Hsides;
        else if ( strcasecmp( attr->value(), "vsides" ) == 0 )
            frame = Vsides;
        else if ( strcasecmp( attr->value(), "above" ) == 0 )
            frame = Above;
        else if ( strcasecmp( attr->value(), "below" ) == 0 )
            frame = Below;
        else if ( strcasecmp( attr->value(), "lhs" ) == 0 )
            frame = Lhs;
        else if ( strcasecmp( attr->value(), "rhs" ) == 0 )
            frame = Rhs;
#endif
    } else if (attr->name() == rulesAttr) {
#if 0
        if ( strcasecmp( attr->value(), "none" ) == 0 )
            rules = None;
        else if ( strcasecmp( attr->value(), "groups" ) == 0 )
            rules = Groups;
        else if ( strcasecmp( attr->value(), "rows" ) == 0 )
            rules = Rows;
        else if ( strcasecmp( attr->value(), "cols" ) == 0 )
            rules = Cols;
        else if ( strcasecmp( attr->value(), "all" ) == 0 )
            rules = All;
#endif
    } else if (attr->name() == cellspacingAttr) {
        if (!attr->value().isEmpty())
            addCSSLength(attr, CSS_PROP_BORDER_SPACING, attr->value());
    } else if (attr->name() == cellpaddingAttr) {
        if (!attr->value().isEmpty())
            padding = kMax( 0, attr->value().toInt() );
        else
            padding = 1;
        if (m_render && m_render->isTable()) {
            static_cast<RenderTable *>(m_render)->setCellPadding(padding);
	    if (!m_render->needsLayout())
	        m_render->setNeedsLayout(true);
        }
    } else if (attr->name() == colsAttr) {
        // ###
#if 0
        int c;
        c = attr->val()->toInt();
        addColumns(c-totalCols);
#endif
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
        HTMLElementImpl::parseMappedAttribute(attr);
}

CSSMutableStyleDeclarationImpl* HTMLTableElementImpl::additionalAttributeStyleDecl()
{
    if (m_noBorder)
        return 0;
    MappedAttributeImpl attr(tableborderAttr, m_solid ? "solid" : "outset");
    CSSMappedAttributeDeclarationImpl* decl = getMappedAttributeDecl(ePersistent, &attr);
    if (!decl) {
        decl = new CSSMappedAttributeDeclarationImpl(0);
        decl->setParent(getDocument()->elementSheet());
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

CSSMutableStyleDeclarationImpl* HTMLTableElementImpl::getSharedCellDecl()
{
    MappedAttributeImpl attr(cellborderAttr, m_noBorder ? "none" : (m_solid ? "solid" : "inset"));
    CSSMappedAttributeDeclarationImpl* decl = getMappedAttributeDecl(ePersistent, &attr);
    if (!decl) {
        decl = new CSSMappedAttributeDeclarationImpl(0);
        decl->setParent(getDocument()->elementSheet());
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

void HTMLTableElementImpl::attach()
{
    assert(!m_attached);
    HTMLElementImpl::attach();
    if ( m_render && m_render->isTable() )
	static_cast<RenderTable *>(m_render)->setCellPadding( padding );
}

bool HTMLTableElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return attr->name() == backgroundAttr;
}

SharedPtr<HTMLCollectionImpl> HTMLTableElementImpl::rows()
{
    return SharedPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::TABLE_ROWS));
}

SharedPtr<HTMLCollectionImpl> HTMLTableElementImpl::tBodies()
{
    return SharedPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::TABLE_TBODIES));
}

DOMString HTMLTableElementImpl::align() const
{
    return getAttribute(alignAttr);
}

void HTMLTableElementImpl::setAlign(const DOMString &value)
{
    setAttribute(alignAttr, value);
}

DOMString HTMLTableElementImpl::bgColor() const
{
    return getAttribute(bgcolorAttr);
}

void HTMLTableElementImpl::setBgColor(const DOMString &value)
{
    setAttribute(bgcolorAttr, value);
}

DOMString HTMLTableElementImpl::border() const
{
    return getAttribute(borderAttr);
}

void HTMLTableElementImpl::setBorder(const DOMString &value)
{
    setAttribute(borderAttr, value);
}

DOMString HTMLTableElementImpl::cellPadding() const
{
    return getAttribute(cellpaddingAttr);
}

void HTMLTableElementImpl::setCellPadding(const DOMString &value)
{
    setAttribute(cellpaddingAttr, value);
}

DOMString HTMLTableElementImpl::cellSpacing() const
{
    return getAttribute(cellspacingAttr);
}

void HTMLTableElementImpl::setCellSpacing(const DOMString &value)
{
    setAttribute(cellspacingAttr, value);
}

DOMString HTMLTableElementImpl::frame() const
{
    return getAttribute(frameAttr);
}

void HTMLTableElementImpl::setFrame(const DOMString &value)
{
    setAttribute(frameAttr, value);
}

DOMString HTMLTableElementImpl::rules() const
{
    return getAttribute(rulesAttr);
}

void HTMLTableElementImpl::setRules(const DOMString &value)
{
    setAttribute(rulesAttr, value);
}

DOMString HTMLTableElementImpl::summary() const
{
    return getAttribute(summaryAttr);
}

void HTMLTableElementImpl::setSummary(const DOMString &value)
{
    setAttribute(summaryAttr, value);
}

DOMString HTMLTableElementImpl::width() const
{
    return getAttribute(widthAttr);
}

void HTMLTableElementImpl::setWidth(const DOMString &value)
{
    setAttribute(widthAttr, value);
}

// --------------------------------------------------------------------------

bool HTMLTablePartElementImpl::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == backgroundAttr) {
        result = (MappedAttributeEntry)(eLastEntry + getDocument()->docID());
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

    return HTMLElementImpl::mapToEntry(attrName, result);
}

void HTMLTablePartElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == bgcolorAttr) {
        addCSSColor(attr, CSS_PROP_BACKGROUND_COLOR, attr->value());
    } else if (attr->name() == backgroundAttr) {
        QString url = khtml::parseURL( attr->value() ).qstring();
        if (!url.isEmpty())
            addCSSImageProperty(attr, CSS_PROP_BACKGROUND_IMAGE, getDocument()->completeURL(url));
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
        DOMString v = attr->value();
        if ( strcasecmp( attr->value(), "middle" ) == 0 || strcasecmp( attr->value(), "center" ) == 0 )
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_CENTER);
        else if (strcasecmp(attr->value(), "absmiddle") == 0)
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL_CENTER);
        else if (strcasecmp(attr->value(), "left") == 0)
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_LEFT);
        else if (strcasecmp(attr->value(), "right") == 0)
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_RIGHT);
        else
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, v);
    } else if (attr->name() == heightAttr) {
        if (!attr->value().isEmpty())
            addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

// -------------------------------------------------------------------------

HTMLTableSectionElementImpl::HTMLTableSectionElementImpl(const QualifiedName& tagName, DocumentPtr *doc, bool implicit)
    : HTMLTablePartElementImpl(tagName, doc)
{
    m_implicit = implicit;
}

bool HTMLTableSectionElementImpl::checkDTD(const NodeImpl* newChild)
{
    return newChild->hasTagName(trTag) || newChild->hasTagName(formTag) ||
           newChild->hasTagName(scriptTag);
}

NodeImpl *HTMLTableSectionElementImpl::addChild(NodeImpl *child)
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6030 ) << nodeName().qstring() << "(Tbody)::addChild( " << child->nodeName().qstring() << " )" << endl;
#endif

    if (child->hasTagName(formTag)) {
        // First add the child.
        HTMLElementImpl::addChild(child);
        // Now simply return ourselves as the newnode.  This has the effect of
        // demoting the form to a leaf and moving it safely out of the way.
        return this;
    }
    return HTMLTablePartElementImpl::addChild(child);
}

// these functions are rather slow, since we need to get the row at
// the index... but they aren't used during usual HTML parsing anyway
HTMLElementImpl *HTMLTableSectionElementImpl::insertRow( long index, int& exceptioncode )
{
    HTMLTableRowElementImpl *r = 0L;
    SharedPtr<NodeListImpl> children = childNodes();
    int numRows = children.notNull() ? (int)children->length() : 0;
    //kdDebug(6030) << k_funcinfo << "index=" << index << " numRows=" << numRows << endl;
    if ( index < -1 || index > numRows ) {
        exceptioncode = DOMException::INDEX_SIZE_ERR; // per the DOM
    }
    else
    {
        r = new HTMLTableRowElementImpl(docPtr());
        if ( numRows == index || index == -1 )
            appendChild(r, exceptioncode);
        else {
            NodeImpl *n;
            if(index < 1)
                n = firstChild();
            else
                n = children->item(index);
            insertBefore(r, n, exceptioncode );
        }
    }
    return r;
}

void HTMLTableSectionElementImpl::deleteRow( long index, int &exceptioncode )
{
    SharedPtr<NodeListImpl> children = childNodes();
    int numRows = children.notNull() ? (int)children->length() : 0;
    if ( index == -1 ) index = numRows - 1;
    if( index >= 0 && index < numRows )
        HTMLElementImpl::removeChild(children->item(index), exceptioncode);
    else
        exceptioncode = DOMException::INDEX_SIZE_ERR;
}

int HTMLTableSectionElementImpl::numRows() const
{
    int rows = 0;
    const NodeImpl *n = firstChild();
    while (n) {
        if (n->hasTagName(trTag))
            rows++;
        n = n->nextSibling();
    }

    return rows;
}

DOMString HTMLTableSectionElementImpl::align() const
{
    return getAttribute(alignAttr);
}

void HTMLTableSectionElementImpl::setAlign(const DOMString &value)
{
    setAttribute(alignAttr, value);
}

DOMString HTMLTableSectionElementImpl::ch() const
{
    return getAttribute(charAttr);
}

void HTMLTableSectionElementImpl::setCh(const DOMString &value)
{
    setAttribute(charAttr, value);
}

DOMString HTMLTableSectionElementImpl::chOff() const
{
    return getAttribute(charoffAttr);
}

void HTMLTableSectionElementImpl::setChOff(const DOMString &value)
{
    setAttribute(charoffAttr, value);
}

DOMString HTMLTableSectionElementImpl::vAlign() const
{
    return getAttribute(valignAttr);
}

void HTMLTableSectionElementImpl::setVAlign(const DOMString &value)
{
    setAttribute(valignAttr, value);
}

SharedPtr<HTMLCollectionImpl> HTMLTableSectionElementImpl::rows()
{
    return SharedPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::TABLE_ROWS));
}

// -------------------------------------------------------------------------

bool HTMLTableRowElementImpl::checkDTD(const NodeImpl* newChild)
{
    return newChild->hasTagName(tdTag) || newChild->hasTagName(thTag) ||
           newChild->hasTagName(formTag) || newChild->hasTagName(scriptTag);
}

NodeImpl *HTMLTableRowElementImpl::addChild(NodeImpl *child)
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6030 ) << nodeName().qstring() << "(Trow)::addChild( " << child->nodeName().qstring() << " )" << endl;
#endif

    if (child->hasTagName(formTag)) {
        // First add the child.
        HTMLElementImpl::addChild(child);
        // Now simply return ourselves as the newnode.  This has the effect of
        // demoting the form to a leaf and moving it safely out of the way.
        return this;
    }
    return HTMLTablePartElementImpl::addChild(child);
}

long HTMLTableRowElementImpl::rowIndex() const
{
    int rIndex = 0;

    NodeImpl *table = parentNode();
    if (!table)
	return -1;
    table = table->parentNode();
    if (!table || !table->hasTagName(tableTag))
	return -1;

    HTMLTableSectionElementImpl *foot = static_cast<HTMLTableElementImpl *>(table)->tFoot();
    NodeImpl *node = table->firstChild();
    while (node) {
        if (node != foot && (node->hasTagName(theadTag) || node->hasTagName(tfootTag) || node->hasTagName(tbodyTag))) {
	    HTMLTableSectionElementImpl* section = static_cast<HTMLTableSectionElementImpl *>(node);
	    const NodeImpl *row = section->firstChild();
	    while ( row ) {
		if ( row == this )
		    return rIndex;
		rIndex++;
		row = row->nextSibling();
	    }
	}
	node = node->nextSibling();
    }
    const NodeImpl *row = foot->firstChild();
    while ( row ) {
	if ( row == this )
	    return rIndex;
	rIndex++;
	row = row->nextSibling();
    }
    // should never happen
    return -1;
}

long HTMLTableRowElementImpl::sectionRowIndex() const
{
    int rIndex = 0;
    const NodeImpl *n = this;
    do {
        n = n->previousSibling();
        if (n && n->hasTagName(trTag))
            rIndex++;
    }
    while (n);

    return rIndex;
}

HTMLElementImpl *HTMLTableRowElementImpl::insertCell( long index, int &exceptioncode )
{
    HTMLTableCellElementImpl *c = 0L;
    SharedPtr<NodeListImpl> children = childNodes();
    int numCells = children.notNull() ? children->length() : 0;
    if ( index < -1 || index > numCells )
        exceptioncode = DOMException::INDEX_SIZE_ERR; // per the DOM
    else
    {
        c = new HTMLTableCellElementImpl(tdTag, docPtr());
        if(numCells == index || index == -1)
            appendChild(c, exceptioncode);
        else {
            NodeImpl *n;
            if(index < 1)
                n = firstChild();
            else
                n = children->item(index);
            insertBefore(c, n, exceptioncode);
        }
    }
    return c;
}

void HTMLTableRowElementImpl::deleteCell( long index, int &exceptioncode )
{
    SharedPtr<NodeListImpl> children = childNodes();
    int numCells = children.notNull() ? children->length() : 0;
    if ( index == -1 ) index = numCells-1;
    if( index >= 0 && index < numCells )
        HTMLElementImpl::removeChild(children->item(index), exceptioncode);
    else
        exceptioncode = DOMException::INDEX_SIZE_ERR;
}

SharedPtr<HTMLCollectionImpl> HTMLTableRowElementImpl::cells()
{
    return SharedPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::TR_CELLS));
}

void HTMLTableRowElementImpl::setCells(HTMLCollectionImpl *, int &exception)
{
    exception = DOMException::NO_MODIFICATION_ALLOWED_ERR;
}

DOMString HTMLTableRowElementImpl::align() const
{
    return getAttribute(alignAttr);
}

void HTMLTableRowElementImpl::setAlign(const DOMString &value)
{
    setAttribute(alignAttr, value);
}

DOMString HTMLTableRowElementImpl::bgColor() const
{
    return getAttribute(bgcolorAttr);
}

void HTMLTableRowElementImpl::setBgColor(const DOMString &value)
{
    setAttribute(bgcolorAttr, value);
}

DOMString HTMLTableRowElementImpl::ch() const
{
    return getAttribute(charAttr);
}

void HTMLTableRowElementImpl::setCh(const DOMString &value)
{
    setAttribute(charAttr, value);
}

DOMString HTMLTableRowElementImpl::chOff() const
{
    return getAttribute(charoffAttr);
}

void HTMLTableRowElementImpl::setChOff(const DOMString &value)
{
    setAttribute(charoffAttr, value);
}

DOMString HTMLTableRowElementImpl::vAlign() const
{
    return getAttribute(valignAttr);
}

void HTMLTableRowElementImpl::setVAlign(const DOMString &value)
{
    setAttribute(valignAttr, value);
}

// -------------------------------------------------------------------------

HTMLTableCellElementImpl::HTMLTableCellElementImpl(const QualifiedName& tagName, DocumentPtr *doc)
  : HTMLTablePartElementImpl(tagName, doc)
{
    _col = -1;
    _row = -1;
    cSpan = rSpan = 1;
    rowHeight = 0;
    m_solid = false;
}

HTMLTableCellElementImpl::~HTMLTableCellElementImpl()
{
}

long HTMLTableCellElementImpl::cellIndex() const
{
    int index = 0;
    for (const NodeImpl * node = previousSibling(); node; node = node->previousSibling()) {
        if (node->hasTagName(tdTag) || node->hasTagName(thTag))
            index++;
    }
    
    return index;
}

bool HTMLTableCellElementImpl::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
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

    return HTMLTablePartElementImpl::mapToEntry(attrName, result);
}

void HTMLTableCellElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
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
            addCSSProperty(attr, CSS_PROP_WHITE_SPACE, CSS_VAL__KHTML_NOWRAP);
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
        HTMLTablePartElementImpl::parseMappedAttribute(attr);
}

// used by table cells to share style decls created by the enclosing table.
CSSMutableStyleDeclarationImpl* HTMLTableCellElementImpl::additionalAttributeStyleDecl()
{
    NodeImpl* p = parentNode();
    while (p && !p->hasTagName(tableTag))
        p = p->parentNode();

    if (p) {
        HTMLTableElementImpl* table = static_cast<HTMLTableElementImpl*>(p);
        return table->getSharedCellDecl();
    }

    return 0;
}

bool HTMLTableCellElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return attr->name() == backgroundAttr;
}

DOMString HTMLTableCellElementImpl::abbr() const
{
    return getAttribute(abbrAttr);
}

void HTMLTableCellElementImpl::setAbbr(const DOMString &value)
{
    setAttribute(abbrAttr, value);
}

DOMString HTMLTableCellElementImpl::align() const
{
    return getAttribute(alignAttr);
}

void HTMLTableCellElementImpl::setAlign(const DOMString &value)
{
    setAttribute(alignAttr, value);
}

DOMString HTMLTableCellElementImpl::axis() const
{
    return getAttribute(axisAttr);
}

void HTMLTableCellElementImpl::setAxis(const DOMString &value)
{
    setAttribute(axisAttr, value);
}

DOMString HTMLTableCellElementImpl::bgColor() const
{
    return getAttribute(bgcolorAttr);
}

void HTMLTableCellElementImpl::setBgColor(const DOMString &value)
{
    setAttribute(bgcolorAttr, value);
}

DOMString HTMLTableCellElementImpl::ch() const
{
    return getAttribute(charAttr);
}

void HTMLTableCellElementImpl::setCh(const DOMString &value)
{
    setAttribute(charAttr, value);
}

DOMString HTMLTableCellElementImpl::chOff() const
{
    return getAttribute(charoffAttr);
}

void HTMLTableCellElementImpl::setChOff(const DOMString &value)
{
    setAttribute(charoffAttr, value);
}

void HTMLTableCellElementImpl::setColSpan(long n)
{
    setAttribute(colspanAttr, QString::number(n));
}

DOMString HTMLTableCellElementImpl::headers() const
{
    return getAttribute(headersAttr);
}

void HTMLTableCellElementImpl::setHeaders(const DOMString &value)
{
    setAttribute(headersAttr, value);
}

DOMString HTMLTableCellElementImpl::height() const
{
    return getAttribute(heightAttr);
}

void HTMLTableCellElementImpl::setHeight(const DOMString &value)
{
    setAttribute(heightAttr, value);
}

bool HTMLTableCellElementImpl::noWrap() const
{
    return !getAttribute(nowrapAttr).isNull();
}

void HTMLTableCellElementImpl::setNoWrap(bool b)
{
    setAttribute(nowrapAttr, b ? "" : 0);
}

void HTMLTableCellElementImpl::setRowSpan(long n)
{
    setAttribute(rowspanAttr, QString::number(n));
}

DOMString HTMLTableCellElementImpl::scope() const
{
    return getAttribute(scopeAttr);
}

void HTMLTableCellElementImpl::setScope(const DOMString &value)
{
    setAttribute(scopeAttr, value);
}

DOMString HTMLTableCellElementImpl::vAlign() const
{
    return getAttribute(valignAttr);
}

void HTMLTableCellElementImpl::setVAlign(const DOMString &value)
{
    setAttribute(valignAttr, value);
}

DOMString HTMLTableCellElementImpl::width() const
{
    return getAttribute(widthAttr);
}

void HTMLTableCellElementImpl::setWidth(const DOMString &value)
{
    setAttribute(widthAttr, value);
}

// -------------------------------------------------------------------------

HTMLTableColElementImpl::HTMLTableColElementImpl(const QualifiedName& tagName, DocumentPtr *doc)
    : HTMLTablePartElementImpl(tagName, doc)
{
    _span = (tagName.matches(colgroupTag) ? 0 : 1);
}

bool HTMLTableColElementImpl::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == widthAttr) {
        result = eUniversal;
        return false;
    }

    return HTMLTablePartElementImpl::mapToEntry(attrName, result);
}

void HTMLTableColElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == spanAttr) {
        _span = !attr->isNull() ? attr->value().toInt() : 1;
        if (renderer() && renderer()->isTableCol())
            static_cast<RenderTableCol*>(renderer())->updateFromElement();
    } else if (attr->name() == widthAttr) {
        if (!attr->value().isEmpty())
            addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    } else
        HTMLTablePartElementImpl::parseMappedAttribute(attr);
}

DOMString HTMLTableColElementImpl::align() const
{
    return getAttribute(alignAttr);
}

void HTMLTableColElementImpl::setAlign(const DOMString &value)
{
    setAttribute(alignAttr, value);
}

DOMString HTMLTableColElementImpl::ch() const
{
    return getAttribute(charAttr);
}

void HTMLTableColElementImpl::setCh(const DOMString &value)
{
    setAttribute(charAttr, value);
}

DOMString HTMLTableColElementImpl::chOff() const
{
    return getAttribute(charoffAttr);
}

void HTMLTableColElementImpl::setChOff(const DOMString &value)
{
    setAttribute(charoffAttr, value);
}

void HTMLTableColElementImpl::setSpan(long n)
{
    setAttribute(spanAttr, QString::number(n));
}

DOMString HTMLTableColElementImpl::vAlign() const
{
    return getAttribute(valignAttr);
}

void HTMLTableColElementImpl::setVAlign(const DOMString &value)
{
    setAttribute(valignAttr, value);
}

DOMString HTMLTableColElementImpl::width() const
{
    return getAttribute(widthAttr);
}

void HTMLTableColElementImpl::setWidth(const DOMString &value)
{
    setAttribute(widthAttr, value);
}

// -------------------------------------------------------------------------

bool HTMLTableCaptionElementImpl::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == alignAttr) {
        result = eCaption;
        return false;
    }

    return HTMLElementImpl::mapToEntry(attrName, result);
}

void HTMLTableCaptionElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == alignAttr) {
        if (!attr->value().isEmpty())
            addCSSProperty(attr, CSS_PROP_CAPTION_SIDE, attr->value());
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

DOMString HTMLTableCaptionElementImpl::align() const
{
    return getAttribute(alignAttr);
}

void HTMLTableCaptionElementImpl::setAlign(const DOMString &value)
{
    setAttribute(alignAttr, value);
}

}
