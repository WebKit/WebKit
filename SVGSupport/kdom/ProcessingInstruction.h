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

#ifndef KDOM_ProcessingInstruction_H
#define KDOM_ProcessingInstruction_H

#include <kdom/Node.h>

namespace KDOM
{
	class DOMString;
	class ProcessingInstructionImpl;

	/**
	 * @short The ProcessingInstruction interface represents a
	 * "processing instruction", used in XML as a way to keep
	 * processor-specific information in the text of the document.
	 *
	 * No lexical check is done on the content of a processing
	 * instruction and it is therefore possible to have the character
	 * sequence "?>" in the content, which is illegal a processing
	 * instruction per section 2.6 of [XML 1.0]. The presence of
	 * this character sequence must generate a fatal error during
	 * serialization.
	 *
	 * @author Rob Buis <buis@kde.org>
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 */
	class ProcessingInstruction : public Node 
	{
	public:
		ProcessingInstruction();
		explicit ProcessingInstruction(ProcessingInstructionImpl *i);
		ProcessingInstruction(const ProcessingInstruction &other);
		ProcessingInstruction(const Node &other);
		virtual ~ProcessingInstruction();

		// Operators
		ProcessingInstruction &operator=(const ProcessingInstruction &other);
		ProcessingInstruction &operator=(const Node &other);

		// 'ProcessingInstruction' functions
		/**
		 * The target of this processing instruction. XML defines this
		 * as being the first token following the markup that begins the
		 * processing instruction.
		 */
		DOMString target() const;

		/**
		 * The content of this processing instruction. This is from the
		 * first non white space character after the target to the
		 * character immediately preceding the ?>.
		 */
		DOMString data() const;
		void setData(const DOMString &data);

		// Internal
		KDOM_INTERNAL(ProcessingInstruction)

	public: // EcmaScript section
		KDOM_GET
		KDOM_PUT

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, const KJS::Value &value, int attr);
	};
};

#endif

// vim:ts=4:noet
