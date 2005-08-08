/*
    Copyright (C) 2005 Frans Englich <frans.englich@telia.com>

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

#ifndef KDOM_XPathException_H
#define KDOM_XPathException_H

#include <kdom/ecma/Ecma.h>

namespace KDOM
{
	class XPathExceptionImpl;

	/**
	 *
	 * @author Frans Englich <frans.englich@telia.com>
	 */
	class XPathException
	{
	public:
		XPathException();
		explicit XPathException(XPathExceptionImpl *i);
		XPathException(const XPathException &other);
		virtual ~XPathException();

		// Operators
		XPathException &operator=(const XPathException &other);
		bool operator==(const XPathException &other) const;
		bool operator!=(const XPathException &other) const;

		// 'XPathException' functions
		unsigned short code() const;

		// Internal
		KDOM_INTERNAL_BASE(XPathException)

	protected:
		XPathExceptionImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

#endif

// vim:ts=4:noet
