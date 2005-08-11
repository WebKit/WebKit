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

#ifndef KDOM_Notation_H
#define KDOM_Notation_H

#include <kdom/Node.h>

namespace KDOM
{
	class DOMString;
	class NotationImpl;

	/**
	 * @short This interface represents a notation declared in the DTD.
	 * A notation either declares, by name, the format of an unparsed
	 * entity (see section 4.7 of the XML 1.0 specification [XML 1.0]), or
	 * is used for formal declaration of processing instruction targets
	 * (see section 2.6 of the XML 1.0 specification [XML 1.0]). The
	 * nodeName attribute inherited from Node is set to the declared
	 * name of the notation.
	 *
	 * The DOM Core does not support editing Notation nodes; they
	 * are therefore readonly.
	 *
	 * A Notation node does not have any parent.
	 *
	 * @author Rob Buis <buis@kde.org>
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 */
	class Notation : public Node 
	{
	public:
		Notation();
		explicit Notation(NotationImpl *i);
		Notation(const Notation &other);
		Notation(const Node &other);
		virtual ~Notation();

		// Operators
		Notation &operator=(const Notation &other);
		Notation &operator=(const Node &other);

		// 'Notation' functions
		/**
		 * The public identifier of this notation. If the public
		 * identifier was not specified, this is null.
		 */
		DOMString publicId() const;

		/**
		 * The system identifier of this notation. If the system
		 * identifier was not specified, this is null. This may be an
		 * absolute URI or not.
		 */
		DOMString systemId() const;

		// Internal
		KDOM_INTERNAL(Notation)

	public: // EcmaScript section
		KDOM_GET
		KDOM_FORWARDPUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

#endif

// vim:ts=4:noet
