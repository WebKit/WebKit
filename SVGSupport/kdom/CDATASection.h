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

#ifndef KDOM_CDATASection_H
#define KDOM_CDATASection_H

#include <kdom/Text.h>

namespace KDOM
{
	class CDATASectionImpl;

	/**
	 * @short CDATA sections are used to escape blocks of text
	 * containing characters that would otherwise be regarded as markup.
	 * The only delimiter that is recognized in a CDATA section is the
	 * "]]>" string that ends the CDATA section. CDATA sections cannot
	 * be nested. Their primary purpose is for including material such as
	 * XML fragments, without needing to escape all the delimiters.
	 *
	 * The CharacterData.data attribute holds the text that is contained
	 * by the CDATA section. Note that this may contain characters that
	 * need to be escaped outside of CDATA sections and that, depending
	 * on the character encoding ("charset") chosen for serialization, it
	 * may be impossible to write out some characters as part of a CDATA
	 * section.
	 *
	 * The CDATASection interface inherits from the CharacterData
	 * interface through the Text interface. Adjacent CDATASection nodes
	 * are not merged by use of the normalize method of the Node interface.
	 *
	 * No lexical check is done on the content of a CDATA section and
	 * it is therefore possible to have the character sequence "]]>"
	 * in the content, which is illegal in a CDATA section per section
	 * 2.7 of [XML 1.0]. The presence of this character sequence must
	 * generate a fatal error during serialization or the cdata section
	 * must be splitted before the serialization (see also the parameter
	 * "split-cdata-sections" in the DOMConfiguration interface).
	 *
	 * Note: Because no markup is recognized within a CDATASection,
	 * character numeric references cannot be used as an escape mechanism
	 * when serializing. Therefore, action needs to be taken when
	 * serializing a CDATASection with a character encoding where
	 * some of the contained characters cannot be represented. Failure
	 * to do so would not produce well-formed XML.
	 * One potential solution in the serialization process is to end
	 * the CDATA section before the character, output the character
	 * using a character reference or entity reference, and open a
	 * new CDATA section for any further characters in the text node.
	 * Note, however, that some code conversion libraries at the time
	 * of writing do not return an error or exception when a character
	 * is missing from the encoding, making the task of ensuring that
	 * data is not corrupted on serialization more difficult.
	 *
	 * @author Rob Buis <buis@kde.org>
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 */
	class CDATASection : public Text 
	{
	public:
		CDATASection();
		explicit CDATASection(CDATASectionImpl *i);
		CDATASection(const CDATASection &other);
		CDATASection(const Node &other);
		virtual ~CDATASection();

		// Operators
		CDATASection &operator=(const CDATASection &other);
		CDATASection &operator=(const Node &other);

		// Internal
		KDOM_INTERNAL(CDATASection)
	};
};

#endif

// vim:ts=4:noet
