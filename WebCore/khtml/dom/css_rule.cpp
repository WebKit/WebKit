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

#include "css/css_ruleimpl.h"

using namespace DOM;

CSSRule::CSSRule()
{
    impl = 0;
}

CSSRule::CSSRule(const CSSRule &other)
{
    impl = other.impl;
    if(impl) impl->ref();
}

CSSRule::CSSRule(CSSRuleImpl *i)
{
    impl = i;
    if(impl) impl->ref();
}

CSSRule &CSSRule::operator = (const CSSRule &other)
{
    if ( impl != other.impl ) {
    if(impl) impl->deref();
    impl = other.impl;
    if(impl) impl->ref();
    }
    return *this;
}

CSSRule::~CSSRule()
{
    if(impl) impl->deref();
}

unsigned short CSSRule::type() const
{
    if(!impl) return 0;
    return ((CSSRuleImpl *)impl)->type();
}

DOMString CSSRule::cssText() const
{
    if(!impl) return DOMString();
    return impl->cssText();
}

void CSSRule::setCssText( const DOMString &value )
{
    if(!impl) return;
    impl->setCssText(value);
}

CSSStyleSheet CSSRule::parentStyleSheet() const
{
    if(!impl) return CSSStyleSheet();
    return ((CSSRuleImpl *)impl)->parentStyleSheet();
}

CSSRule CSSRule::parentRule() const
{
    if(!impl) return 0;
    return ((CSSRuleImpl *)impl)->parentRule();
}

CSSRuleImpl *CSSRule::handle() const
{
    return impl;
}

bool CSSRule::isNull() const
{
    return (impl == 0);
}

void CSSRule::assignOther( const CSSRule &other, RuleType thisType )
{
    if (other.type() != thisType ) {
	if ( impl ) impl->deref();
	impl = 0;
    } else
	CSSRule::operator = ( other );
}

// ----------------------------------------------------------


CSSCharsetRule::CSSCharsetRule()
    : CSSRule()
{
}

CSSCharsetRule::CSSCharsetRule(const CSSCharsetRule &other) : CSSRule(other)
{
}

CSSCharsetRule::CSSCharsetRule(const CSSRule &other)
{
    impl = 0;
    operator=(other);
}

CSSCharsetRule::CSSCharsetRule(CSSCharsetRuleImpl *impl) : CSSRule(impl)
{
}

CSSCharsetRule &CSSCharsetRule::operator = (const CSSCharsetRule &other)
{
    CSSRule::operator = (other);
    return *this;
}

CSSCharsetRule &CSSCharsetRule::operator = (const CSSRule &other)
{
    assignOther( other, CSSRule::CHARSET_RULE);
    return *this;
}

CSSCharsetRule::~CSSCharsetRule()
{
}

DOMString CSSCharsetRule::encoding() const
{
    if(!impl) return DOMString();
    return ((CSSCharsetRuleImpl*)impl)->encoding();
}

void CSSCharsetRule::setEncoding( const DOMString &value )
{
    ((CSSCharsetRuleImpl*)impl)->setEncoding(value);
}


// ----------------------------------------------------------


CSSFontFaceRule::CSSFontFaceRule() : CSSRule()
{
}

CSSFontFaceRule::CSSFontFaceRule(const CSSFontFaceRule &other) : CSSRule(other)
{
}

CSSFontFaceRule::CSSFontFaceRule(const CSSRule &other)
{
    impl = 0;
    operator=(other);
}

CSSFontFaceRule::CSSFontFaceRule(CSSFontFaceRuleImpl *impl) : CSSRule(impl)
{
}

CSSFontFaceRule &CSSFontFaceRule::operator = (const CSSFontFaceRule &other)
{
    CSSRule::operator = (other);
    return *this;
}

CSSFontFaceRule &CSSFontFaceRule::operator = (const CSSRule &other)
{
    assignOther( other, CSSRule::FONT_FACE_RULE );
    return *this;
}

