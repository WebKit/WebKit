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
#include "DOMError.h"
#include "DOMErrorImpl.h"
#include "DOMLocator.h"
#include "DOMException.h"
#include "DOMExceptionImpl.h"
#include "DOMObject.h"

#include "DOMConstants.h"
#include "DOMError.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin DOMError::s_hashTable 7
 severity			DOMErrorConstants::Severity			DontDelete|ReadOnly
 message			DOMErrorConstants::Message			DontDelete|ReadOnly
 type				DOMErrorConstants::Type				DontDelete|ReadOnly
 relatedException	DOMErrorConstants::RelatedException	DontDelete|ReadOnly
 relatedData		DOMErrorConstants::RelatedData		DontDelete|ReadOnly
 location			DOMErrorConstants::Location			DontDelete|ReadOnly
@end
*/

ValueImp *DOMError::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case DOMErrorConstants::Severity:
			return Number(severity());
		case DOMErrorConstants::Message:
			return getDOMString(message());
		case DOMErrorConstants::Type:
			return getDOMString(type());
		/*case DOMErrorConstants::RelatedException:
			return Number(relatedException());*/
		case DOMErrorConstants::RelatedData:
		{
			DOMObject object = relatedData();
			NodeImpl *imp = static_cast<NodeImpl *>(object.handle());
			if(imp)
				return Node(imp).cache(exec);

			return Undefined();
		}
		case DOMErrorConstants::Location:
			return safe_cache<DOMLocator>(exec, location());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

DOMError DOMError::null;

DOMError::DOMError() : d(0)
{
}

DOMError::DOMError(DOMErrorImpl *i) : d(i)
{
	if(d)
		d->ref();
}

DOMError::DOMError(const DOMError &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(DOMError)

unsigned short DOMError::severity() const
{
	if(!d)
		return 0;

	return d->severity();
}

DOMString DOMError::message() const
{
	if(!d)
		return DOMString();

	return DOMString(d->message());
}

DOMString DOMError::type() const
{
	if(!d)
		return DOMString();

	return DOMString(d->type());
}

//DOMObject DOMError::relatedException() const
//{
//}

DOMObject DOMError::relatedData() const
{
	if(!d)
		return DOMObject::null;

	return DOMObject(d->relatedData());
}

DOMLocator DOMError::location() const
{
	if(!d)
		return DOMLocator::null;

	return DOMLocator(d->location());
}

// vim:ts=4:noet
