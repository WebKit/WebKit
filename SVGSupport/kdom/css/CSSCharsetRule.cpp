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

#include "Ecma.h"
#include "DOMException.h"
#include "CSSCharsetRule.h"
#include "DOMExceptionImpl.h"
#include "CSSCharsetRuleImpl.h"

#include "kdom/data/CSSConstants.h"
#include "CSSCharsetRule.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin CSSCharsetRule::s_hashTable 2
 encoding	CSSCharsetRuleConstants::Encoding	DontDelete
@end
*/

ValueImp *CSSCharsetRule::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
		case CSSCharsetRuleConstants::Encoding:
			return getDOMString(encoding());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

void CSSCharsetRule::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case CSSCharsetRuleConstants::Encoding:
			setEncoding(toDOMString(exec, value));
			break;
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
}

// The qdom way...
#define impl (static_cast<CSSCharsetRuleImpl *>(d))

CSSCharsetRule CSSCharsetRule::null;

CSSCharsetRule::CSSCharsetRule() : CSSRule()
{
}

CSSCharsetRule::CSSCharsetRule(const CSSCharsetRule &other) : CSSRule()
{
	(*this) = other;
}

CSSCharsetRule::CSSCharsetRule(const CSSRule &other) : CSSRule()
{
	(*this) = other;
}

CSSCharsetRule::CSSCharsetRule(CSSCharsetRuleImpl *i) : CSSRule(i)
{
}

CSSCharsetRule::~CSSCharsetRule()
{
}

CSSCharsetRule &CSSCharsetRule::operator=(const CSSCharsetRule &other)
{
	CSSRule::operator=(other);
	return *this;
}

KDOM_CSSRULE_DERIVED_ASSIGN_OP(CSSCharsetRule, CHARSET_RULE)

void CSSCharsetRule::setEncoding(const DOMString &encoding)
{
	if(d)
		impl->setEncoding(encoding);
}

DOMString CSSCharsetRule::encoding() const
{
	if(!d)
		return DOMString();

	return impl->encoding();
}

// vim:ts=4:noet
