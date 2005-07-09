/*
    Copyright(C) 2004 Richard Moore <rich@kde.org>
    Copyright(C) 2005 Frans Englich <frans.englich@telia.com>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or(at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/


#ifndef KDOM_XPathFactoryBaseImpl_H
#define KDOM_XPathFactoryBaseImpl_H

#include <kdom/xpath/impl/AxisImpl.h>
#include <kdom/xpath/impl/OperatorImpl.h>
#include <kdom/xpath/impl/StepImpl.h>

namespace KDOM
{
	
namespace XPath
{
	class AxisImpl;
	class FunctionCallImpl;
	class LiteralImpl;
	class OperatorImpl;
	class StepImpl;
	class ValueImpl;
	class VariableRefImpl;
	class XPathExpressionFilterImpl;

	/**
	 * A Factory for creating AST nodes, instances are used for building XPath 
	 * expressions.
	 *
	 * How an expression is built -- and if at all -- can be influenced by altering
	 * a factory. One method is to inherit the class and override the create calls, in
	 * order to handle custom functions, for example. Another method is to set an 
	 * XPathExpressionFilterImpl via setExpressionFilter(), which is used for checking
	 * if an AST node of a certain type is allowed. If an expression only needs to be
	 * constrained, a filter should be used, since that is sufficient and what it is
	 * designed for.
	 *
	 * If a filter is in use and an AST node is flagged as not accepted, the exception
	 * XPathCustomExceptionImpl is thrown with the code attribute set 
	 * to FORBIDDEN_EXPRESSION_COMPONENT.
	 *
	 * This is a pure virtual class and provides helper functions for
	 * classes implementing it. The static functions defaultFactory() and
	 * setDefaultFactory() might be of interest.
	 *
	 * @see XPathFactory1Impl
	 * @see XPathExpressionFilterImpl
	 * @see XPathCustomExceptionImpl 
	 * @author Frans Englich <frans.englich@telia.com>
	 */
	class XPathFactoryBaseImpl
	{
	public:

		virtual ~XPathFactoryBaseImpl();

		/**
		 * Creates an operator node.
		 */
		virtual OperatorImpl *createOperator(OperatorImpl::OperatorId id);

		/**
		 * Creates an axis node.
		 */
		virtual AxisImpl *createAxis(AxisImpl::AxisId id);

		/**
		 * Creates a location step node.
		 */
		virtual StepImpl *createStep(StepImpl::StepId id);

		/**
		 * Creates a literal node.
		 */
		virtual LiteralImpl *createLiteral(ValueImpl *value);

		/**
		 * Creates a variableref node.
		 */
		virtual VariableRefImpl *createVariableRef(const char *name);

		/**
		 * Creates a function call node.
		 */
		virtual FunctionCallImpl *createFunctionCall(const char *name);

		/** 
		 * Returns the default factory. If no factory have been set with 
		 * setDefaultFactory(), a XPathFactory1Impl() instance will be created(XPath 1.0).
		 */
		static XPathFactoryBaseImpl *defaultFactory();

		/**
		 * Sets the default factory to be the one specified. The existing factory
		 * will be deleted.
		 */
		static void setDefaultFactory(XPathFactoryBaseImpl *factory);

		/**
		 * Return he expression filter in use, if any. By default no 
		 * expression filter is used, accepting plain XPath 1.0 expression.
		 */
		virtual XPathExpressionFilterImpl *expressionFilter() const;

		/**
		 * Sets the expression filter to @p filter. When any create function is 
		 * called, the filter will be asked whether the requested ExprNodeImpl is
		 * allowed.
		 */
		virtual void setExpressionFilter(XPathExpressionFilterImpl *filter);

	protected:

		virtual FunctionCallImpl *_createFunctionCall(const char *name) = 0;
		virtual VariableRefImpl *_createVariableRef(const char *name) = 0;
		virtual LiteralImpl *_createLiteral(ValueImpl *value) = 0;
		virtual StepImpl *_createStep(StepImpl::StepId id) = 0;
		virtual AxisImpl *_createAxis(AxisImpl::AxisId id) = 0;
		virtual OperatorImpl *_createOperator(OperatorImpl::OperatorId id) = 0;

		XPathFactoryBaseImpl();

	private:

		/**
		 * The default factory.
		 */
		static XPathFactoryBaseImpl *s_factory;

		XPathExpressionFilterImpl *m_exprFilter;
	};
};
};
#endif
// vim:ts=4:noet
