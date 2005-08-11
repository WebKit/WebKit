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
#include "CSSRule.h"
#include "CSSValue.h"
#include "CDFInterface.h"
#include "DOMException.h"
#include "DOMExceptionImpl.h"
#include "CSSStyleDeclaration.h"
#include "CSSStyleDeclarationImpl.h"

#include "kdom/data/CSSConstants.h"
#include "CSSStyleDeclaration.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin CSSStyleDeclaration::s_hashTable 5
 cssText	CSSStyleDeclarationConstants::CssText		DontDelete
 length		CSSStyleDeclarationConstants::Length		DontDelete|ReadOnly
 parentRule	CSSStyleDeclarationConstants::ParentRule	DontDelete|ReadOnly
@end
@begin CSSStyleDeclarationProto::s_hashTable 7
 getPropertyValue		CSSStyleDeclarationConstants::GetPropertyValue        DontDelete|Function 1
 getPropertyCSSValue	CSSStyleDeclarationConstants::GetPropertyCSSValue     DontDelete|Function 1
 removeProperty			CSSStyleDeclarationConstants::RemoveProperty          DontDelete|Function 1
 getPropertyPriority	CSSStyleDeclarationConstants::GetPropertyPriority     DontDelete|Function 1
 setProperty			CSSStyleDeclarationConstants::SetProperty             DontDelete|Function 3
 item					CSSStyleDeclarationConstants::Item                    DontDelete|Function 1
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("CSSStyleDeclaration", CSSStyleDeclarationProto, CSSStyleDeclarationProtoFunc)

ValueImp *CSSStyleDeclaration::getValueProperty(ExecState *exec, int token) const
{
	switch(token)
	{
		case CSSStyleDeclarationConstants::CssText:
			return getDOMString(cssText());
		case CSSStyleDeclarationConstants::Length:
			return Number(length());
		case CSSStyleDeclarationConstants::ParentRule:
			return getDOMCSSRule(exec, parentRule());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

void CSSStyleDeclaration::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case CSSStyleDeclarationConstants::CssText:
			setCssText(toDOMString(exec, value));
			break;
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
}

ValueImp *CSSStyleDeclarationProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(CSSStyleDeclaration)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case CSSStyleDeclarationConstants::GetPropertyValue:
		{
			DOMString propertyName = toDOMString(exec, args[0]);
			return getDOMString(obj.getPropertyValue(propertyName));
		}
		case CSSStyleDeclarationConstants::GetPropertyCSSValue:
		{
			DOMString propertyName = toDOMString(exec, args[0]);
			return getDOMCSSValue(exec, obj.getPropertyCSSValue(propertyName));
		}
		case CSSStyleDeclarationConstants::RemoveProperty:
		{
			DOMString propertyName = toDOMString(exec, args[0]);
			return getDOMString(obj.removeProperty(propertyName));
		}
		case CSSStyleDeclarationConstants::GetPropertyPriority:
		{
			DOMString propertyName = toDOMString(exec, args[0]);
			return getDOMString(obj.getPropertyPriority(propertyName));
		}
		case CSSStyleDeclarationConstants::SetProperty:
		{
			DOMString propertyName = toDOMString(exec, args[0]);
			DOMString value = toDOMString(exec, args[1]);
			DOMString priority = toDOMString(exec, args[2]);
			obj.setProperty(propertyName, value, priority);
			return Undefined();
		}
		case CSSStyleDeclarationConstants::Item:
		{
			unsigned long index = args[0]->toUInt32(exec);
			return getDOMString(obj.item(index));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

CSSStyleDeclaration CSSStyleDeclaration::null;

CSSStyleDeclaration::CSSStyleDeclaration() : d(0)
{
}

CSSStyleDeclaration::CSSStyleDeclaration(const CSSStyleDeclaration &other) : d(other.d)
{
	if(d)
		d->ref();
}

CSSStyleDeclaration::CSSStyleDeclaration(CSSStyleDeclarationImpl *i) : d(i)
{
	if(d)
		d->ref();
}

KDOM_IMPL_DTOR_ASSIGN_OP(CSSStyleDeclaration)

void CSSStyleDeclaration::setCssText(const DOMString &cssText)
{
	if(d)
		d->setCssText(cssText);
}

DOMString CSSStyleDeclaration::cssText() const
{
	if(!d)
		return DOMString();

	return d->cssText();
}

DOMString CSSStyleDeclaration::getPropertyValue(const DOMString &propertyName) const
{
	if(d)
	{
		int id = d->interface()->getPropertyID(propertyName.string().ascii(), propertyName.length());
		if(id)
			return d->getPropertyValue(id);
	}

	return DOMString();
}

CSSValue CSSStyleDeclaration::getPropertyCSSValue(const DOMString &propertyName) const
{
	if(!d)
		return CSSValue::null;

	return CSSValue(d->getPropertyCSSValue(propertyName));
}

DOMString CSSStyleDeclaration::removeProperty(const DOMString &propertyName)
{
	if(!d)
		return DOMString();

	int id = d->interface()->getPropertyID(propertyName.string().ascii(), propertyName.length());
	return d->removeProperty(id);
}

DOMString CSSStyleDeclaration::getPropertyPriority(const DOMString &propertyName) const
{
	if(!d)
		return DOMString();

	int id = d->interface()->getPropertyID(propertyName.string().ascii(), propertyName.length());
	if(id && d->getPropertyPriority(id))
		return DOMString("important");

	return DOMString();
}

void CSSStyleDeclaration::setProperty(const DOMString &propertyName, const DOMString &value, const DOMString &prio)
{
	if(d)
	{
		int id = d->interface()->getPropertyID(propertyName.string().ascii(), propertyName.length());
		if(id)
		{
			bool important = false;

			QString str = prio.string();
			if(str.find(QString::fromLatin1("important"), 0, false) != -1)
				important = true;
				
			d->setProperty(id, value, important);
		}
	}
}

unsigned long CSSStyleDeclaration::length() const
{
	if(!d)
		return 0;

	return d->length();
}

DOMString CSSStyleDeclaration::item(unsigned long index) const
{
	if(!d)
		return DOMString();

	return d->item(index);
}

CSSRule CSSStyleDeclaration::parentRule() const
{
	if(!d)
		return CSSRule::null;

	return CSSRule(d->parentRule());
}

// vim:ts=4:noet
