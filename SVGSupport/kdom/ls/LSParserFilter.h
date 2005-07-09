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

#ifndef KDOM_LSParserFilter_H
#define KDOM_LSParserFilter_H

#include <kdom/ecma/DOMLookup.h>

namespace KDOM
{
	class Node;
	class Element;
	class LSParserFilterImpl;

	/**
	 * @short An interface to an object that is able to build, or augment, a DOM
	 * tree from various input sources.
	 *
	 * LSParserFilter provides an API for parsing XML and building the corresponding DOM
	 * document structure. A LSParserFilter instance can be obtained by invoking the
	 * DOMImplementationLS.createLSParserFilter() method.
	 *
	 * @author Rob Buis <buis@kde.org>
	 */
	class LSParserFilter
	{
	public:
		LSParserFilter();
		explicit LSParserFilter(LSParserFilterImpl *impl);
		LSParserFilter(const LSParserFilter &other);
		virtual ~LSParserFilter();

		// Operators
		LSParserFilter &operator=(const LSParserFilter &other);
		bool operator==(const LSParserFilter &other) const;
		bool operator!=(const LSParserFilter &other) const;

		// 'LSParserFilter' functions
		/**
		 * This method will be called by the parser at the completion of the
		 * parsing of each node. The node and all of its descendants will exist
		 * and be complete. The parent node will also exist, although it may be
		 * incomplete, i.e. it may have additional children that have not yet been
		 * parsed. Attribute nodes are never passed to this function.
		 * From within this method, the new node may be freely modified - children
		 * may be added or removed, text nodes modified, etc. The state of the rest
		 * of the document outside this node is not defined, and the affect of any
		 * attempt to navigate to, or to modify any other part of the document is undefined. 
		 * For validating parsers, the checks are made on the original document, before
		 * any modification by the filter. No validity checks are made on any document
		 * modifications made by the filter.
		 * If this new node is rejected, the parser might reuse the new node and any
		 * of its descendants.
		 *
		 * @param nodeArg The newly constructed element. At the time this method is
		 * called, the element is complete - it has all of its children (and their
		 * children, recursively) and attributes, and is attached as a child to its parent. 
		 *
		 * @returns FILTER_ACCEPT if this Node should be included in the DOM document
		 * being built. 
		 * FILTER_REJECT if the Node and all of its children should be rejected. 
		 * FILTER_SKIP if the Node should be skipped and the Node should be replaced
		 * by all the children of the Node. 
		 * FILTER_INTERRUPT if the filter wants to stop the processing of the document.
		 * Interrupting the processing of the document does no longer guarantee that
		 * the resulting DOM tree is XML well-formed. The Node is accepted and will
		 * be the last completely parsed node.
		 */
		unsigned short acceptNode(const Node &nodeArg);

		/**
		 * The parser will call this method after each Element start tag has
		 * been scanned, but before the remainder of the Element is processed.
		 * The intent is to allow the element, including any children, to be
		 * efficiently skipped. Note that only element nodes are passed to the
		 * startElement function. 
		 * The element node passed to startElement for filtering will include all
		 * of the Element's attributes, but none of the children nodes. The Element
		 * may not yet be in place in the document being constructed (it may not
		 * have a parent node.) 
		 * A startElement filter function may access or change the attributes for
		 * the Element. Changing Namespace declarations will have no effect on
		 * namespace resolution by the parser.
		 * For efficiency, the Element node passed to the filter may not be the
		 * same one as is actually placed in the tree if the node is accepted. And
		 * the actual node (node object identity) may be reused during the process
		 * of reading in and filtering a document.
		 *
		 * @param elementArg The newly encountered element. At the time this method is
		 * called, the element is incomplete - it will have its attributes, but no
		 * children.
		 *
		 * @returns FILTER_ACCEPT if the Element should be included in the DOM document
		 * being built. 
		 * FILTER_REJECT if the Element and all of its children should be rejected.
		 * FILTER_SKIP if the Element should be skipped. All of its children are inserted
		 * in place of the skipped Element node.
		 * FILTER_INTERRUPT if the filter wants to stop the processing of the document.
		 * Interrupting the processing of the document does no longer guarantee that
		 * the resulting DOM tree is XML well-formed. The Element is rejected.
		 */
		unsigned short startElement(const Element &elementArg);

		/**
		 * Tells the LSParser what types of nodes to show to the method
		 * LSParserFilter.acceptNode. If a node is not shown to the filter using
		 * this attribute, it is automatically included in the DOM document being built.
		 * See NodeFilter for definition of the constants. The constants
		 * SHOW_ATTRIBUTE, SHOW_DOCUMENT, SHOW_DOCUMENT_TYPE, SHOW_NOTATION,
		 * SHOW_ENTITY, and SHOW_DOCUMENT_FRAGMENT are meaningless here. Those
		 * nodes will never be passed to LSParserFilter.acceptNode. 
		 * The constants used here are defined in [DOM Level 2 Traversal and Range]. 
		 */
		unsigned long whatToShow() const;

		// Internal
		KDOM_INTERNAL_BASE(LSParserFilter)

	private:
		LSParserFilterImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(LSParserFilterProto)
KDOM_IMPLEMENT_PROTOFUNC(LSParserFilterProtoFunc, LSParserFilter)

#endif

// vim:ts=4:noet
