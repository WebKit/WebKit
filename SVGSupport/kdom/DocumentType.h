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

#ifndef KDOM_DocumentType_H
#define KDOM_DocumentType_H

#include <kdom/Node.h>

namespace KDOM
{
	class DOMString;
	class NamedNodeMap;
	class DocumentTypeImpl;

	/**
	 * @short Each Document has a doctype attribute whose value
	 * is either null or a DocumentType object. The DocumentType
	 * interface in the DOM Core provides an interface to the list
	 * of entities that are defined for the document, and little
	 * else because the effect of namespaces and the various XML
	 * schema efforts on DTD representation are not clearly
	 * understood as of this writing.
	 *
	 * DOM Level 3 doesn't support editing DocumentType nodes.
	 * DocumentType nodes are read-only.
	 *
	 * @author Rob Buis <buis@kde.org>
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 */
	class DocumentType : public Node 
	{
	public:
		DocumentType();
		explicit DocumentType(DocumentTypeImpl *i);
		DocumentType(const DocumentType &other);
		DocumentType(const Node &other);
		virtual ~DocumentType();

		// Operators
		DocumentType &operator=(const DocumentType &other);
		DocumentType &operator=(const Node &other);

		// 'DocumentType' functions
		/**
		 * The name of DTD; i.e., the name immediately following the
		 * DOCTYPE keyword.
		 */
		DOMString name() const;

		/**
		 * A NamedNodeMap containing the general entities, both
		 * external and internal, declared in the DTD. Parameter
		 * entities are not contained. Duplicates are discarded.
		 * Every node in this map also implements the Entity interface.
		 *
		 * The DOM Level 2 does not support editing entities,
		 * therefore entities cannot be altered in any way.
		 */
		NamedNodeMap entities() const;
	
		/**
		 * A NamedNodeMap containing the notations declared in the DTD.
		 * Duplicates are discarded. Every node in this map also implements
		 * the Notation interface.
		 * The DOM Level 2 does not support editing notations, therefore
		 * notations cannot be altered in any way.
		 */
		NamedNodeMap notations() const;

		/**
		 * The public identifier of the external subset.
		 */
		DOMString publicId() const;

		/**
		 * The system identifier of the external subset.
		 * This may be an absolute URI or not.
		 */
		DOMString systemId() const;

		/**
		 * The internal subset as a string, or null if there is none.
		 * This internal subset does not contain the delimiting
		 * square brackets.
		 *
		 * Note: The actual content returned depends on how much
		 * information is available to the implementation. This may
		 * vary depending on various parameters, including the XML
		 * processor used to build the document.
		 */
		DOMString internalSubset() const;

		// Internal
		KDOM_INTERNAL(DocumentType)

	public: // EcmaScript section
		KDOM_GET
		KDOM_FORWARDPUT
		KDOM_CAST

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};

	KDOM_DEFINE_CAST(DocumentType)
};

#endif

// vim:ts=4:noet
