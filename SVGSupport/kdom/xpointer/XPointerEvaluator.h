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

#ifndef KDOM_XPointerEvaluator_H
#define KDOM_XPointerEvaluator_H

namespace KDOM
{
	class Node;
	class DOMString;

namespace XPointer
{
	class XPointerResult;
	class XPointerExpression;
	class XPointerEvaluatorImpl;

	/**
	 * A central class for creating XPointerExpressions, as well as provides a shortcut for 
	 * evaluating pointers.
	 *
	 * The XPointerEvaluator is used via the Document interface, which inherits XPointerEvaluator.
	 * A typical usage looks like this:
	 *
	 * \code
	 * // Document doc;
	 * XPointerResult ret = doc.evaluateXPointer("element(/1/2)");
	 *
	 * if(ret.resultType() == XPointerResult::SINGLE_NODE)
	 *     Node mySelectedNode = ret.singleNodeValue();
	 * else
	 *     kdDebug() << "No element found." << endl;
	 * \endcode
	 *
	 * If a particular pointer will be used many times, a performance gain may be reached
	 * by re-using XPointerExpressions, which can be created with XPointerEvaluator::createXPointer(),
	 * to then call the instance's XPointerExpression::evaluate().
	 *
	 * @see XPointerExpression
	 * @author Frans Englich <frans.englich@telia.com>
	 */
	class XPointerEvaluator
	{
	public:
		XPointerEvaluator();
		explicit XPointerEvaluator(XPointerEvaluatorImpl *i);
		XPointerEvaluator(const XPointerEvaluator &other);
		virtual ~XPointerEvaluator();

		XPointerEvaluator &operator=(const XPointerEvaluator &other);
		XPointerEvaluator &operator=(XPointerEvaluatorImpl *other);

		/**
		 * Creates a memory representation of the XPointer string @p pointer.
		 *
		 * This function parses the XPointer string, @p pointer, and compiles an internal
		 * representation of it, which then can be used for evaluating the xpointer.
		 *
		 * It is the callers responsibility to ensure that any media dependent coding or escaping,
		 * such as URI escaping, is resolved before calling this function. However, any XPointer 
		 * escaping, the use of the circumflex(^), is handled by this function. See section 4 of the 
		 * XPointer Framework specification, for a deep discussion.
		 *
		 * @throws INVALID_EXPRESSION_ERR if @p pointer is syntactically invalid
		 * @throws INVALID_ENCODING if @p pointer was escaped in a wrong way. That is, if
		 * the circumflex(^) was errronously used. This is a subset of a syntax error.
		 *
		 * @param xpointer an XPointer string
		 * @param relatedNode a related node, used for error reporting. Typically this is the node which
		 * contained the XPointer string
		 * @see XPointerExpression
		 * @returns an optimized memory representation of @p xpointer
		 */
		virtual XPointerExpression createXPointer(const DOMString &string,
										  const Node &relatedNode = Node::null);

		/**
		 * This is convenience function for evaluating XPointers directly from the string 
		 * representation.
		 *
		 * Calling this function is equivalent to first calling createXPointer() and from the
		 * returned XPointerExpression, call XPointerExpression::evaluate(). A small performance
		 * gain is achieved by reusing XPointerExpressions(which are created for each call with this
		 * function) but it is only noticeble when using a pointer many times.
		 *
		 * @see createXPointer()
		 * @see XPointerExpression
		 * @param xpointer an XPointer string
		 * @param relatedNode a related node, used for error reporting. Typically this is the node which
		 * contained the XPointer string
		 */
		virtual XPointerResult evaluateXPointer(const DOMString& xpointer,
												const Node &relatedNode = Node::null) const;

		KDOM_INTERNAL(XPointerEvaluator)

	protected:
		XPointerEvaluatorImpl *d;
	};
};

};

#endif
// vim:ts=4:noet
