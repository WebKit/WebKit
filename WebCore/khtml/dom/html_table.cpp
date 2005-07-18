/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
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
 *
 */
// --------------------------------------------------------------------------

#include "dom/html_misc.h"
#include "dom/html_table.h"
#include "dom/dom_exception.h"

#include "html/html_miscimpl.h"
#include "html/html_tableimpl.h"

using namespace DOM;

HTMLTableCaptionElement::HTMLTableCaptionElement() : HTMLElement()
{
}

HTMLTableCaptionElement::HTMLTableCaptionElement(const HTMLTableCaptionElement &other) : HTMLElement(other)
{
}

HTMLTableCaptionElement::HTMLTableCaptionElement(HTMLTableCaptionElementImpl *impl) : HTMLElement(impl)
{
}

HTMLTableCaptionElement &HTMLTableCaptionElement::operator = (const Node &other)
{
    assignOther( other, HTMLNames::caption() );
    return *this;
}

HTMLTableCaptionElement &HTMLTableCaptionElement::operator = (const HTMLTableCaptionElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLTableCaptionElement::~HTMLTableCaptionElement()
{
}

DOMString HTMLTableCaptionElement::align() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_ALIGN);
}

void HTMLTableCaptionElement::setAlign( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_ALIGN, value);
}

// --------------------------------------------------------------------------

HTMLTableCellElement::HTMLTableCellElement() : HTMLElement()
{
}

HTMLTableCellElement::HTMLTableCellElement(const HTMLTableCellElement &other) : HTMLElement(other)
{
}

HTMLTableCellElement::HTMLTableCellElement(HTMLTableCellElementImpl *impl) : HTMLElement(impl)
{
}

HTMLTableCellElement &HTMLTableCellElement::operator = (const Node &other)
{
    if (!other.handle()->hasTagName(HTMLNames::td()) ||
        !other.handle()->hasTagName(HTMLNames::th())) {
	if ( impl ) impl->deref();
	impl = 0;
    } else {
    Node::operator = (other);
    }
    return *this;
}

HTMLTableCellElement &HTMLTableCellElement::operator = (const HTMLTableCellElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLTableCellElement::~HTMLTableCellElement()
{
}

long HTMLTableCellElement::cellIndex() const
{
    if(!impl) return 0;
    return ((HTMLTableCellElementImpl *)impl)->cellIndex();
}

void HTMLTableCellElement::setCellIndex( long /*_cellIndex*/ )
{
    throw DOMException(DOMException::NO_MODIFICATION_ALLOWED_ERR);
}

DOMString HTMLTableCellElement::abbr() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_ABBR);
}

void HTMLTableCellElement::setAbbr( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_ABBR, value);
}

DOMString HTMLTableCellElement::align() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_ALIGN);
}

void HTMLTableCellElement::setAlign( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_ALIGN, value);
}

DOMString HTMLTableCellElement::axis() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_AXIS);
}

void HTMLTableCellElement::setAxis( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_AXIS, value);
}

DOMString HTMLTableCellElement::bgColor() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_BGCOLOR);
}

void HTMLTableCellElement::setBgColor( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_BGCOLOR, value);
}

DOMString HTMLTableCellElement::ch() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_CHAR);
}

void HTMLTableCellElement::setCh( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_CHAR, value);
}

DOMString HTMLTableCellElement::chOff() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_CHAROFF);
}

void HTMLTableCellElement::setChOff( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_CHAROFF, value);
}

long HTMLTableCellElement::colSpan() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute(ATTR_COLSPAN).toInt();
}

void HTMLTableCellElement::setColSpan( long _colSpan )
{
    if(impl) {
	DOMString value(QString::number(_colSpan));
        ((ElementImpl *)impl)->setAttribute(ATTR_COLSPAN,value);
    }
}

DOMString HTMLTableCellElement::headers() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_HEADERS);
}

void HTMLTableCellElement::setHeaders( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_HEADERS, value);
}

DOMString HTMLTableCellElement::height() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_HEIGHT);
}

void HTMLTableCellElement::setHeight( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_HEIGHT, value);
}

bool HTMLTableCellElement::noWrap() const
{
    if(!impl) return false;
    return !((ElementImpl *)impl)->getAttribute(ATTR_NOWRAP).isNull();
}

void HTMLTableCellElement::setNoWrap( bool _noWrap )
{
    if(impl)
	((ElementImpl *)impl)->setAttribute(ATTR_NOWRAP, _noWrap ? "" : 0);
}

