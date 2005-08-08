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
#include "MediaList.h"
#include "CSSStyleSheet.h"
#include "CSSImportRule.h"
#include "CSSImportRuleImpl.h"

#include "kdom/data/CSSConstants.h"
#include "CSSImportRule.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin CSSImportRule::s_hashTable 5
 href		CSSImportRuleConstants::Href		DontDelete|ReadOnly
 media		CSSImportRuleConstants::Media		DontDelete|ReadOnly
 styleSheet	CSSImportRuleConstants::StyleSheet	DontDelete|ReadOnly
@end
*/

ValueImp *CSSImportRule::getValueProperty(ExecState *exec, int token) const
{
	switch(token)
	{
		case CSSImportRuleConstants::Href:
			return getDOMString(href());
		case CSSImportRuleConstants::Media:
			return safe_cache<MediaList>(exec, media());
		case CSSImportRuleConstants::StyleSheet:
			return safe_cache<CSSStyleSheet>(exec, styleSheet());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

// The qdom way...
#define impl (static_cast<CSSImportRuleImpl *>(d))

CSSImportRule CSSImportRule::null;

CSSImportRule::CSSImportRule() : CSSRule()
{
}

CSSImportRule::CSSImportRule(const CSSImportRule &other) : CSSRule()
{
	(*this) = other;
}

CSSImportRule::CSSImportRule(const CSSRule &other) : CSSRule()
{
	(*this) = other;
}

CSSImportRule::CSSImportRule(CSSImportRuleImpl *i) : CSSRule(i)
{
}

CSSImportRule::~CSSImportRule()
{
}

CSSImportRule &CSSImportRule::operator=(const CSSImportRule &other)
{
	CSSRule::operator=(other);
	return *this;
}

KDOM_CSSRULE_DERIVED_ASSIGN_OP(CSSImportRule, IMPORT_RULE)

DOMString CSSImportRule::href() const
{
	if(!d)
		return DOMString();

	return impl->href();
}

MediaList CSSImportRule::media() const
{
	if(!d)
		return MediaList::null;

	return MediaList(impl->media());
}

CSSStyleSheet CSSImportRule::styleSheet() const
{
	if(!d)
		return CSSStyleSheet::null;

	return CSSStyleSheet(impl->styleSheet());
}

// vim:ts=4:noet
