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

#ifndef KDOM_LSSerializer_H
#define KDOM_LSSerializer_H

#include <kdom/ecma/DOMLookup.h>

namespace KDOM
{
	class Node;
	class DOMString;
	class DOMConfiguration;
	class LSOutput;
	class LSSerializerImpl;

	/**
	 * @short A LSSerializer provides an API for serializing (writing) a DOM document
	 * out into XML.
	 *
	 * The XML data is written to a string or an output stream. Any changes or
	 * fixups made during the serialization affect only the serialized data.
	 * The Document object and its children are never altered by the
	 * serialization operation.
	 *
	 * @author Rob Buis <buis@kde.org>
	 */
	class LSSerializer
	{
	public:
		LSSerializer();
		explicit LSSerializer(LSSerializerImpl *impl);
		LSSerializer(const LSSerializer &other);
		virtual ~LSSerializer();

		// Operators
		LSSerializer &operator=(const LSSerializer &other);
		bool operator==(const LSSerializer &other) const;
		bool operator!=(const LSSerializer &other) const;

		// 'LSSerializer' functions
		/**
		 * The DOMConfiguration object used by the LSSerializer when serializing
		 * a DOM node. 
		 */
		DOMConfiguration domConfig() const;

		/**
		 * The end-of-line sequence of characters to be used in the XML being
		 * written out. Any string is supported, but XML treats only a certain
		 * set of characters sequence as end-of-line (See section 2.11,
		 * "End-of-Line Handling" in [XML 1.0], if the serialized content is XML 1.0
		 * or section 2.11, "End-of-Line Handling" in [XML 1.1], if the serialized
		 * content is XML 1.1). Using other character sequences than the recommended
		 * ones can result in a document that is either not serializable or not
		 * well-formed). 
		 * On retrieval, the default value of this attribute is the implementation
		 * specific default end-of-line sequence. DOM implementations should choose
		 * the default to match the usual convention for text files in the environment
		 * being used. Implementations must choose a default sequence that matches one
		 * of those allowed by XML 1.0 or XML 1.1, depending on the serialized content.
		 * Setting this attribute to null will reset its value to the default value. 
		 */
		DOMString newLine() const;
		void setNewLine(const DOMString &newLine);

		//LSSerializerFilter filter() const;
		//void setFilter(const LSSerializerFilter &filter);

		/**
		 * Serialize the specified node as described above in the general
		 * description of the LSSerializer interface. The output is written
		 * to the supplied LSOutput.
		 *
		 * @param nodeArg The node to serialize.
		 * @param destination The destination for the serialized DOM.
		 *
		 * @returns Returns true if node was successfully serialized. Return
		 * false in case the normal processing stopped but the implementation
		 * kept serializing the document; the result of the serialization
		 * being implementation dependent then.
		 */
		bool write(const Node &nodeArg, const LSOutput &output);

		/**
		 * A convenience method that acts as if LSSerializer.write was called
		 * with a LSOutput with no encoding specified and LSOutput.systemId set
		 * to the uri argument.
		 *
		 * @param nodeArg The node to serialize.
		 * @param uri The URI to write to.
		 *
		 * @returns Returns true if node was successfully serialized. Return
		 * false in case the normal processing stopped but the implementation
		 * kept serializing the document; the result of the serialization
		 * being implementation dependent then.
		 */
		bool writeToURI(const Node &nodeArg, const DOMString &uri);

		/**
		 * Serialize the specified node as described above in the general
		 * description of the LSSerializer interface. The output is written to
		 * a DOMString that is returned to the caller. The encoding used is the
		 * encoding of the DOMString type, i.e. UTF-16. Note that no Byte Order
		 * Mark is generated in a DOMString object.
		 *
		 * @param nodeArg The node to serialize.
		 * 
		 * @returns Returns the serialized data.
		 */
		DOMString writeToString(const Node &nodeArg);

		// Internal
		KDOM_INTERNAL_BASE(LSSerializer)

	private:
		LSSerializerImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET
		KDOM_PUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, KJS::ValueImp *value, int attr);
	};
};

KDOM_DEFINE_PROTOTYPE(LSSerializerProto)
KDOM_IMPLEMENT_PROTOFUNC(LSSerializerProtoFunc, LSSerializer)

#endif

// vim:ts=4:noet
