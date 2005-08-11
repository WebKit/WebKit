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

#ifndef KDOM_H
#define KDOM_H

#include <kdebug.h>

#include <qstring.h>

#include <kdom/DOMString.h>

/**
 * @short General namespace specific definitions.
 */
namespace KDOM
{
	/**
	 * All DOM constants
	 */
	enum NodeType
	{
		ELEMENT_NODE = 1,
		ATTRIBUTE_NODE = 2,
		TEXT_NODE = 3,
		CDATA_SECTION_NODE = 4,
		ENTITY_REFERENCE_NODE = 5,
		ENTITY_NODE = 6,
		PROCESSING_INSTRUCTION_NODE = 7,
		COMMENT_NODE = 8,
		DOCUMENT_NODE = 9,
		DOCUMENT_TYPE_NODE = 10,
		DOCUMENT_FRAGMENT_NODE = 11,
		NOTATION_NODE = 12
	};

	enum ExceptionCode
	{
		INDEX_SIZE_ERR = 1,
		DOMSTRING_SIZE_ERR = 2,
		HIERARCHY_REQUEST_ERR = 3,
		WRONG_DOCUMENT_ERR = 4,
		INVALID_CHARACTER_ERR = 5,
		NO_DATA_ALLOWED_ERR = 6,
		NO_MODIFICATION_ALLOWED_ERR = 7,
		NOT_FOUND_ERR = 8,
		NOT_SUPPORTED_ERR = 9,
		INUSE_ATTRIBUTE_ERR = 10,
		INVALID_STATE_ERR = 11,
		SYNTAX_ERR = 12,
		INVALID_MODIFICATION_ERR = 13,
		NAMESPACE_ERR = 14,
		INVALID_ACCESS_ERR = 15,
		VALIDATION_ERR = 16,	// DOM3
		TYPE_MISMATCH_ERR = 17	// DOM3
	};

	enum DocumentPosition
	{
		DOCUMENT_POSITION_DISCONNECTED				= 0x01,
		DOCUMENT_POSITION_PRECEDING					= 0x02,
		DOCUMENT_POSITION_FOLLOWING					= 0x04,
		DOCUMENT_POSITION_CONTAINS					= 0x08,
		DOCUMENT_POSITION_CONTAINED_BY				= 0x10,
		DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC	= 0x20
	};

	enum DerivationMethods
	{
		DERIVATION_RESTRICTION         = 0x00000001,
		DERIVATION_EXTENSION           = 0x00000002,
		DERIVATION_UNION               = 0x00000004,
		DERIVATION_LIST                = 0x00000008
	};
	
	/**
	 * A TimeStamp represents a number of milliseconds
	 */
	typedef unsigned long long DOMTimeStamp;

	/**
	 * A macro expanding to an expression which returns true if @p
	 * n is of type element.
	 *
	 * @param n the node to check
	 */
	#define isElementNode(n) (n->nodeType() == KDOM::ELEMENT_NODE)
	#define isDocumentNode(n) (n->nodeType() == KDOM::DOCUMENT_NODE)
	#define isTextNode(n) (n->nodeType() == KDOM::TEXT_NODE)
	#define isImplicitNode(n) (false) // TODO: check that when khtml2 is ready

	// Debugging helper
#ifndef APPLE_CHANGES
	inline kndbgstream &operator<<(kndbgstream &stream, const DOMString &string) { return (stream << string.string()); }
#endif
	inline kdbgstream  &operator<<(kdbgstream  &stream, const DOMString &string) { return (stream << string.string()); }
};

#endif

// vim:ts=4:noet
