/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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
#include "dom/dom_exception.h"
#include "dom/css_rule.h"
#include "dom/dom_doc.h"

#include "xml/dom_docimpl.h"

#include "html/html_headimpl.h"

#include "css/css_stylesheetimpl.h"

#include <stdio.h>

using namespace DOM;

StyleSheet::StyleSheet()
{
    impl = 0;
}

StyleSheet::StyleSheet(const StyleSheet &other)
{
    impl = other.impl;
    if(impl) impl->ref();
}

StyleSheet::StyleSheet(StyleSheetImpl *i)
{
    impl = i;
    if(impl) impl->ref();
}

StyleSheet &StyleSheet::operator = (const StyleSheet &other)
{
    if ( impl != other.impl ) {
    if(impl) impl->deref();
    impl = other.impl;
    if(impl) impl->ref();
    }
    return *this;
}

StyleSheet::~StyleSheet()
{
    if(impl) impl->deref();
}

DOMString StyleSheet::type() const
{
    if(!impl) return DOMString();
    return ((StyleSheetImpl *)impl)->type();
}

bool StyleSheet::disabled() const
{
    if(!impl) return 0;
    return ((StyleSheetImpl *)impl)->disabled();
}

void StyleSheet::setDisabled( bool _disabled )
{
    if(impl)
        ((StyleSheetImpl *)impl)->setDisabled( _disabled );
}

DOM::Node StyleSheet::ownerNode() const
{
    if(!impl) return Node();
    return ((StyleSheetImpl *)impl)->ownerNode();
}

StyleSheet StyleSheet::parentStyleSheet() const
{
    if(!impl) return 0;
    return ((StyleSheetImpl *)impl)->parentStyleSheet();
}

DOMString StyleSheet::href() const
{
    if(!impl) return DOMString();
    return ((StyleSheetImpl *)impl)->href();
}

DOMString StyleSheet::title() const
{
    if(!impl) return DOMString();
    return ((StyleSheetImpl *)impl)->title();
}

MediaList StyleSheet::media() const
{
    if(!impl) return 0;
    return ((StyleSheetImpl *)impl)->media();
}

bool StyleSheet::isCSSStyleSheet() const
{
    if(!impl) return false;
    return ((StyleSheetImpl *)impl)->isCSSStyleSheet();
}

bool StyleSheet::isNull() const
{
    return (impl == 0);
}



CSSStyleSheet::CSSStyleSheet() : StyleSheet()
{
}

CSSStyleSheet::CSSStyleSheet(const CSSStyleSheet &other) : StyleSheet(other)
{
}

CSSStyleSheet::CSSStyleSheet(const StyleSheet &other)
{
    if (!other.isCSSStyleSheet())
	impl = 0;
    else
	operator=(other);
}

CSSStyleSheet::CSSStyleSheet(CSSStyleSheetImpl *impl) : StyleSheet(impl)
{
}

CSSStyleSheet &CSSStyleSheet::operator = (const CSSStyleSheet &other)
{
    StyleSheet::operator = (other);
    return *this;
}

CSSStyleSheet &CSSStyleSheet::operator = (const StyleSheet &other)
{
    if(!other.handle()->isCSSStyleSheet())
    {
        if(impl) impl->deref();
        impl = 0;
    } else {
    StyleSheet::operator = (other);
    }
    return *this;
}

CSSStyleSheet::~CSSStyleSheet()
{
}

CSSRule CSSStyleSheet::ownerRule() const
{
    if(!impl) return 0;
    return ((CSSStyleSheetImpl *)impl)->ownerRule();
}

CSSRuleList CSSStyleSheet::cssRules() const
{
    if(!impl) return (CSSRuleListImpl*)0;
    return ((CSSStyleSheetImpl *)impl)->cssRules();
}

unsigned long CSSStyleSheet::insertRule( const DOMString &rule, unsigned long index )
{
    int exceptioncode = 0;
    if(!impl) return 0;
    unsigned long retval = ((CSSStyleSheetImpl *)impl)->insertRule( rule, index, exceptioncode );
    if ( exceptioncode >= CSSException::_EXCEPTION_OFFSET )
        throw CSSException( exceptioncode - CSSException::_EXCEPTION_OFFSET );
    if ( exceptioncode )
        throw DOMException( exceptioncode );
    return retval;
}

void CSSStyleSheet::deleteRule( unsigned long index )
{
    int exceptioncode = 0;
    if(impl)
        ((CSSStyleSheetImpl *)impl)->deleteRule( index, exceptioncode );
    if ( exceptioncode >= CSSException::_EXCEPTION_OFFSET )
        throw CSSException( exceptioncode - CSSException::_EXCEPTION_OFFSET );
    if ( exceptioncode )
        throw DOMException( exceptioncode );
}

void CSSStyleSheet::addRule( const DOMString &selector, const DOMString &style, long index )
{
    if (!impl)
        return;
    int exceptioncode = 0;
    static_cast<CSSStyleSheetImpl *>(impl)->addRule( selector, style, index, exceptioncode );
    if ( exceptioncode >= CSSException::_EXCEPTION_OFFSET )
        throw CSSException( exceptioncode - CSSException::_EXCEPTION_OFFSET );
    if ( exceptioncode )
        throw DOMException( exceptioncode );
}



StyleSheetList::StyleSheetList()
{
    impl = 0;
}

StyleSheetList::StyleSheetList(const StyleSheetList &other)
{
    impl = other.impl;
    if(impl) impl->ref();
}

