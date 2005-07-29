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
#include "CSSFontFaceRule.h"
#include "CSSFontFaceRuleImpl.h"
#include "CSSStyleDeclaration.h"

#include "kdom/data/CSSConstants.h"
#include "CSSFontFaceRule.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin CSSFontFaceRule::s_hashTable 2
 style	CSSFontFaceRuleConstants::Style	DontDelete|ReadOnly
@end
*/

Value CSSFontFaceRule::getValueProperty(ExecState *exec, int token) const
{
	switch(token)
	{
		case CSSFontFaceRuleConstants::Style:
			return safe_cache<CSSStyleDeclaration>(exec, style());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

// The qdom way...
#define impl (static_cast<CSSFontFaceRuleImpl *>(d))

CSSFontFaceRule CSSFontFaceRule::null;

CSSFontFaceRule::CSSFontFaceRule() : CSSRule()
{
}

CSSFontFaceRule::CSSFontFaceRule(const CSSFontFaceRule &other) : CSSRule()
{
	(*this) = other;
}

CSSFontFaceRule::CSSFontFaceRule(const CSSRule &other) : CSSRule()
{
	(*this) = other;
}

CSSFontFaceRule::CSSFontFaceRule(CSSFontFaceRuleImpl *i) : CSSRule(i)
{
}

CSSFontFaceRule::~CSSFontFaceRule()
{
}

CSSFontFaceRule &CSSFontFaceRule::operator=(const CSSFontFaceRule &other)
{
	CSSRule::operator=(other);
	return *this;
}

KDOM_CSSRULE_DERIVED_ASSIGN_OP(CSSFontFaceRule, FONT_FACE_RULE)

CSSStyleDeclaration CSSFontFaceRule::style() const
{
	if(!d)
		return CSSStyleDeclaration::null;

	return CSSStyleDeclaration(impl->style());
}

// vim:ts=4:noet
