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

#include "Node.h"
#include "Ecma.h"
#include "MediaList.h"
#include "StyleSheet.h"
#include "StyleSheetImpl.h"

#include "CSSConstants.h"
#include "StyleSheet.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin StyleSheet::s_hashTable 9
 type				StyleSheetConstants::Type				DontDelete|ReadOnly
 disabled			StyleSheetConstants::Disabled			DontDelete
 ownerNode			StyleSheetConstants::OwnerNode			DontDelete|ReadOnly
 parentStyleSheet	StyleSheetConstants::ParentStyleSheet	DontDelete|ReadOnly
 href				StyleSheetConstants::Href				DontDelete|ReadOnly
 title				StyleSheetConstants::Title				DontDelete|ReadOnly
 media				StyleSheetConstants::Media				DontDelete|ReadOnly
@end
*/

Value StyleSheet::getValueProperty(ExecState *exec, int token) const
{
	switch(token)
	{
		case StyleSheetConstants::Type:
			return getDOMString(type());
		case StyleSheetConstants::Disabled:
			return KJS::Boolean(disabled());
		case StyleSheetConstants::OwnerNode:
			return getDOMNode(exec, ownerNode());
		case StyleSheetConstants::ParentStyleSheet:
			return safe_cache<StyleSheet>(exec, parentStyleSheet());
		case StyleSheetConstants::Href:
			return getDOMString(href());
		case StyleSheetConstants::Title:
			return getDOMString(title());
		case StyleSheetConstants::Media:
			return safe_cache<MediaList>(exec, media());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
};

void StyleSheet::putValueProperty(ExecState *exec, int token, const Value &value, int)
{
	switch(token)
	{
		case StyleSheetConstants::Disabled:
			setDisabled(value.toBoolean(exec));
			break;
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}
}

StyleSheet StyleSheet::null;

StyleSheet::StyleSheet() : d(0)
{
}

StyleSheet::StyleSheet(StyleSheetImpl *i) : d(i)
{
	if(d)
		d->ref();
}

StyleSheet::StyleSheet(const StyleSheet &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(StyleSheet)

DOMString StyleSheet::type() const
{
	return DOMString();
}

void StyleSheet::setDisabled(bool disabled)
{
	if(d)
		d->setDisabled(disabled);
}

bool StyleSheet::disabled() const
{
	if(!d)
		return false;

	return d->disabled();
}

Node StyleSheet::ownerNode() const
{
	if(!d)
		return Node::null;

	return d->ownerNode();
}

StyleSheet StyleSheet::parentStyleSheet() const
{
	if(!d)
		return StyleSheet::null;

	return StyleSheet(d->parentStyleSheet());
}

DOMString StyleSheet::href() const
{
	if(!d)
		return DOMString();

	return d->href();
}

DOMString StyleSheet::title() const
{
	if(!d)
		return DOMString();
	
	return d->title();
}

MediaList StyleSheet::media() const
{
	if(!d)
		return MediaList::null;

	return MediaList(d->media());
}

bool StyleSheet::isCSSStyleSheet() const
{
	if(!d)
		return false;

	return d->isCSSStyleSheet();
}

// vim:ts=4:noet
