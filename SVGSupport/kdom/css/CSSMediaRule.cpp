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
#include "MediaList.h"
#include "DOMException.h"
#include "CSSMediaRule.h"
#include "DOMExceptionImpl.h"
#include "CSSMediaRuleImpl.h"

#include "kdom/data/CSSConstants.h"
#include "CSSMediaRule.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin CSSMediaRule::s_hashTable 3
 media				CSSMediaRuleConstants::Media		DontDelete|ReadOnly
 cssRules			CSSMediaRuleConstants::CssRules		DontDelete|ReadOnly
@end
@begin CSSMediaRuleProto::s_hashTable 3
 insertRule		CSSMediaRuleConstants::InsertRule    DontDelete|Function 2
 deleteRule		CSSMediaRuleConstants::DeleteRule    DontDelete|Function 1
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("CSSMediaRule", CSSMediaRuleProto, CSSMediaRuleProtoFunc)

ValueImp *CSSMediaRule::getValueProperty(ExecState *exec, int token) const
{
	switch(token)
	{
		case CSSMediaRuleConstants::Media:
			return safe_cache<MediaList>(exec, media());
		case CSSMediaRuleConstants::CssRules:
			return safe_cache<CSSRuleList>(exec, cssRules());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

ValueImp *CSSMediaRuleProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(CSSMediaRule)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case CSSMediaRuleConstants::InsertRule:
		{
			DOMString rule = toDOMString(exec, args[0]);
			unsigned long index = args[1]->toUInt32(exec);
			return Number(obj.insertRule(rule, index));
		}
		case CSSMediaRuleConstants::DeleteRule:
		{
			unsigned long index = args[0]->toUInt32(exec);
			obj.deleteRule(index);
			return Undefined();
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<CSSMediaRuleImpl *>(d))

CSSMediaRule CSSMediaRule::null;

CSSMediaRule::CSSMediaRule() : CSSRule()
{
}

CSSMediaRule::CSSMediaRule(const CSSMediaRule &other) : CSSRule()
{
	(*this) = other;
}

CSSMediaRule::CSSMediaRule(const CSSRule &other) : CSSRule()
{
	(*this) = other;
}

CSSMediaRule::CSSMediaRule(CSSMediaRuleImpl *i) : CSSRule(i)
{
}

CSSMediaRule::~CSSMediaRule()
{
}

CSSMediaRule &CSSMediaRule::operator=(const CSSMediaRule &other)
{
	CSSRule::operator=(other);
	return *this;
}

KDOM_CSSRULE_DERIVED_ASSIGN_OP(CSSMediaRule, MEDIA_RULE)

MediaList CSSMediaRule::media() const
{
	if(!d)
		return MediaList::null;

	return MediaList(impl->media());
}

CSSRuleList CSSMediaRule::cssRules() const
{
	if(!d)
		return CSSRuleList::null;

	return CSSRuleList(impl->cssRules());
}

unsigned long CSSMediaRule::insertRule(const DOMString &rule, unsigned long index)
{
	if(!d)
		return index;

	return impl->insertRule(rule, index);
}

void CSSMediaRule::deleteRule(unsigned long index)
{
	if(d)
		return impl->deleteRule(index);
}

// vim:ts=4:noet
