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
#include "CSSPageRule.h"
#include "DOMException.h"
#include "CSSPageRuleImpl.h"
#include "DOMExceptionImpl.h"
#include "CSSStyleDeclaration.h"

#include "kdom/data/CSSConstants.h"
#include "CSSPageRule.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin CSSPageRule::s_hashTable 3
 selectorText	CSSPageRuleConstants::SelectorText	DontDelete|ReadOnly
 style			CSSPageRuleConstants::Style			DontDelete|ReadOnly
@end
*/

Value CSSPageRule::getValueProperty(ExecState *exec, int token) const
{
	switch(token)
	{
		case CSSPageRuleConstants::SelectorText:
			return getDOMString(selectorText());
		case CSSPageRuleConstants::Style:
			return safe_cache<CSSStyleDeclaration>(exec, style());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

void CSSPageRule::putValueProperty(ExecState *exec, int token, const Value &value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case CSSPageRuleConstants::SelectorText:
			setSelectorText(toDOMString(exec, value));
			break;
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
}

// The qdom way...
#define impl (static_cast<CSSPageRuleImpl *>(d))

CSSPageRule CSSPageRule::null;

CSSPageRule::CSSPageRule() : CSSRule()
{
}

CSSPageRule::CSSPageRule(const CSSPageRule &other) : CSSRule()
{
	(*this) = other;
}

CSSPageRule::CSSPageRule(const CSSRule &other) : CSSRule()
{
	(*this) = other;
}

CSSPageRule::CSSPageRule(CSSPageRuleImpl *i) : CSSRule(i)
{
}

CSSPageRule::~CSSPageRule()
{
}

CSSPageRule &CSSPageRule::operator=(const CSSPageRule &other)
{
	CSSRule::operator=(other);
	return *this;
}

KDOM_CSSRULE_DERIVED_ASSIGN_OP(CSSPageRule, PAGE_RULE)

void CSSPageRule::setSelectorText(const DOMString &text)
{
	if(d)
		impl->setSelectorText(text);
}

DOMString CSSPageRule::selectorText() const
{
	if(!d)
		return DOMString();

	return impl->selectorText();
}

CSSStyleDeclaration CSSPageRule::style() const
{
	if(!d)
		return CSSStyleDeclaration::null;

	return CSSStyleDeclaration(impl->style());
}

// vim:ts=4:noet
