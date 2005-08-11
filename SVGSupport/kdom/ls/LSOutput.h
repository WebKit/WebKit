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

#ifndef KDOM_LSOutput_H
#define KDOM_LSOutput_H

#include <kdom/ecma/DOMLookup.h>

namespace KDOM
{
	class DOMString;
	class LSOutputImpl;

	/**
	 * @short This interface represents an output destination for data.
	 *
	 * This interface allows an application to encapsulate information about an
	 * output destination in a single object, which may include a URI, a byte
	 * stream (possibly with a specified encoding), a base URI, and/or a
	 * character stream.
	 *
	 * @author Rob Buis <buis@kde.org>
	 */
	class LSOutput
	{
	public:
		LSOutput();
		explicit LSOutput(LSOutputImpl *impl);
		LSOutput(const LSOutput &other);
		virtual ~LSOutput();

		// Operators
		LSOutput &operator=(const LSOutput &other);
		bool operator==(const LSOutput &other) const;
		bool operator!=(const LSOutput &other) const;

		// 'LSOutput' functions
		/**
		 * The system identifier, a URI reference [IETF RFC 2396], for this
		 * output destination.  If the system ID is a relative URI reference
		 * (see section 5 in [IETF RFC 2396]), the behavior is implementation
		 * dependent. 
		 */
		DOMString systemId() const;
		void setSystemId(const DOMString &systemId);

		/**
		 * The character encoding to use for the output. The encoding must be
		 * a string acceptable for an XML encoding declaration ([XML 1.0]
		 * section 4.3.3 "Character Encoding in Entities"), it is recommended
		 * that character encodings registered (as charsets) with the Internet
		 * Assigned Numbers Authority [IANA-CHARSETS] should be referred to
		 * using their registered names. 
		 */
		DOMString encoding() const;
		void setEncoding(const DOMString &encoding);

		// Internal
		KDOM_INTERNAL_BASE(LSOutput)

	private:
		LSOutputImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET
		KDOM_PUT
		KDOM_CAST

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, KJS::ValueImp *value, int attr);
	};

	KDOM_DEFINE_CAST(LSOutput)
};

#endif

// vim:ts=4:noet
