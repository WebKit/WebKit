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

#ifndef KDOM_LSParser_H
#define KDOM_LSParser_H

#include <kdom/ecma/DOMLookup.h>
#include <kdom/events/EventTarget.h>

namespace KDOM
{
	class Node;
	class Document;
	class DOMString;
	class DOMConfiguration;
	class LSInput;
	class LSParserImpl;
	class LSParserFilter;

	/**
	 * @short An interface to an object that is able to build, or augment, a DOM
	 * tree from various input sources.
	 *
	 * LSParser provides an API for parsing XML and building the corresponding DOM
	 * document structure. A LSParser instance can be obtained by invoking the
	 * DOMImplementationLS.createLSParser() method.
	 *
	 * @author Rob Buis <buis@kde.org>
	 */
	class LSParser : public EventTarget
	{
	public:
		LSParser();
		explicit LSParser(LSParserImpl *impl);
		LSParser(const LSParser &other);
		virtual ~LSParser();

		// Operators
		LSParser &operator=(const LSParser &other);

		// 'LSParser' functions
		/**
		 * The DOMConfiguration object used when parsing an input source. This
		 * DOMConfiguration is specific to the parse operation. No parameter values
		 * from this DOMConfiguration object are passed automatically to the
		 * DOMConfiguration object on the Document that is created, or used, by the
		 * parse operation. The DOM application is responsible for passing any needed
		 * parameter values from this DOMConfiguration object to the DOMConfiguration
		 * object referenced by the Document object. 
		 */
		DOMConfiguration domConfig() const;

		// TODO
		LSParserFilter filter() const;
		void setFilter(const LSParserFilter &filter);

		/**
		 * @returns true if the LSParser is asynchronous, false if it is synchronous. 
		 */
		bool async() const;

		/**
		 * @returns true if the LSParser is currently busy loading a
		 * document, otherwise false. 
		 */
		bool busy() const;

		/**
		 * Parse an XML document from a resource identified by a LSInput.
		 *
		 * @param input The LSInput from which the source of the document is to be read.
		 *
		 * @returns If the LSParser is a synchronous LSParser, the newly created and
		 * populated Document is returned. If the LSParser is asynchronous, null is
		 * returned since the document object may not yet be constructed when this
		 * method returns. 
		 */
		Document parse(const LSInput &input);

		/**
		 * Parse an XML document from a location identified by a URI reference
		 * [IETF RFC 2396]. If the URI contains a fragment identifier (see section
		 * 4.1 in [IETF RFC 2396]), the behavior is not defined by this specification,
		 * future versions of this specification may define the behavior.
		 *
		 * @param uri The location of the XML document to be read.
		 *
		 * @returns If the LSParser is a synchronous LSParser, the newly created and
		 * populated Document is returned, or null if an error occured. If the LSParser
		 * is asynchronous, null is returned since the document object may not yet be
		 * constructed when this method returns.
		 */
		Document parseURI(const DOMString &uri);

		/**
		 * Parse an XML fragment from a resource identified by a LSInput and insert the
		 * content into an existing document at the position specified with the context
		 * and action arguments. When parsing the input stream, the context node (or its
		 * parent, depending on where the result will be inserted) is used for resolving
		 * unbound namespace prefixes. The context node's ownerDocument node (or the node
		 * itself if the node of type DOCUMENT_NODE) is used to resolve default attributes
		 * and entity references.
		 *
		 * @param input The LSInput from which the source document is to be read.
		 * The source document must be an XML fragment, i.e. anything except a complete
		 * XML document (except in the case where the context node of type DOCUMENT_NODE,
		 * and the action is ACTION_REPLACE_CHILDREN), a DOCTYPE (internal subset), entity
		 * declaration(s), notation declaration(s), or XML or text declaration(s).
		 * @param contextArg The node that is used as the context for the data that is
		 * being parsed. This node must be a Document node, a DocumentFragment node, or
		 * a node of a type that is allowed as a child of an Element node, e.g. it
		 * cannot be an Attribute node.
		 * @param action This parameter describes which action should be taken between
		 * the new set of nodes being inserted and the existing children of the context
		 * node. The set of possible actions is defined in ACTION_TYPES above.
		 *
		 * @returns Return the node that is the result of the parse operation. If the
		 * result is more than one top-level node, the first one is returned.
		 */
		Node parseWithContext(const LSInput &input, const Node &contextArg,
							  unsigned short action);

		/**
		 * Abort the loading of the document that is currently being loaded by the LSParser.
		 * If the LSParser is currently not busy, a call to this method does nothing.
		 */
		void abort() const;

		// Internal
		KDOM_INTERNAL(LSParser)

	public: // EcmaScript section
		KDOM_BASECLASS_GET
		KDOM_PUT

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, const KJS::Value &value, int attr);
	};
};

KDOM_DEFINE_PROTOTYPE(LSParserProto)
KDOM_IMPLEMENT_PROTOFUNC(LSParserProtoFunc, LSParser)

#endif

// vim:ts=4:noet
