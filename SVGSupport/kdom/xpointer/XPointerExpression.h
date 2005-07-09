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

#ifndef KDOM_XPointerExpression_H
#define KDOM_XPointerExpression_H

namespace KDOM
{
	class Document;
	class DOMString;

namespace XPointer
{
	class XPointerResult;
	class XPointerExpressionImpl;

	/**
	 * Expressions can be created with XPointerEvalutor.
	 *
	 * An XPointerExpression is a memory representation of an XPointer pointer,
	 * in an internal format for doing fast lookups.
	 *
	 * XPointerExpression instances can be created with XPointerEvaluator.
	 *
	 * @author Frans Englich <frans.englich@telia.com>
	 * @see XPointerEvalutor
	 */
	class XPointerExpression
	{
	public:
		XPointerExpression();
		explicit XPointerExpression(XPointerExpressionImpl *i);
		XPointerExpression(const XPointerExpression &other);
		virtual ~XPointerExpression();

		// Operators
		XPointerExpression &operator=(const XPointerExpression &other);
		bool operator==(const XPointerExpression &other) const;
		bool operator!=(const XPointerExpression &other) const;

		/**
		 * Returns the string which the pointer was constructed from.
		 */
		DOMString string() const;

		/**
		 * Evaluates the pointer in a node, and returns the result, if any.
		 *
		 * @param context the node to use as evaluation context. If context is null(default), the
		 * Document node the expression was created from is used.
		 */
	    virtual XPointerResult evaluate() const;

		KDOM_INTERNAL_BASE(XPointerExpression)

	protected:
		XPointerExpressionImpl *d;
	};
};

};

#endif

// vim:ts=4:noet
