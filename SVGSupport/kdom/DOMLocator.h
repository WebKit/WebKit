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

#ifndef KDOM_DOMLocator_H
#define KDOM_DOMLocator_H

#include <kdom/ecma/DOMLookup.h>

namespace KDOM
{
	class DOMString;
	class Node;
	class DOMLocatorImpl;

	/**
	 * @short DOMLocator is an interface that describes a location (e.g. where
	 * an error occurred).
	 *
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 * @author Rob Buis <buis@kde.org>
	 */
	// Introduced in DOM Level 3:
	class DOMLocator
	{
	public:
		DOMLocator();
		explicit DOMLocator(DOMLocatorImpl *i);
		DOMLocator(const DOMLocator &other);
		virtual ~DOMLocator();

		// Operators
		DOMLocator &operator=(const DOMLocator &other);
		bool operator==(const DOMLocator &other) const;
		bool operator!=(const DOMLocator &other) const;

		// 'DOMLocator' functions
		/**
		 * The line number this locator is pointing to, or -1 if there is no
		 * column number available.
		 */
		long lineNumber() const;

		/**
		 * The column number this locator is pointing to, or -1 if there is no
		 * column number available.
		 */
		long columnNumber() const;

		/**
		 * The byte offset into the input source this locator is pointing to or
		 * -1 if there is no byte offset available.
		 */
		long byteOffset() const;

		/**
		 * The UTF-16, as defined in [Unicode] and Amendment 1 of [ISO/IEC 10646],
		 * offset into the input source this locator is pointing to or -1 if
		 * there is no UTF-16 offset available.
		 */
		long utf16Offset() const;

		/**
		 * The node this locator is pointing to, or null if no node is available.
		 */
		Node relatedNode() const;

		/**
		 * The URI this locator is pointing to, or null if no URI is available.
		 */
		DOMString uri() const;

		// Internal
		KDOM_INTERNAL_BASE(DOMLocator)

	protected:
		DOMLocatorImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

#endif

// vim:ts=4:noet