CSSFontFaceRule::~CSSFontFaceRule()
{
}

CSSStyleDeclaration CSSFontFaceRule::style() const
{
    if(!impl) return CSSStyleDeclaration();
    return ((CSSFontFaceRuleImpl *)impl)->style();
}


// ----------------------------------------------------------


CSSImportRule::CSSImportRule() : CSSRule()
{
}

CSSImportRule::CSSImportRule(const CSSImportRule &other) : CSSRule(other)
{
}

CSSImportRule::CSSImportRule(const CSSRule &other)
{
    impl = 0;
    operator=(other);
}

CSSImportRule::CSSImportRule(CSSImportRuleImpl *impl) : CSSRule(impl)
{
}

CSSImportRule &CSSImportRule::operator = (const CSSImportRule &other)
{
    CSSRule::operator = (other);
    return *this;
}

CSSImportRule &CSSImportRule::operator = (const CSSRule &other)
{
    assignOther( other, CSSRule::IMPORT_RULE );
    return *this;
}

CSSImportRule::~CSSImportRule()
{
}

DOMString CSSImportRule::href() const
{
    if(!impl) return DOMString();
    return ((CSSImportRuleImpl *)impl)->href();
}

MediaList CSSImportRule::media() const
{
    if(!impl) return MediaList();
    return ((CSSImportRuleImpl *)impl)->media();
}

CSSStyleSheet CSSImportRule::styleSheet() const
{
    if(!impl) return CSSStyleSheet();
    return ((CSSImportRuleImpl *)impl)->styleSheet();
}


// ----------------------------------------------------------


CSSMediaRule::CSSMediaRule() : CSSRule()
{
}

CSSMediaRule::CSSMediaRule(const CSSMediaRule &other) : CSSRule(other)
{
}

CSSMediaRule::CSSMediaRule(const CSSRule &other)
{
    impl = 0;
    operator=(other);
}

CSSMediaRule::CSSMediaRule(CSSMediaRuleImpl *impl) : CSSRule(impl)
{
}

CSSMediaRule &CSSMediaRule::operator = (const CSSMediaRule &other)
{
    CSSRule::operator = (other);
    return *this;
}

CSSMediaRule &CSSMediaRule::operator = (const CSSRule &other)
{
    assignOther( other, CSSRule::MEDIA_RULE );
    return *this;
}

CSSMediaRule::~CSSMediaRule()
{
}

MediaList CSSMediaRule::media() const
{
    if(!impl) return MediaList();
    return ((CSSMediaRuleImpl *)impl)->media();
}

CSSRuleList CSSMediaRule::cssRules() const
{
    if(!impl) return CSSRuleList();
    return ((CSSMediaRuleImpl *)impl)->cssRules();
}

unsigned long CSSMediaRule::insertRule( const DOMString &rule, unsigned long index )
{
    if(!impl) return 0;
    return ((CSSMediaRuleImpl *)impl)->insertRule( rule, index );
}

void CSSMediaRule::deleteRule( unsigned long index )
{
    if(impl)
        ((CSSMediaRuleImpl *)impl)->deleteRule( index );
}


// ----------------------------------------------------------


CSSPageRule::CSSPageRule() : CSSRule()
{
}

CSSPageRule::CSSPageRule(const CSSPageRule &other) : CSSRule(other)
{
}

CSSPageRule::CSSPageRule(const CSSRule &other)
{
    impl = 0;
    operator=(other);
}

CSSPageRule::CSSPageRule(CSSPageRuleImpl *impl) : CSSRule(impl)
{
}

CSSPageRule &CSSPageRule::operator = (const CSSPageRule &other)
{
    CSSRule::operator = (other);
    return *this;
}

CSSPageRule &CSSPageRule::operator = (const CSSRule &other)
{
    assignOther( other, CSSRule::PAGE_RULE );
    return *this;
}

CSSPageRule::~CSSPageRule()
{
}

