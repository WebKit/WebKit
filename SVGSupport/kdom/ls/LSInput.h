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

#ifndef KDOM_LSInput_H
#define KDOM_LSInput_H

#include <kdom/ecma/DOMLookup.h>

class QTextIStream;

namespace KDOM
{
	class DOMString;
	class LSInputImpl;

	/**
	 * @short This interface represents an input source for data.
	 *
	 * This interface allows an application to encapsulate information about an
	 * input source in a single object, which may include a public identifier, a
	 * system identifier, a byte stream (possibly with a specified encoding), a
	 * base URI, and/or a character stream.
	 *
	 * @author Rob Buis <buis@kde.org>
	 */
	class LSInput
	{
	public:
		LSInput();
		explicit LSInput(LSInputImpl *impl);
		LSInput(const LSInput &other);
		virtual ~LSInput();

		// Operators
		LSInput &operator=(const LSInput &other);
		bool operator==(const LSInput &other) const;
		bool operator!=(const LSInput &other) const;

		// 'LSInput' functions

		/**
		 * An attribute of a language and binding dependent type that represents
		 * a stream of 16-bit units. The application must encode the stream using
		 * UTF-16 (defined in [Unicode] and in [ISO/IEC 10646]). It is not a
		 * requirement to have an XML declaration when using character streams.
		 * If an XML declaration is present, the value of the encoding attribute
		 * will be ignored. 
		 */
		QTextIStream *characterStream() const;
		void setCharacterStream(QTextIStream *characterStream);

		/**
		 * An attribute of a language and binding dependent type that represents
		 * a stream of bytes.  If the application knows the character encoding
		 * of the byte stream, it should set the encoding attribute. Setting the
		 * encoding in this way will override any encoding specified in an XML
		 * declaration in the data. 
		 */
		DOMString byteStream() const;
		void setByteStream(const DOMString &byteStream);

		/**
		 * String data to parse. If provided, this will always be treated as
		 * a sequence of 16-bit units (UTF-16 encoded characters). It is not a
		 * requirement to have an XML declaration when using stringData. If an
		 * XML declaration is present, the value of the encoding attribute will
		 * be ignored. 
		 */
		DOMString stringData() const;
		void setStringData(const DOMString &stringData);

		/**
		 * The system identifier, a URI reference [IETF RFC 2396], for this
		 * input source. The system identifier is optional if there is a byte
		 * stream, a character stream, or string data. It is still useful to
		 * provide one, since the application will use it to resolve any
		 * relative URIs and can include it in error messages and warnings.
		 * (The LSParser will only attempt to fetch the resource identified by
		 * the URI reference if there is no other input available in the input
		 * source.) 
		 * If the application knows the character encoding of the object
		 * pointed to by the system identifier, it can set the encoding using
		 * the encoding attribute. 
		 * If the specified system ID is a relative URI reference (see section
		 * 5 in [IETF RFC 2396]), the DOM implementation will attempt to resolve
		 * the relative URI with the baseURI as the base, if that fails, the
		 * behavior is implementation dependent. 
		 */
		DOMString systemId() const;
		void setSystemId(const DOMString &systemId);

		/**
		 * The public identifier for this input source. This may be mapped to
		 * an input source using an implementation dependent mechanism (such as
		 * catalogues or other mappings). The public identifier, if specified,
		 * may also be reported as part of the location information when errors
		 * are reported. 
		 */
		DOMString publicId() const;
		void setPublicId(const DOMString &publicId);

		/**
		 * The base URI to be used (see section 5.1.4 in [IETF RFC 2396]) for
		 * resolving a relative systemId to an absolute URI. 
		 * If, when used, the base URI is itself a relative URI, an empty
		 * string, or null, the behavior is implementation dependent. 
		 */
		DOMString baseURI() const;
		void setBaseURI(const DOMString &baseURI);

		/**
		 * The character encoding, if known. The encoding must be a string
		 * acceptable for an XML encoding declaration ([XML 1.0] section
		 * 4.3.3 "Character Encoding in Entities"). 
		 * This attribute has no effect when the application provides a
		 * character stream or string data. For other sources of input, an
		 * encoding specified by means of this attribute will override any
		 * encoding specified in the XML declaration or the Text declaration,
		 * or an encoding obtained from a higher level protocol, such as
		 * HTTP [IETF RFC 2616]. 
		 */
		DOMString encoding() const;
		void setEncoding(const DOMString &encoding);

		/**
		 * If set to true, assume that the input is certified (see section 2.13
		 * in [XML 1.1]) when parsing [XML 1.1]. 
		 */
		bool certifiedText() const;
		void setCertifiedText(bool certifiedText);

		// Internal
		KDOM_INTERNAL_BASE(LSInput)

	private:
		LSInputImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET
		KDOM_PUT
		KDOM_CAST

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, KJS::ValueImp *value, int attr);
	};

	KDOM_DEFINE_CAST(LSInput)
};

#endif

// vim:ts=4:noet
