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

#ifndef KDOM_utils_QNameImpl_H
#define KDOM_utils_QNameImpl_H

#include <kdom/Shared.h>

namespace KDOM
{
	class DOMString;

	/**
	 * QNameImpl represents a qualified name as defined in the XML 
	 * specifications: XML Schema Part2: Datatypes specification, Namespaces in XML.
	 *
	 * The value of a QNameImpl contains a Namespace URI, local part and prefix.
	 *
	 * The lexical value of a QName is the prefix and local name, 
	 * for example "xhtml:p".
	 *
	 * Equality is defined using only the Namespace URI and local part.
	 *
	 * @author Frans Englich <frans.englich@telia.com>
	 */
	class QNameImpl : public Shared
	{
	public:

		/**
		 * Constructs a QNameImpl instance with namespace URI set to @p namespaceURI,
		 * the local part to @p localPart, and a prefix of @p prefix.
		 */
		QNameImpl(const DOMString &namespaceURI, const DOMString &localPart,
				  const DOMString &prefix);

		virtual ~QNameImpl();

		/**
		 * The prefix assigned to a QNameImpl may not be valid in a different 
		 * context. For example, a QNameImpl may be assigned a prefix in the 
		 * context of parsing a document but that prefix may be invalid 
		 * in the context of a different document.
		 */
		DOMString prefix() const;

		/**
		 * Get the Namespace URI of this QNameImpl.
		 */
		DOMString namespaceURI() const;

		/**
		 * Get the local part of this QNameImpl.
		 */
		DOMString localPart() const;

		/**
		 * Checks whether this instance and @p impl are 
		 * considered equal. Two QNames are equal if their localPart()s and
		 * namespaceURI()s are equal. In other words, their
		 * prefixes may well differ.
		 *
		 * @returns true if the two objects are equal.
		 */
		bool operator==(const QNameImpl &impl) const;

	private:

		DOMString m_namespace;
		DOMString m_local;
		DOMString m_prefix;
	};
};

#endif
// vim:ts=4:noet
