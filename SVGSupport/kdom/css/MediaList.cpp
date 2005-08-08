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
#include "DOMString.h"
#include "MediaList.h"
#include "DOMException.h"
#include "MediaListImpl.h"
#include "DOMExceptionImpl.h"

#include "kdom/data/CSSConstants.h"
#include "MediaList.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin MediaList::s_hashTable 3
 mediaText		MediaListConstants::MediaText         DontDelete
 length			MediaListConstants::Length            DontDelete|ReadOnly
@end
@begin MediaListProto::s_hashTable 5
 item			MediaListConstants::Item              DontDelete|Function 1
 deleteMedium	MediaListConstants::DeleteMedium      DontDelete|Function 1
 appendMedium	MediaListConstants::AppendMedium      DontDelete|Function 1
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("MediaList", MediaListProto, MediaListProtoFunc)

ValueImp *MediaList::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
		case MediaListConstants::MediaText:
			return getDOMString(mediaText());
		case MediaListConstants::Length:
			return Number(length());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
};

void MediaList::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case MediaListConstants::MediaText:
			setMediaText(toDOMString(exec, value));
			break;
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
}

ValueImp *MediaListProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(MediaList)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case MediaListConstants::Item:
		{
			unsigned long index = args[0]->toUInt32(exec);
			return getDOMString(obj.item(index));
		}
		case MediaListConstants::DeleteMedium:
		{
			DOMString oldMedium = toDOMString(exec, args[0]);
			obj.deleteMedium(oldMedium);
			return Undefined();
		}
		case MediaListConstants::AppendMedium:
		{
			DOMString newMedium = toDOMString(exec, args[0]);
			obj.appendMedium(newMedium);
			return Undefined();
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
			break;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

MediaList MediaList::null;

MediaList::MediaList() : d(0)
{
}

MediaList::MediaList(const MediaList &other) : d(other.d)
{
	if(d)
		d->ref();
}

MediaList::MediaList(MediaListImpl *i) : d(i)
{
	if(d)
		d->ref();
}

KDOM_IMPL_DTOR_ASSIGN_OP(MediaList)

void MediaList::setMediaText(const DOMString &mediatext)
{
	if(d)
		return d->setMediaText(mediatext);
}

DOMString MediaList::mediaText() const
{
	if(!d)
		return DOMString();

	return d->mediaText();
}

unsigned long MediaList::length() const
{
	if(!d)
		return 0;

	return d->length();
}

DOMString MediaList::item(unsigned long index) const
{
	if(!d)
		return DOMString();

	return d->item(index);
}

void MediaList::deleteMedium(const DOMString &oldMedium)
{
	if(d)
		d->deleteMedium(oldMedium);
}

void MediaList::appendMedium(const DOMString &newMedium)
{
	if(d)
		d->appendMedium(newMedium);
}

// vim:ts=4:noet
