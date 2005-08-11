/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>
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

#ifndef KDOM_DOMErrorHandler_H
#define KDOM_DOMErrorHandler_H

#include <kdom/ecma/DOMLookup.h>

namespace KDOM
{
	class DOMError;
	class DOMErrorImpl;
	class DOMErrorHandlerImpl;

	class DOMErrorHandler
	{
	public:
		DOMErrorHandler();
		explicit DOMErrorHandler(DOMErrorHandlerImpl *i);
		DOMErrorHandler(const DOMErrorHandler &other);
		virtual ~DOMErrorHandler();

		// Operators
		DOMErrorHandler &operator=(const DOMErrorHandler &other);
		bool operator==(const DOMErrorHandler &other) const;
		bool operator!=(const DOMErrorHandler &other) const;

		bool handleError(const DOMError &error) const;

		KDOM_INTERNAL_BASE(DOMErrorHandler)

	protected:
		DOMErrorHandlerImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(DOMErrorHandlerProto)
KDOM_IMPLEMENT_PROTOFUNC(DOMErrorHandlerProtoFunc, DOMErrorHandler)

#endif

// vim:ts=4:noet
