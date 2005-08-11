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

#include "DocumentRange.h"
#include "DocumentRangeImpl.h"
#include "Ecma.h"
#include "RangeImpl.h"
#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "RangeConstants.h"
#include "DocumentRange.lut.h"
using namespace KDOM;

/*
@begin DocumentRange::s_hashTable 2
 dummy				DocumentRangeConstants::Dummy				DontDelete|ReadOnly
@end
@begin DocumentRangeProto::s_hashTable 2
 createRange		DocumentRangeConstants::CreateRange			DontDelete|Function 4
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("DocumentRange", DocumentRangeProto, DocumentRangeProtoFunc)

KJS::ValueImp *DocumentRange::getValueProperty(KJS::ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return KJS::Undefined();
}

KJS::ValueImp *DocumentRangeProtoFunc::callAsFunction(KJS::ExecState *exec, KJS::ObjectImp *thisObj, const KJS::List &)
{
	DocumentRange obj(cast(exec, thisObj));
	Q_ASSERT(obj.d != 0);

	KDOM_ENTER_SAFE

	switch(id)
	{
		case DocumentRangeConstants::CreateRange:
			return safe_cache<Range>(exec, obj.createRange());
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(DOMException)
	return KJS::Undefined();
}

DocumentRange DocumentRange::null;

DocumentRange::DocumentRange() : d(0)
{
}

DocumentRange::DocumentRange(DocumentRangeImpl *i) : d(i)
{
}

DocumentRange::DocumentRange(const DocumentRange &other) : d(0)
{
	(*this) = other;
}

DocumentRange::~DocumentRange()
{
}

DocumentRange &DocumentRange::operator=(const DocumentRange &other)
{
	if(d != other.d)
		d = other.d;

	return *this;
}

DocumentRange &DocumentRange::operator=(DocumentRangeImpl *other)
{
	if(d != other)
		d = other;

	return *this;
}

Range DocumentRange::createRange() const
{
	if(!d)
		return Range::null;

	return Range(d->createRange());
}

// vim:ts=4:noet
