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
#include <kdom/Shared.h>
#include "CSSRule.h"
#include "CSSHelper.h"
#include "CSSRuleImpl.h"
#include "DOMException.h"
#include "CSSStyleSheet.h"
#include "DOMExceptionImpl.h"

#include "CSSConstants.h"
#include "CSSRule.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin CSSRule::s_hashTable 5
 type				CSSRuleConstants::Type				DontDelete|ReadOnly
 cssText			CSSRuleConstants::CssText			DontDelete|ReadOnly
 parentStyleSheet	CSSRuleConstants::ParentStyleSheet	DontDelete|ReadOnly
 parentRule			CSSRuleConstants::ParentRule		DontDelete|ReadOnly
@end
*/

Value CSSRule::getValueProperty(ExecState *exec, int token) const
{
	switch(token)
	{
		case CSSRuleConstants::Type:
			return Number(type());
		case CSSRuleConstants::CssText:
			return getDOMString(cssText());
		case CSSRuleConstants::ParentStyleSheet:
			return safe_cache<CSSStyleSheet>(exec, parentStyleSheet());
		case CSSRuleConstants::ParentRule:
			return getDOMCSSRule(exec, parentRule());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

void CSSRule::putValueProperty(ExecState *exec, int token, const Value &value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case CSSRuleConstants::CssText:
			setCssText(toDOMString(exec, value));
			break;
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
}

CSSRule CSSRule::null;

CSSRule::CSSRule() : d(0)
{
}

CSSRule::CSSRule(CSSRuleImpl *i) : d(i)
{
	if(d)
		d->ref();
}

CSSRule::CSSRule(const CSSRule &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(CSSRule)

unsigned short CSSRule::type() const
{
	if(!d)
		return UNKNOWN_RULE;

	return d->type();
}

void CSSRule::setCssText(const DOMString &text)
{
	if(d)
		d->setCssText(text);
}

DOMString CSSRule::cssText() const
{
	if(!d)
		return DOMString();

	return d->cssText();
}

CSSStyleSheet CSSRule::parentStyleSheet() const
{
	if(!d)
		return CSSStyleSheet::null;

	return CSSStyleSheet(d->parentStyleSheet());
}

CSSRule CSSRule::parentRule() const
{
	if(!d)
		return CSSRule::null;

	return CSSRule(d->parentRule());
}

// vim:ts=4:noet
