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

#ifndef KDOM_Entity_H
#define KDOM_Entity_H

#include <kdom/Node.h>

namespace KDOM
{
	class DOMString;
	class EntityImpl;

	/**
	 * @short This interface represents a known entity, either parsed or
	 * unparsed, in an XML document. Note that this models the entity itself
	 * not the entity declaration.
	 *
	 * The nodeName attribute that is inherited from Node contains the name of
	 * the entity.
	 *
	 * An XML processor may choose to completely expand entities before the
	 * structure model is passed to the DOM; in this case there will be no
	 * EntityReference nodes in the document tree.
	 *
	 * XML does not mandate that a non-validating XML processor read and
	 * process entity declarations made in the external subset or declared
	 * in parameter entities. This means that parsed entities declared in the
	 * external subset need not be expanded by some classes of applications, and
	 * that the replacement text of the entity may not be available. When the
	 * replacement text is available, the corresponding Entity node's child list
	 * represents the structure of that replacement value. Otherwise, the child
	 * list is empty.
	 *
	 * DOM Level 3 does not support editing Entity nodes; if a user wants to
	 * make changes to the contents of an Entity, every related EntityReference
	 * node has to be replaced in the structure model by a clone of the
	 * Entity's contents, and then the desired changes must be made to each of
	 * those clones instead. Entity nodes and all their descendants are readonly.
	 *
	 * An Entity node does not have any parent.
	 *
	 * @author Rob Buis <buis@kde.org>
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 */
	class Entity : public Node 
	{
	public:
		Entity();
		explicit Entity(EntityImpl *i);
		Entity(const Entity &other);
		Entity(const Node &other);
		virtual ~Entity();

		// Operators
		Entity &operator=(const Entity &other);
		Entity &operator=(const Node &other);

		// 'Entity' functions
		/**
		 * The public identifier associated with the entity if specified, and null
		 * otherwise.
		 */
		DOMString publicId() const;

		/**
		 * The system identifier associated with the entity if specified, and null
		 * otherwise. This may be an absolute URI or not.
		 */
		DOMString systemId() const;

		/**
		 * For unparsed entities, the name of the notation for the entity. For parsed
		 * entities, this is null.
		 */
		DOMString notationName() const;

		/**
		 * An attribute specifying the encoding used for this entity at the time of
		 * parsing, when it is an external parsed entity. This is null if it an
		 * entity from the internal subset or if it is not known.
		 */
		DOMString inputEncoding() const; // DOM3

		/**
		 * An attribute specifying, as part of the text declaration, the encoding
		 * of this entity, when it is an external parsed entity. This is null otherwise.
		 */
		DOMString xmlEncoding() const; // DOM3

		/**
		 * An attribute specifying, as part of the text declaration, the version
		 * number of this entity, when it is an external parsed entity. This is
		 * null otherwise.
		 */
		DOMString xmlVersion() const; // DOM3

		// Internal
		KDOM_INTERNAL(Entity)

	public: // EcmaScript section
		KDOM_GET
		KDOM_FORWARDPUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

#endif

// vim:ts=4:noet
