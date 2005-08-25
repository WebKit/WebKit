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

#ifndef KDOM_DocumentBuilder_H
#define KDOM_DocumentBuilder_H

namespace KDOM
{
	class NodeImpl;
	class DOMString;
	class DocumentImpl;
	class DOMImplementation;

	/**
	 * KDOM Document builder.
	 */
	class DocumentBuilder
	{
	public:
		DocumentBuilder();
		virtual ~DocumentBuilder();

		/**
		 * @returns the parsed document.
		 */
		DocumentImpl *document() const;

	public:
		virtual bool startDocument(const KURL &uri);
		virtual bool endDocument();

		virtual bool startElement(const DOMString &tagName);
		virtual bool startElementNS(const DOMString &namespaceURI, const DOMString &prefix, const DOMString &localName);
		virtual bool startElementNS(const DOMString &namespaceURI, const DOMString &qName);
		
		virtual bool endElement(const DOMString &tagName);
		virtual bool endElementNS(const DOMString &namespaceURI, const DOMString &prefix, const DOMString &localName);
		virtual void startElementEnd();

		virtual void startAttribute(const DOMString &localName, const DOMString &value);
		virtual void startAttributeNS(const DOMString &namespaceURI, const DOMString &qName, const DOMString &value);

		virtual bool comment(const DOMString &ch);
		virtual bool characters(const DOMString &ch);
				
		virtual bool startCDATA();
		virtual bool endCDATA();
		
		virtual bool startPI(const DOMString &target, const DOMString &data);
		virtual bool startDTD(const DOMString &name, const DOMString &publicId, const DOMString &systemId);

		virtual bool internalEntityDecl(const DOMString &name, const DOMString &value, bool deep = false);
		virtual bool internalEntityDeclEnd();
		
		virtual bool externalEntityDecl(const DOMString &name, const DOMString &publicId, const DOMString &systemId);
		virtual bool unparsedEntityDecl(const DOMString &name, const DOMString &publicId, const DOMString &systemId, const DOMString &notationName);

		virtual bool notationDecl(const DOMString &name, const DOMString &publicId, const DOMString &systemId);

		virtual bool entityReferenceStart(const DOMString &name);
		virtual bool entityReferenceEnd(const DOMString &name);

		/**
		 * @returns a node related to where the error occured.
		 *
		 * This is used for error reporting.
		 */
		NodeImpl *currentNode() const;

		// Used by DOM3 Load/Save part
		void pushNode(NodeImpl *node);
		NodeImpl *popNode();

		void setDocument(DocumentImpl *doc);

	private:
		class Private;
		Private *d;
	};
};

#endif

// vim:ts=4:noet
