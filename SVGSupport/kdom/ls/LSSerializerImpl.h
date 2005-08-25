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

#ifndef KDOM_LSSerializerImpl_H
#define KDOM_LSSerializerImpl_H

#include <kdom/Shared.h>
#include <qstring.h>

class QTextStream;

namespace KDOM
{
	class NodeImpl;
	class DOMString;
	class DOMConfigurationImpl;
	class LSOutputImpl;
	class LSSerializerFilterImpl;
	class DocumentTypeImpl;

	class LSSerializerImpl : public Shared
	{
	public:
		LSSerializerImpl();
		virtual ~LSSerializerImpl();

		// 'LSSerializer' functions
		DOMConfigurationImpl *domConfig() const;

		DOMString newLine() const;
		void setNewLine(DOMStringImpl *newLine);

		LSSerializerFilterImpl *filter() const;
		void setFilter(LSSerializerFilterImpl *filter);

		bool write(NodeImpl *nodeArg, LSOutputImpl *output);
		bool writeToURI(NodeImpl *nodeArg, DOMStringImpl *uri);
		DOMString writeToString(NodeImpl *nodeArg);

		// Internal
		static void PrintNode(QTextStream &ret, NodeImpl *node,
							  const QString &indent = QString(),
							  const QString &newLine = QString::fromLatin1("\n"),
							  unsigned short level = 0,
							  DOMConfigurationImpl *config = 0);

		/**
		 * Prints the document type @p docType to text stream @p ret with
		 * the string @p indent idented @p level times.
		 *
		 * @param ret the text stream to output to
		 * @param docType the Document Type to serialize
		 * @param prettyIndent if true, the subset is indented according to @p level and @p indent
		 * @param level the indentation level counted in spaces
		 * @param indent the string to indent with
		 */
		static void PrintInternalSubset(QTextStream &ret,
						DocumentTypeImpl *docType,
						bool prettyIndent = false,
						unsigned short level = 0,
						const QString &indent = QString());

		/**
		 * Prints spaces to text stream @p ret with the indentation string @p indent
		 * at the hierarchy level @p level.
		 *
		 * For example, the following code:
		 *
		 * \code
		 * KDOM::Helper::PrintIndentation(ret, 2, "  ");
		 * \endcode
		 *
		 * would be equivalent to four spaces.
		 *
		 * @param ret the text stream to output to
		 * @param level the position in the XML hierarchy
		 * @param indent the string to indent
		 */
		static void PrintIndentation(QTextStream &ret,
									 unsigned short level,
									 const QString &indent);

	private:

		/**
		 * Escapes @p escapee with XML's builtin character references, 
		 * except for apostrophes.
		 *
		 * The following characters are mapped to their corresponding entities:
		 *
		 * > == &gt;
		 * < == &lt;
		 * & == &amp;
		 *
		 * @param escapee the string to escape
		 * @returns a new string with character entities
		 */
		static DOMStringImpl *escape(DOMStringImpl *escapee);

		/**
		 * Identical to escape() with the addition that apostrophes and quotes 
		 * are also escaped. E.g:
		 *
		 * ' == &apos;
		 * " == &quot;
		 */
		static DOMStringImpl *escapeAttribute(DOMStringImpl *escapee);

		bool serialize(NodeImpl *nodeArg, LSOutputImpl *output) const;

		static long acceptNode();

	private:
		DOMStringImpl *m_newLine;
		mutable DOMConfigurationImpl *m_config;
		LSSerializerFilterImpl *m_serializerFilter;
	};
};

#endif

// vim:ts=4:noet
