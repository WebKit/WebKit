/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>
				  2005 Frans Englich <frans.englich@telia.com>

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

#include "DOMError.h"
#include "DOMErrorHandler.h"
#include "DOMException.h"
#include "Ecma.h"

#include "DOMErrorHandlerImpl.h"
#include "DOMExceptionImpl.h"

#include "DOMConstants.h"
#include "DOMErrorHandler.lut.h"

using namespace KDOM;
using namespace KJS;

/*
@begin DOMErrorHandler::s_hashTable 2
 dummy			DOMErrorHandlerConstants::Dummy						DontDelete|ReadOnly
@end
@begin DOMErrorHandlerProto::s_hashTable 2
 handleError	DOMErrorHandlerConstants::HandleError				DontDelete|Function 1
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("DOMErrorHandler", DOMErrorHandlerProto, DOMErrorHandlerProtoFunc)

ValueImp *DOMErrorHandler::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}
	
	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

ValueImp *DOMErrorHandlerProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(DOMErrorHandler)
	KDOM_ENTER_SAFE
	
	switch(id)
	{
			case DOMErrorHandlerConstants::HandleError:
		{
			DOMError error = ecma_cast<DOMError>(exec, args[0], &toDOMError);
			return KJS::Boolean(obj.handleError(error));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}
	
	KDOM_LEAVE_CALL_SAFE(DOMException)
	return Undefined();
}

DOMErrorHandler DOMErrorHandler::null;

DOMErrorHandler::DOMErrorHandler() : d(0)
{
}

DOMErrorHandler::DOMErrorHandler(DOMErrorHandlerImpl *i) : d(i)
{
	if(d)
		d->ref();
}

DOMErrorHandler::DOMErrorHandler(const DOMErrorHandler &other) : d(0)
{
	(*this) = other;
}

bool DOMErrorHandler::handleError(const DOMError &) const
{
	return false;
}

KDOM_IMPL_DTOR_ASSIGN_OP(DOMErrorHandler)

// vim:ts=4:noet
