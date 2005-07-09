/*
 * This file is part of the KDE libraries
 *
 * Copyright (C) 2005 Frans Englich 	<frans.englich@telia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef KDOM_XPointer_PointerPartImpl_H
#define KDOM_XPointer_PointerPartImpl_H

#include <kdom/Shared.h>

namespace KDOM
{

class NodeImpl;
class DOMString;
namespace XPointer
{
	class NBCImpl;
	class XPointerResultImpl;

	/**
	 * @short Base class of all pointer parts, SchemeBased or ShortHand.
	 * 
	 * A PointerPart is a member of a XPointerExpressionImpl, and is the base-class
	 * for what in the specifications EBNF grammar is a PointerPart, with the exception
	 * that xmlns() schemes are not handled(they are handled differently).
	 *
	 * A PointerPartImpl is never constructed with data that is 
	 * invalid according to the XPointer framework productions. However, it may
	 * be invalid according to semantics of the specific scheme in question, since that's what
	 * the specific PointerPartImpl sub-class is supposed to handle.
	 *
	 * @author Frans Englich <frans.englich@telia.com>
	 */
	class PointerPartImpl : public Shared
	{
	public:

	 	/**
		 *
		 * Constructs a PointerPartImpl instance.
		 *
		 * This constructor should throw INVALID_EXPRESSION_ERR if the scheme data
		 * is syntactically invalid or contains any other error which is considered
		 * a static error.
		 *
		 * @note the @p schemeData has been decoded from any encoding such as XPointer escaping 
		 * and is a plain unicode string.
		 *
		 * @note the arguments in the constructor must be passed to the base class, PointerPartImpl.
		 */
		PointerPartImpl(const DOMString &name, const DOMString &schemeData, NBCImpl *nbc);
		virtual ~PointerPartImpl();

		/**
		 * The name of the pointer part. When the pointer part is a ShortHand, this is simply
		 * the NCName. When the PointerPart is a SchemeBased, it is the scheme name, it may
		 * be a QName.
		 *
		 * @returns the scheme name
		 */
		DOMString name() const;

		/**
		 * The scheme's scheme data. That is, the string between the paranteses.
		 *
		 * @note The content is decoded. In other words, not escaped with circumflexes(^)
		 *
		 * @returns the scheme data
		 */
		DOMString data() const;

		/**
		 * The pointer's Namespace Binding Context. It provides
		 * a mapping between prefixes and namespaces, which the pointer part
		 * have to its disposal.
		 */
		NBCImpl *nbc() const;

		/**
		 * Asks the PointerPart to execute itself with the node @p ctxt as
		 * current context.
		 */
		virtual XPointerResultImpl *evaluate(NodeImpl *context) const;

	private:
		DOMString m_data;
		DOMString m_name;
		
		NBCImpl *m_nbc;
	};
};

};

#endif

// vim:ts=4:noet
