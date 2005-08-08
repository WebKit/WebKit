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

#ifndef KDOM_DOMError_H
#define KDOM_DOMError_H

#include <kdom/ecma/DOMLookup.h>

namespace KDOM
{
	enum ErrorSeverity
	{
		SEVERITY_WARNING = 1,
		SEVERITY_ERROR = 2,
		SEVERITY_FATAL_ERROR = 3,
	};

	class DOMString;
	class DOMLocator;
	class DOMErrorImpl;
	class DOMObject;

	/**
	 * @short DOMError is an interface that describes an error.
	 *
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 * @author Rob Buis <buis@kde.org>
	 */
	// Introduced in DOM Level 3:
	class DOMError
	{
	public:
		DOMError();
		explicit DOMError(DOMErrorImpl *i);
		DOMError(const DOMError &other);
		virtual ~DOMError();

		// Operators
		DOMError &operator=(const DOMError &other);
		bool operator==(const DOMError &other) const;
		bool operator!=(const DOMError &other) const;

		// 'DOMError' functions
		/**
		 * The severity of the error, either SEVERITY_WARNING,
		 * SEVERITY_ERROR, or SEVERITY_FATAL_ERROR.
		 */
		unsigned short severity() const;

		/**
		 * An implementation specific string describing the error
		 * that occurred.
		 */
		DOMString message() const;

		/**
		 * A DOMString indicating which related data is expected in relatedData.
		 * Users should refer to the specification of the error in order to
		 * find its DOMString type and relatedData definitions if any.
		 */
		DOMString type() const;

		/**
		 * The related platform dependent exception if any.
		 */
		//DOMObject relatedException() const;

		/**
		 * The related DOMError.type dependent data if any.
		 */
		DOMObject relatedData() const;

		/**
		 * The location of the error.
		 */
		DOMLocator location() const;

		// Internal
		KDOM_INTERNAL_BASE(DOMError)

		KDOM_CAST

	protected:
		DOMErrorImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};

	KDOM_DEFINE_CAST(DOMError)
};

#endif

// vim:ts=4:noet