long HTMLTableCellElement::rowSpan() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute(ATTR_ROWSPAN).toInt();
}

void HTMLTableCellElement::setRowSpan( long _rowSpan )
{
    if(impl) {
	DOMString value(QString::number(_rowSpan));
        ((ElementImpl *)impl)->setAttribute(ATTR_ROWSPAN,value);
    }
}

DOMString HTMLTableCellElement::scope() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_SCOPE);
}

void HTMLTableCellElement::setScope( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_SCOPE, value);
}

DOMString HTMLTableCellElement::vAlign() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_VALIGN);
}

void HTMLTableCellElement::setVAlign( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_VALIGN, value);
}

DOMString HTMLTableCellElement::width() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_WIDTH);
}

void HTMLTableCellElement::setWidth( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_WIDTH, value);
}

// --------------------------------------------------------------------------

HTMLTableColElement::HTMLTableColElement() : HTMLElement()
{
}

HTMLTableColElement::HTMLTableColElement(const HTMLTableColElement &other) : HTMLElement(other)
{
}

HTMLTableColElement::HTMLTableColElement(HTMLTableColElementImpl *impl) : HTMLElement(impl)
{
}

HTMLTableColElement &HTMLTableColElement::operator = (const Node &other)
{
    if (!other.handle()->hasTagName(HTMLNames::col()) &&
        !other.handle()->hasTagName(HTMLNames::colgroup())) {
	if ( impl ) impl->deref();
	impl = 0;
    } else {
    Node::operator = (other);
    }
    return *this;
}

HTMLTableColElement &HTMLTableColElement::operator = (const HTMLTableColElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLTableColElement::~HTMLTableColElement()
{
}

DOMString HTMLTableColElement::align() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_ALIGN);
}

void HTMLTableColElement::setAlign( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_ALIGN, value);
}

DOMString HTMLTableColElement::ch() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_CHAR);
}

void HTMLTableColElement::setCh( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_CHAR, value);
}

DOMString HTMLTableColElement::chOff() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_CHAROFF);
}

void HTMLTableColElement::setChOff( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_CHAROFF, value);
}

long HTMLTableColElement::span() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute(ATTR_SPAN).toInt();
}

void HTMLTableColElement::setSpan( long _span )
{
    if(impl) {
	DOMString value(QString::number(_span));
        ((ElementImpl *)impl)->setAttribute(ATTR_SPAN,value);
    }
}

DOMString HTMLTableColElement::vAlign() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_VALIGN);
}

void HTMLTableColElement::setVAlign( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_VALIGN, value);
}

DOMString HTMLTableColElement::width() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_WIDTH);
}

void HTMLTableColElement::setWidth( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_WIDTH, value);
}

// --------------------------------------------------------------------------

HTMLTableElement::HTMLTableElement() : HTMLElement()
{
}

HTMLTableElement::HTMLTableElement(const HTMLTableElement &other) : HTMLElement(other)
{
}

HTMLTableElement::HTMLTableElement(HTMLTableElementImpl *impl) : HTMLElement(impl)
{
}

HTMLTableElement &HTMLTableElement::operator = (const Node &other)
{
    assignOther( other, HTMLNames::table() );
    return *this;
}

HTMLTableElement &HTMLTableElement::operator = (const HTMLTableElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLTableElement::~HTMLTableElement()
{
}

HTMLTableCaptionElement HTMLTableElement::caption() const
{
    if(!impl) return 0;
    return ((HTMLTableElementImpl *)impl)->caption();
}

void HTMLTableElement::setCaption( const HTMLTableCaptionElement &_caption )
{
    if(impl)
        ((HTMLTableElementImpl *)impl)
	    ->setCaption( ((HTMLTableCaptionElementImpl *)_caption.impl) );
}

HTMLTableSectionElement HTMLTableElement::tHead() const
{
    if(!impl) return 0;
    return ((HTMLTableElementImpl *)impl)->tHead();
}

void HTMLTableElement::setTHead( const HTMLTableSectionElement &_tHead )
{

    if(impl)
        ((HTMLTableElementImpl *)impl)
	    ->setTHead( ((HTMLTableSectionElementImpl *)_tHead.impl) );
}

HTMLTableSectionElement HTMLTableElement::tFoot() const
{
    if(!impl) return 0;
    return ((HTMLTableElementImpl *)impl)->tFoot();
}

void HTMLTableElement::setTFoot( const HTMLTableSectionElement &_tFoot )
{

    if(impl)
        ((HTMLTableElementImpl *)impl)
	    ->setTFoot( ((HTMLTableSectionElementImpl *)_tFoot.impl) );
}

HTMLCollection HTMLTableElement::rows() const
{
    if(!impl) return HTMLCollection();
    return HTMLCollection(impl, HTMLCollectionImpl::TABLE_ROWS);
}

HTMLCollection HTMLTableElement::tBodies() const
{
    if(!impl) return HTMLCollection();
    return HTMLCollection(impl, HTMLCollectionImpl::TABLE_TBODIES);
}

DOMString HTMLTableElement::align() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_ALIGN);
}

