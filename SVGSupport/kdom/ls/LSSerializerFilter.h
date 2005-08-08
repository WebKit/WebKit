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

#ifndef KDOM_LSSerializerFilter_H
#define KDOM_LSSerializerFilter_H

#include <kdom/ecma/DOMLookup.h>
#include <kdom/traversal/NodeFilter.h>

namespace KDOM
{
	class LSSerializerFilterImpl;

	/**
	 * @short LSSerializerFilters provide applications the ability to
	 * examine nodes as they are being serialized and decide what nodes
	 * should be serialized or not. The LSSerializerFilter interface is
	 * based on the NodeFilter interface defined in [DOM Level 2 Traversal
	 * and Range].
	 *
	 * @author Rob Buis <buis@kde.org>
	 */
	class LSSerializerFilter : public NodeFilter
	{
	public:
		LSSerializerFilter();
		explicit LSSerializerFilter(LSSerializerFilterImpl *impl);
		LSSerializerFilter(const LSSerializerFilter &other);
		virtual ~LSSerializerFilter();

		// Operators
		LSSerializerFilter &operator=(const LSSerializerFilter &other);

		// 'LSSerializerFilter' functions
		/**
		 * Tells the LSSerializer what types of nodes to show to the filter.
		 * If a node is not shown to the filter using this attribute, it is
		 * automatically serialized. See NodeFilter for definition of the
		 * constants. The constants SHOW_DOCUMENT, SHOW_DOCUMENT_TYPE,
		 * SHOW_DOCUMENT_FRAGMENT, SHOW_NOTATION, and SHOW_ENTITY are
		 * meaningless here, such nodes will never be passed to a
		 * LSSerializerFilter. Unlike [DOM Level 2 Traversal and Range],
		 * the SHOW_ATTRIBUTE constant indicates that the Attr nodes are
		 * shown and passed to the filter.
		 * The constants used here are defined in [DOM Level 2 Traversal
		 * and Range]. 
		 */
		unsigned long whatToShow() const;

		// Internal
		KDOM_INTERNAL(LSSerializerFilter)

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

#endif

// vim:ts=4:noet