DOMString CSSPageRule::selectorText() const
{
    if(!impl) return DOMString();
    return ((CSSPageRuleImpl*)impl)->selectorText();
}

void CSSPageRule::setSelectorText( const DOMString &value )
{
    ((CSSPageRuleImpl*)impl)->setSelectorText(value);
}

CSSStyleDeclaration CSSPageRule::style() const
{
    if(!impl) return CSSStyleDeclaration();
    return ((CSSPageRuleImpl *)impl)->style();
}


// ----------------------------------------------------------

CSSStyleRule::CSSStyleRule() : CSSRule()
{
}

CSSStyleRule::CSSStyleRule(const CSSStyleRule &other)
    : CSSRule(other)
{
}

CSSStyleRule::CSSStyleRule(const CSSRule &other)
{
    impl = 0;
    operator=(other);
}


CSSStyleRule::CSSStyleRule(CSSStyleRuleImpl *impl)
    : CSSRule(impl)
{
}

CSSStyleRule &CSSStyleRule::operator = (const CSSStyleRule &other)
{
    CSSRule::operator = (other);
    return *this;
}

CSSStyleRule &CSSStyleRule::operator = (const CSSRule &other)
{
    assignOther( other, CSSRule::STYLE_RULE );
    return *this;
}

CSSStyleRule::~CSSStyleRule()
{
}

DOMString CSSStyleRule::selectorText() const
{
    if(!impl) return DOMString();
    return ((CSSStyleRuleImpl*)impl)->selectorText();
}

void CSSStyleRule::setSelectorText( const DOMString &value )
{
    ((CSSStyleRuleImpl*)impl)->setSelectorText(value);
}

CSSStyleDeclaration CSSStyleRule::style() const
{
    if(!impl) return CSSStyleDeclaration();
    return ((CSSStyleRuleImpl *)impl)->style();
}


// ----------------------------------------------------------


CSSUnknownRule::CSSUnknownRule() : CSSRule()
{
}

CSSUnknownRule::CSSUnknownRule(const CSSUnknownRule &other)
    : CSSRule(other)
{
}

CSSUnknownRule::CSSUnknownRule(const CSSRule &other)
{
    impl = 0;
    operator=(other);
}

CSSUnknownRule::CSSUnknownRule(CSSUnknownRuleImpl *impl)
    : CSSRule(impl)
{
}

CSSUnknownRule &CSSUnknownRule::operator = (const CSSUnknownRule &other)
{
    CSSRule::operator = (other);
    return *this;
}

CSSUnknownRule &CSSUnknownRule::operator = (const CSSRule &other)
{
    assignOther( other, CSSRule::UNKNOWN_RULE );
    return *this;
}

CSSUnknownRule::~CSSUnknownRule()
{
}


// ----------------------------------------------------------

CSSRuleList::CSSRuleList()
{
    impl = 0;
}

CSSRuleList::CSSRuleList(const CSSRuleList &other)
{
    impl = other.impl;
    if(impl) impl->ref();
}

CSSRuleList::CSSRuleList(CSSRuleListImpl *i)
{
    impl = i;
    if(impl) impl->ref();
}

CSSRuleList &CSSRuleList::operator = (const CSSRuleList &other)
{
    if ( impl != other.impl ) {
    if(impl) impl->deref();
    impl = other.impl;
    if(impl) impl->ref();
    }
    return *this;
}

CSSRuleList::~CSSRuleList()
{
    if(impl) impl->deref();
}

unsigned long CSSRuleList::length() const
{
    if(!impl) return 0;
    return ((CSSRuleListImpl *)impl)->length();
    return 0;
}

CSSRule CSSRuleList::item( unsigned long index )
{
    if(!impl) return CSSRule();
    return ((CSSRuleListImpl *)impl)->item( index );
}

CSSRuleListImpl *CSSRuleList::handle() const
{
    return impl;
}

bool CSSRuleList::isNull() const
{
    return (impl == 0);
}




