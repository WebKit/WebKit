/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "CSSStyleRuleImpl.h"
#include "CSSStyleSheetImpl.h"
#include "CSSStyleDeclarationImpl.h"

using namespace KDOM;

CSSStyleRuleImpl::CSSStyleRuleImpl(StyleBaseImpl *parent) : CSSRuleImpl(parent)
{
	m_type = STYLE_RULE;

	m_style = 0;
	m_selector = 0;
}

CSSStyleRuleImpl::~CSSStyleRuleImpl()
{
	if(m_style)
	{
		m_style->setParent(0);
		m_style->deref();
	}

	delete m_selector;
}

DOMStringImpl *CSSStyleRuleImpl::selectorText() const
{
	if(m_selector && m_selector->first())
	{
		// ### m_selector will be a single selector hopefully. so ->first() will disappear
		CSSSelector *cs = m_selector->first();
		//cs->print(); // debug
		return cs->selectorText();
	}

	return 0;
}

void CSSStyleRuleImpl::setSelectorText(DOMStringImpl *)
{
	// FIXME!
}

bool CSSStyleRuleImpl::parseString(const DOMString &, bool)
{
	// FIXME!
	return false;
}

CSSStyleDeclarationImpl *CSSStyleRuleImpl::style() const
{
	return m_style;
}

void CSSStyleRuleImpl::setSelector(QPtrList<CSSSelector> *selector)
{
	m_selector = selector;
}

void CSSStyleRuleImpl::setDeclaration(CSSStyleDeclarationImpl *style)
{
	KDOM_SAFE_SET(m_style, style);
}

void CSSStyleRuleImpl::setNonCSSHints()
{
	CSSSelector *s = m_selector->first();
	while(s)
	{
		s->nonCSSHint = true;
		s = m_selector->next();
	}
}

// vim:ts=4:noet
