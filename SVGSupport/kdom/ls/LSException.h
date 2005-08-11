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

#ifndef KDOM_LSException_H
#define KDOM_LSException_H

#include <kdom/ecma/Ecma.h>

namespace KDOM
{
	class LSExceptionImpl;

	/**
	 * @short Parser or write operations may throw an LSException if the processing
	 * is stopped. The processing can be stopped due to a DOMError with a severity
	 * of DOMError.SEVERITY_FATAL_ERROR or a non recovered DOMError.SEVERITY_ERROR,
	 * or if DOMErrorHandler.handleError() returned false.
	 *
	 * @author Rob Buis <buis@kde.org>
	 */
	class LSException
	{
	public:
		LSException();
		explicit LSException(LSExceptionImpl *i);
		LSException(const LSException &other);
		virtual ~LSException();

		// Operators
		LSException &operator=(const LSException &other);
		bool operator==(const LSException &other) const;
		bool operator!=(const LSException &other) const;

		// 'LSException' functions
		unsigned short code() const;

		// Internal
		KDOM_INTERNAL_BASE(LSException)

	protected:
		LSExceptionImpl *d;

	public: // EcmaScript section
		KDOM_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

#endif

// vim:ts=4:noet
