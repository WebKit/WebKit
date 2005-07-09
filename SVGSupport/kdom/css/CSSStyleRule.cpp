/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

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

#include "Ecma.h"
#include "DOMException.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSStyleRuleImpl.h"
#include "DOMExceptionImpl.h"
#include "CSSStyleDeclaration.h"

#include "CSSConstants.h"
#include "CSSStyleRule.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin CSSStyleRule::s_hashTable 3
 selectorText	CSSStyleRuleConstants::SelectorText		DontDelete|ReadOnly
 style			CSSStyleRuleConstants::Style			DontDelete|ReadOnly
@end
*/

Value CSSStyleRule::getValueProperty(ExecState *exec, int token) const
{
	switch(token)
	{
		case CSSStyleRuleConstants::SelectorText:
			return getDOMString(selectorText());
		case CSSStyleRuleConstants::Style:
			return safe_cache<CSSStyleDeclaration>(exec, style());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

void CSSStyleRule::putValueProperty(ExecState *exec, int token, const Value &value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case CSSStyleRuleConstants::SelectorText:
			setSelectorText(toDOMString(exec, value));
			break;
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
}

// The qdom way...
#define impl (static_cast<CSSStyleRuleImpl *>(d))

CSSStyleRule CSSStyleRule::null;

CSSStyleRule::CSSStyleRule() : CSSRule()
{
}

CSSStyleRule::CSSStyleRule(const CSSStyleRule &other) : CSSRule()
{
	(*this) = other;
}

CSSStyleRule::CSSStyleRule(const CSSRule &other) : CSSRule()
{
	(*this) = other;
}

CSSStyleRule::CSSStyleRule(CSSStyleRuleImpl *i) : CSSRule(i)
{
}

CSSStyleRule::~CSSStyleRule()
{
}

CSSStyleRule &CSSStyleRule::operator=(const CSSStyleRule &other)
{
	CSSRule::operator=(other);
	return *this;
}

KDOM_CSSRULE_DERIVED_ASSIGN_OP(CSSStyleRule, STYLE_RULE)

void CSSStyleRule::setSelectorText(const DOMString &selectorText)
{
	if(d)
		impl->setSelectorText(selectorText);
}

DOMString CSSStyleRule::selectorText() const
{
	if(!d)
		return DOMString();

	return impl->selectorText();
}

CSSStyleDeclaration CSSStyleRule::style() const
{
	if(!d)
		return CSSStyleDeclaration::null;

	return CSSStyleDeclaration(impl->style());
}

// vim:ts=4:noet
