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
#include "CSSRule.h"
#include "CSSHelper.h"
#include "CSSRuleList.h"
#include "DOMException.h"
#include "CSSStyleSheet.h"
#include "DOMExceptionImpl.h"
#include "CSSStyleSheetImpl.h"

#include "kdom/data/CSSConstants.h"
#include "CSSStyleSheet.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin CSSStyleSheet::s_hashTable 3
 ownerRule	CSSStyleSheetConstants::OwnerRule	DontDelete|ReadOnly
 cssRules	CSSStyleSheetConstants::CssRules	DontDelete|ReadOnly
@end
@begin CSSStyleSheetProto::s_hashTable 3
 insertRule	CSSStyleSheetConstants::InsertRule	DontDelete|Function 2
 deleteRule	CSSStyleSheetConstants::DeleteRule	DontDelete|Function 1
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("CSSStyleSheet", CSSStyleSheetProto, CSSStyleSheetProtoFunc)

Value CSSStyleSheet::getValueProperty(ExecState *exec, int token) const
{
	switch(token)
	{
		case CSSStyleSheetConstants::OwnerRule:
			return getDOMCSSRule(exec, ownerRule());
		case CSSStyleSheetConstants::CssRules:
			return safe_cache<CSSRuleList>(exec, cssRules());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

Value CSSStyleSheetProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
	KDOM_CHECK_THIS(CSSStyleSheet)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case CSSStyleSheetConstants::InsertRule:
		{
			DOMString rule = toDOMString(exec, args[0]);
			unsigned long index = args[1].toUInt32(exec);
			obj.insertRule(rule, index);
			return Undefined();
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<CSSStyleSheetImpl *>(d))

CSSStyleSheet CSSStyleSheet::null;

CSSStyleSheet::CSSStyleSheet() : StyleSheet()
{
}

CSSStyleSheet::CSSStyleSheet(const CSSStyleSheet &other) : StyleSheet(other)
{
}

CSSStyleSheet::CSSStyleSheet(const StyleSheet &other)
{
	if(!other.isCSSStyleSheet())
		d = 0;
	else
		(*this) = other;
}

CSSStyleSheet::CSSStyleSheet(CSSStyleSheetImpl *i) : StyleSheet(i)
{
}

CSSStyleSheet::~CSSStyleSheet()
{
}

CSSStyleSheet &CSSStyleSheet::operator=(const CSSStyleSheet &other)
{
	StyleSheet::operator=(other);
	return *this;
}

CSSStyleSheet &CSSStyleSheet::operator=(const StyleSheet &other)
{
	if(!other.isCSSStyleSheet())
	{
		if(d)
			d->deref();

		d = 0;
	}
	else
		StyleSheet::operator=(other);

	return *this;
}

CSSRule CSSStyleSheet::ownerRule() const
{
	if(!d)
		return CSSRule::null;

	return CSSRule(impl->ownerRule());
}

CSSRuleList CSSStyleSheet::cssRules() const
{
	if(!d)
		return CSSRuleList::null;

	return CSSRuleList(impl->cssRules());
}

unsigned long CSSStyleSheet::insertRule(const DOMString &rule, unsigned long index)
{
	if(!d)
		return 0;

	return impl->insertRule(rule, index);
}

void CSSStyleSheet::deleteRule(unsigned long index)
{
	if(d)
		impl->deleteRule(index);
}

DOMString CSSStyleSheet::type() const
{
	return "text/css";
}

void CSSStyleSheet::setCssText(const DOMString &cssText)
{
	if(d)
		impl->parseString(cssText);
}

// vim:ts=4:noet
