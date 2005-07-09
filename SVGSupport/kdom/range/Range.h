/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Gunnstein Lye (gunnstein@netcom.no)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * (C) 2001 Peter Kelly (pmk@post.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * This file includes excerpts from the Document Object Model (DOM)
 * Level 2 Specification (Candidate Recommendation)
 * http://www.w3.org/TR/2000/CR-DOM-Level-2-20000510/
 * Copyright © 2000 W3C® (MIT, INRIA, Keio), All Rights Reserved.
 *
 */
 
#ifndef KDOM_Range_H
#define KDOM_Range_H

#include <kdom/ecma/DOMLookup.h>

namespace KDOM
{
	class Node;
	class DOMString;
	class RangeImpl;
	class Document;
	class DocumentImpl;
	class DocumentFragment;
	
	class Range
	{
	friend class Document;
	friend class RangeImpl;
	friend class DocumentImpl;
	public:
		Range();
		explicit Range(RangeImpl *i);
		Range(const Range &other);
		Range(const Document &rootContainer);
		Range(const Node &startContainer, const long startOffset,
			  const Node &endContainer, const long endOffset);
		virtual ~Range();
		
		// Operators
		Range &operator=(const Range &other);
		bool operator==(const Range &other) const;
		bool operator!=(const Range &other) const;

		/**
		 * Node within which the range begins
		 *
		 */
		Node startContainer() const;

		/**
		 * Offset within the starting node of the range.
		 *
		 */
		long startOffset() const;

		/**
		 * Node within which the range ends
		 *
		 */
		Node endContainer() const;

		/**
		 * Offset within the ending node of the range.
		 *
		 */
		long endOffset() const;

		/**
		 * true if the range is collapsed
		 *
		 */
		bool collapsed() const;

		/**
		 * Gets the common ancestor container of the range's two end-points.
		 * Also sets it.
		 *
		 */
		Node commonAncestorContainer() const;

		/**
		 * Sets the attributes describing the start of the range.
		 *
		 * @param refNode The \c refNode value. This parameter
		 * must be different from \c null .
		 *
		 * @param offset The \c startOffset value.
		 *
		 * @return
		 *
		 * @exception RangeException
		 * NULL_NODE_ERR: Raised if \c refNode is \c null .
		 *
		 *  INVALID_NODE_TYPE_ERR: Raised if \c refNode or an
		 * ancestor of \c refNode is an Attr, Entity,
		 * Notation, or DocumentType node.
		 *
		 *  If an offset is out-of-bounds, should it just be fixed up or
		 * should an exception be raised.
		 *
		 */
		void setStart(const Node &refNode, long offset);

		/**
		 * Sets the attributes describing the end of a range.
		 *
		 * @param refNode The \c refNode value. This parameter
		 * must be different from \c null .
		 *
		 * @param offset The \c endOffset value.
		 *
		 * @return
		 *
		 * @exception RangeException
		 * NULL_NODE_ERR: Raised if \c refNode is \c null .
		 *
		 *  INVALID_NODE_TYPE_ERR: Raised if \c refNode or an
		 * ancestor of \c refNode is an Attr, Entity,
		 * Notation, or DocumentType node.
		 *
		 */
		void setEnd(const Node &refNode, long offset);

		/**
		 * Sets the start position to be before a node
		 *
		 * @param refNode Range starts before \c refNode
		 *
		 * @return
		 *
		 * @exception RangeException
		 * INVALID_NODE_TYPE_ERR: Raised if an ancestor of \c refNode
		 * is an Attr, Entity, Notation, or DocumentType node or
		 * if \c refNode is a Document, DocumentFragment,
		 * Attr, Entity, or Notation node.
		 *
		 */
		void setStartBefore(const Node &refNode);

		/**
		 * Sets the start position to be after a node
		 *
		 * @param refNode Range starts after \c refNode
		 *
		 * @return
		 *
		 * @exception RangeException
		 * INVALID_NODE_TYPE_ERR: Raised if an ancestor of \c refNode
		 * is an Attr, Entity, Notation, or DocumentType node or
		 * if \c refNode is a Document, DocumentFragment,
		 * Attr, Entity, or Notation node.
		 *
		 */
		void setStartAfter(const Node &refNode);

		/**
		 * Sets the end position to be before a node.
		 *
		 * @param refNode Range ends before \c refNode
		 *
		 * @return
		 *
		 * @exception RangeException
		 * INVALID_NODE_TYPE_ERR: Raised if an ancestor of \c refNode
		 * is an Attr, Entity, Notation, or DocumentType node or
		 * if \c refNode is a Document, DocumentFragment,
		 * Attr, Entity, or Notation node.
		 *
		 */
		void setEndBefore(const Node &refNode);

		/**
		 * Sets the end of a range to be after a node
		 *
		 * @param refNode Range ends after \c refNode .
		 *
		 * @return
		 *
		 * @exception RangeException
		 * INVALID_NODE_TYPE_ERR: Raised if an ancestor of \c refNode
		 * is an Attr, Entity, Notation or DocumentType node or if
		 * \c refNode is a Document, DocumentFragment, Attr,
		 * Entity, or Notation node.
		 *
		 */
		void setEndAfter(const Node &refNode);

		/**
		 * Collapse a range onto one of its end-points
		 *
		 * @param toStart If true, collapses the Range onto its start; if
		 * false, collapses it onto its end.
		 *
		 * @return
		 *
		 */
		void collapse(bool toStart);

		/**
		 * Select a node and its contents
		 *
		 * @param refNode The node to select.
		 *
		 * @return
		 *
		 * @exception RangeException
		 * INVALID_NODE_TYPE_ERR: Raised if an ancestor of \c refNode
		 * is an Attr, Entity, Notation or DocumentType node or if
		 * \c refNode is a Document, DocumentFragment, Attr,
		 * Entity, or Notation node.
		 *
		 */
		void selectNode(const Node &refNode);

		/**
		 * Select the contents within a node
		 *
		 * @param refNode Node to select from
		 *
		 * @return
		 *
		 * @exception RangeException
		 * INVALID_NODE_TYPE_ERR: Raised if \c refNode or an
		 * ancestor of \c refNode is an Attr, Entity, Notation
		 * or DocumentType node.
		 *
		 */
		void selectNodeContents(const Node &refNode);
	
		/**
		 * Compare the end-points of two ranges in a document.
		 *
		 * @param how
		 *
		 * @param sourceRange
		 *
		 * @return -1, 0 or 1 depending on whether the corresponding
		 * end-point of the Range is before, equal to, or after the
		 * corresponding end-point of \c sourceRange .
		 *
		 * @exception DOMException
		 * WRONG_DOCUMENT_ERR: Raised if the two Ranges are not in the
		 * same document or document fragment.
		 *
		 */
		short compareBoundaryPoints(unsigned short how, const Range &sourceRange);

		/**
		 * @internal
		 * not part of the DOM
		 *
		 * Compare the boundary-points of a range.
		 *
		 * Return true if the startContainer is before the endContainer,
		 * or if they are equal.
		 * Return false if the startContainer is after the endContainer.
		 *
		 */
		bool boundaryPointsValid();
		
		/**
		 * Removes the contents of a range from the containing document or
		 * document fragment without returning a reference to the removed
		 * content.
		 *
		 * @return
		 *
		 * @exception DOMException
		 * NO_MODIFICATION_ALLOWED_ERR: Raised if any portion of the
		 * content of the range is read-only or any of the nodes that
		 * contain any of the content of the range are read-only.
		 *
		 */
		void deleteContents();

		/**
		 * Moves the contents of a range from the containing document or
		 * document fragment to a new DocumentFragment.
		 *
		 * @return A DocumentFragment containing the extracted contents.
		 *
		 * @exception DOMException
		 * NO_MODIFICATION_ALLOWED_ERR: Raised if any portion of the
		 * content of the range is read-only or any of the nodes which
		 * contain any of the content of the range are read-only.
		 *
		 *  HIERARCHY_REQUEST_ERR: Raised if a DocumentType node would be
		 * extracted into the new DocumentFragment.
		 *
		 */
		DocumentFragment extractContents();

		/**
		 * Duplicates the contents of a range
		 *
		 * @return A DocumentFragment containing contents equivalent to
		 * those of this range.
		 *
		 * @exception DOMException
		 * HIERARCHY_REQUEST_ERR: Raised if a DocumentType node would be
		 * extracted into the new DocumentFragment.
		 *
		 */
		DocumentFragment cloneContents();

		/**
		 * Inserts a node into the document or document fragment at the
		 * start of the range.
		 *
		 * @param newNode The node to insert at the start of the range
		 *
		 * @return
		 *
		 * @exception DOMException
		 * NO_MODIFICATION_ALLOWED_ERR: Raised if an ancestor container of
		 * the start of the range is read-only.
		 *
		 *  WRONG_DOCUMENT_ERR: Raised if \c newNode and the
		 * container of the start of the Range were not created from the
		 * same document.
		 *
		 *  HIERARCHY_REQUEST_ERR: Raised if the container of the start of
		 * the Range is of a type that does not allow children of the type
		 * of \c newNode or if \c newNode is an
		 * ancestor of the container .
		 *
		 * @exception RangeException
		 * INVALID_NODE_TYPE_ERR: Raised if \c node is an
		 * Attr, Entity, Notation, DocumentFragment, or Document node.
		 *
		 */
		void insertNode(const Node &newNode);

		/**
		 * Reparents the contents of the range to the given node and
		 * inserts the node at the position of the start of the range.
		 *
		 * @param newParent The node to surround the contents with.
		 *
		 * @return
		 *
		 * @exception DOMException
		 * NO_MODIFICATION_ALLOWED_ERR: Raised if an ancestor container of
		 * either end-point of the range is read-only.
		 *
		 *  WRONG_DOCUMENT_ERR: Raised if \c newParent and the
		 * container of the start of the Range were not created from the
		 * same document.
		 *
		 *  HIERARCHY_REQUEST_ERR: Raised if the container of the start of
		 * the Range is of a type that does not allow children of the type
		 * of \c newParent or if \c newParent is
		 * an ancestor of the container or if \c node would
		 * end up with a child node of a type not allowed by the type of
		 * \c node .
		 *
		 * @exception RangeException
		 * BAD_ENDPOINTS_ERR: Raised if the range partially selects a
		 * non-text node.
		 *
		 *  INVALID_NODE_TYPE_ERR: Raised if \c node is an
		 * Attr, Entity, DocumentType, Notation, Document, or
		 * DocumentFragment node.
		 *
		 */
		void surroundContents(const Node &newParent);

		/**
		 * Produces a new range whose end-points are equal to the
		 * end-points of the range.
		 *
		 * @return The duplicated range.
		 *
		 */
		Range cloneRange();

		/**
		 * Returns the contents of a range as a string.
		 *
		 *@return The contents of the range.
		 *
		 */
		DOMString toString();

		/**
		 * Called to indicate that the range is no longer in use and that
		 * the implementation may relinquish any resources associated with
		 * this range. Subsequent calls to any methods or attribute getters
		 * on this range will result in a DOMException being thrown with an
		 * error code of INVALID_STATE_ERR.
		 *
		 */
		void detach();
		
		/**
		 * not part of the DOM
		 * true if the range is detached
		 */
		bool isDetached() const;

		KDOM_INTERNAL_BASE(Range);
		
	private:
		RangeImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET
		KDOM_CAST

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(RangeProto)
KDOM_IMPLEMENT_PROTOFUNC(RangeProtoFunc, Range)

#endif

// vim:ts=4:noet