void HTMLTableElement::setAlign( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_ALIGN, value);
}

DOMString HTMLTableElement::bgColor() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_BGCOLOR);
}

void HTMLTableElement::setBgColor( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_BGCOLOR, value);
}

DOMString HTMLTableElement::border() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_BORDER);
}

void HTMLTableElement::setBorder( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_BORDER, value);
}

DOMString HTMLTableElement::cellPadding() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_CELLPADDING);
}

void HTMLTableElement::setCellPadding( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_CELLPADDING, value);
}

DOMString HTMLTableElement::cellSpacing() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_CELLSPACING);
}

void HTMLTableElement::setCellSpacing( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_CELLSPACING, value);
}

DOMString HTMLTableElement::frame() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_FRAME);
}

void HTMLTableElement::setFrame( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_FRAME, value);
}

DOMString HTMLTableElement::rules() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_RULES);
}

void HTMLTableElement::setRules( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_RULES, value);
}

DOMString HTMLTableElement::summary() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_SUMMARY);
}

void HTMLTableElement::setSummary( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_SUMMARY, value);
}

DOMString HTMLTableElement::width() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_WIDTH);
}

void HTMLTableElement::setWidth( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_WIDTH, value);
}

HTMLElement HTMLTableElement::createTHead(  )
{
    if(!impl) return 0;
    return ((HTMLTableElementImpl *)impl)->createTHead(  );
}

void HTMLTableElement::deleteTHead(  )
{
    if(impl)
        ((HTMLTableElementImpl *)impl)->deleteTHead(  );
}

HTMLElement HTMLTableElement::createTFoot(  )
{
    if(!impl) return 0;
    return ((HTMLTableElementImpl *)impl)->createTFoot(  );
}

void HTMLTableElement::deleteTFoot(  )
{
    if(impl)
        ((HTMLTableElementImpl *)impl)->deleteTFoot(  );
}

HTMLElement HTMLTableElement::createCaption(  )
{
    if(!impl) return 0;
    return ((HTMLTableElementImpl *)impl)->createCaption(  );
}

void HTMLTableElement::deleteCaption(  )
{
    if(impl)
        ((HTMLTableElementImpl *)impl)->deleteCaption(  );
}

HTMLElement HTMLTableElement::insertRow( long index )
{
    if(!impl) return 0;
    int exceptioncode = 0;
    HTMLElementImpl* ret = ((HTMLTableElementImpl *)impl)->insertRow( index, exceptioncode );
    if (exceptioncode)
        throw DOMException(exceptioncode);
    return ret;
}

void HTMLTableElement::deleteRow( long index )
{
    int exceptioncode = 0;
    if(impl)
        ((HTMLTableElementImpl *)impl)->deleteRow( index, exceptioncode );
    if (exceptioncode)
        throw DOMException(exceptioncode);
}

// --------------------------------------------------------------------------

HTMLTableRowElement::HTMLTableRowElement() : HTMLElement()
{
}

HTMLTableRowElement::HTMLTableRowElement(const HTMLTableRowElement &other) : HTMLElement(other)
{
}

HTMLTableRowElement::HTMLTableRowElement(HTMLTableRowElementImpl *impl) : HTMLElement(impl)
{
}

HTMLTableRowElement &HTMLTableRowElement::operator = (const Node &other)
{
    assignOther( other, HTMLNames::tr() );
    return *this;
}