StyleSheetList::StyleSheetList(StyleSheetListImpl *i)
{
    impl = i;
    if(impl) impl->ref();
}

StyleSheetList &StyleSheetList::operator = (const StyleSheetList &other)
{
    if ( impl != other.impl ) {
    if(impl) impl->deref();
    impl = other.impl;
    if(impl) impl->ref();
    }
    return *this;
}

StyleSheetList::~StyleSheetList()
{
    if(impl) impl->deref();
}

unsigned long StyleSheetList::length() const
{
    if(!impl) return 0;
    return ((StyleSheetListImpl *)impl)->length();
}

StyleSheet StyleSheetList::item( unsigned long index )
{
    if(!impl) return StyleSheet();
    return ((StyleSheetListImpl *)impl)->item( index );
}

StyleSheetListImpl *StyleSheetList::handle() const
{
    return impl;
}

bool StyleSheetList::isNull() const
{
    return (impl == 0);
}

// ----------------------------------------------------------

MediaList::MediaList()
{
    impl = 0;
}

MediaList::MediaList(const MediaList &other)
{
    impl = other.impl;
    if(impl) impl->ref();
}

MediaList::MediaList(MediaListImpl *i)
{
    impl = i;
    if(impl) impl->ref();
}

MediaList &MediaList::operator = (const MediaList &other)
{
    if ( impl != other.impl ) {
    if(impl) impl->deref();
    impl = other.impl;
    if(impl) impl->ref();
    }
    return *this;
}

MediaList::~MediaList()
{
    if(impl) impl->deref();
}

DOM::DOMString MediaList::mediaText() const
{
    if(!impl) return DOMString();
    return static_cast<MediaListImpl *>(impl)->mediaText();
}

void MediaList::setMediaText(const DOM::DOMString &value )
{
    if(impl)
        static_cast<MediaListImpl *>(impl)->setMediaText( value );
}

unsigned long MediaList::length() const
{
    if(!impl) return 0;
    return ((MediaListImpl *)impl)->length();
}

DOM::DOMString MediaList::item(unsigned long index) const
{
    if(!impl) return DOMString();
    return ((MediaListImpl *)impl)->item( index );
}

void MediaList::deleteMedium(const DOM::DOMString &oldMedium)
{
    if(impl)
        ((MediaListImpl *)impl)->deleteMedium( oldMedium );
}

void MediaList::appendMedium(const DOM::DOMString &newMedium)
{
    if(impl)
        ((MediaListImpl *)impl)->appendMedium( newMedium );
}

MediaListImpl *MediaList::handle() const
{
    return impl;
}

bool MediaList::isNull() const
{
    return (impl == 0);
}

// ----------------------------------------------------------

LinkStyle::LinkStyle()
{
    node = 0;
}

LinkStyle::LinkStyle(const LinkStyle &other)
{
    node = other.node;
    if(node) node->ref();
}

LinkStyle & LinkStyle::operator = (const LinkStyle &other)
{
    if ( node != other.node ) {
    if(node) node->deref();
    node = other.node;
    if(node) node->ref();
    }
    return *this;
}

LinkStyle & LinkStyle::operator = (const Node &other)
{
    if(node) node->deref();
    node = 0;
    // ### add processing instructions
    NodeImpl *n = other.handle();

    // ### check link is really linking a style sheet
    if( n && n->isElementNode() &&
	(n->hasTagName(HTMLTags::style()) || n->hasTagName(HTMLTags::link()))) {
    node = n;
    if(node) node->ref();
    }
    return *this;
}

LinkStyle::~LinkStyle()
{
    if(node) node->deref();
}

StyleSheet LinkStyle::sheet()
{
    if (!node)
        return StyleSheet();

    // ### add PI
    return 
	(node->hasTagName(HTMLTags::style())) ?
	static_cast<HTMLStyleElementImpl *>(node)->sheet()
	: ( node->hasTagName(HTMLTags::link()) ?
	    static_cast<HTMLLinkElementImpl *>(node)->sheet()
	    : StyleSheet() );
}

bool LinkStyle::isNull() const
{
    return (node == 0);
}


// ----------------------------------------------------------

DocumentStyle::DocumentStyle()
{
    doc = 0;
}

DocumentStyle::DocumentStyle(const DocumentStyle &other)
{
    doc = other.doc;
    if(doc) doc->ref();
}

DocumentStyle & DocumentStyle::operator = (const DocumentStyle &other)
{
    if ( doc != other.doc ) {
    if(doc) doc->deref();
    doc = other.doc;
    if(doc) doc->ref();
    }
    return *this;
}

DocumentStyle & DocumentStyle::operator = (const Document &other)
{
    DocumentImpl *odoc = static_cast<DocumentImpl *>(other.handle());
    if ( doc != odoc ) {
    if(doc) doc->deref();
	doc = odoc;
    if(doc) doc->ref();
    }
    return *this;
}

DocumentStyle::~DocumentStyle()
{
    if(doc) doc->deref();
}

StyleSheetList DocumentStyle::styleSheets()
{
    return doc->styleSheets();
}

DOMString DocumentStyle::preferredStylesheetSet()
{
    return doc->preferredStylesheetSet();
}

void DocumentStyle::setSelectedStylesheetSet(const DOMString& aStr)
{
    return doc->setSelectedStylesheetSet(aStr);
}

DOMString DocumentStyle::selectedStylesheetSet()
{
    return doc->selectedStylesheetSet();
}

bool DocumentStyle::isNull() const
{
    return (doc == 0);
}