HTMLTableRowElement &HTMLTableRowElement::operator = (const HTMLTableRowElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLTableRowElement::~HTMLTableRowElement()
{
}

long HTMLTableRowElement::rowIndex() const
{
    if(!impl) return 0;
    return ((HTMLTableRowElementImpl *)impl)->rowIndex();
}

void HTMLTableRowElement::setRowIndex( long /*_rowIndex*/ )
{
    throw DOMException(DOMException::NO_MODIFICATION_ALLOWED_ERR);
}

long HTMLTableRowElement::sectionRowIndex() const
{
    if(!impl) return 0;
    return ((HTMLTableRowElementImpl *)impl)->sectionRowIndex();
}

void HTMLTableRowElement::setSectionRowIndex( long /*_sectionRowIndex*/ )
{
    throw DOMException(DOMException::NO_MODIFICATION_ALLOWED_ERR);
}

HTMLCollection HTMLTableRowElement::cells() const
{
    if(!impl) return HTMLCollection();
    return HTMLCollection(impl, HTMLCollectionImpl::TR_CELLS);
}

void HTMLTableRowElement::setCells( const HTMLCollection & /*_cells*/ )
{
    throw DOMException(DOMException::NO_MODIFICATION_ALLOWED_ERR);
}

DOMString HTMLTableRowElement::align() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_ALIGN);
}

void HTMLTableRowElement::setAlign( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_ALIGN, value);
}

DOMString HTMLTableRowElement::bgColor() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_BGCOLOR);
}

void HTMLTableRowElement::setBgColor( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_BGCOLOR, value);
}

DOMString HTMLTableRowElement::ch() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_CHAR);
}

void HTMLTableRowElement::setCh( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_CHAR, value);
}

DOMString HTMLTableRowElement::chOff() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_CHAROFF);
}

void HTMLTableRowElement::setChOff( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_CHAROFF, value);
}

DOMString HTMLTableRowElement::vAlign() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_VALIGN);
}

void HTMLTableRowElement::setVAlign( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_VALIGN, value);
}

HTMLElement HTMLTableRowElement::insertCell( long index )
{
    if(!impl) return 0;
    int exceptioncode = 0;
    HTMLElementImpl* ret = ((HTMLTableRowElementImpl *)impl)->insertCell( index, exceptioncode );
    if (exceptioncode)
        throw DOMException(exceptioncode);
    return ret;
}

void HTMLTableRowElement::deleteCell( long index )
{
    int exceptioncode = 0;
    if(impl)
        ((HTMLTableRowElementImpl *)impl)->deleteCell( index, exceptioncode );
    if (exceptioncode)
        throw DOMException(exceptioncode);
}

// --------------------------------------------------------------------------

HTMLTableSectionElement::HTMLTableSectionElement() : HTMLElement()
{
}

HTMLTableSectionElement::HTMLTableSectionElement(const HTMLTableSectionElement &other) : HTMLElement(other)
{
}

HTMLTableSectionElement::HTMLTableSectionElement(HTMLTableSectionElementImpl *impl) : HTMLElement(impl)
{
}

HTMLTableSectionElement &HTMLTableSectionElement::operator = (const Node &other)
{
    if (!other.handle()->hasTagName(HTMLNames::tbody()) &&
        !other.handle()->hasTagName(HTMLNames::thead()) &&
        !other.handle()->hasTagName(HTMLNames::tfoot())) {
	if ( impl ) impl->deref();
	impl = 0;
    } else {
    Node::operator = (other);
    }
    return *this;
}

HTMLTableSectionElement &HTMLTableSectionElement::operator = (const HTMLTableSectionElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLTableSectionElement::~HTMLTableSectionElement()
{
}

DOMString HTMLTableSectionElement::align() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_ALIGN);
}

void HTMLTableSectionElement::setAlign( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_ALIGN, value);
}

DOMString HTMLTableSectionElement::ch() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_CHAR);
}

void HTMLTableSectionElement::setCh( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_CHAR, value);
}

DOMString HTMLTableSectionElement::chOff() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_CHAROFF);
}

void HTMLTableSectionElement::setChOff( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_CHAROFF, value);
}

DOMString HTMLTableSectionElement::vAlign() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_VALIGN);
}

void HTMLTableSectionElement::setVAlign( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_VALIGN, value);
}

HTMLCollection HTMLTableSectionElement::rows() const
{
    if(!impl) return HTMLCollection();
    return HTMLCollection(impl, HTMLCollectionImpl::TABLE_ROWS);
}

HTMLElement HTMLTableSectionElement::insertRow( long index )
{
    if(!impl) return 0;
    int exceptioncode = 0;
    HTMLElementImpl* ret = ((HTMLTableSectionElementImpl *)impl)->insertRow( index, exceptioncode );
    if (exceptioncode)
        throw DOMException(exceptioncode);
    return ret;
}

void HTMLTableSectionElement::deleteRow( long index )
{
    int exceptioncode = 0;
    if(impl)
        ((HTMLTableSectionElementImpl *)impl)->deleteRow( index, exceptioncode );
    if (exceptioncode)
        throw DOMException(exceptioncode);
}

