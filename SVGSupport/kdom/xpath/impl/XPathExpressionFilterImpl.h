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

#ifndef KDOM_XPath_XPathExpressionFilterImpl_H
#define KDOM_XPath_XPathExpressionFilterImpl_H

#include <kdom/Shared.h>
#include <kdom/xpath/impl/AxisImpl.h>
#include <kdom/xpath/impl/OperatorImpl.h>
#include <kdom/xpath/impl/StepImpl.h>

namespace KDOM
{

namespace XPath
{
	class ValueImpl;

	/**
	 * An XPathExpressionFilterImpl instance is used by an XPathFactoryBaseImpl 
	 * instance when creating AST nodes for an XPath expression, in order to
	 * to detect particular nodes which are not allowed, as dicated by the 
	 * filter. Some scenarios, such as W3C XML Schema's identity constraints,
	 * does not allow full XPath expressions, but a sub-set.
	 *
	 * By default is all types of nodes accepted. In other words, all accept() calls
	 * returns true.
	 *
	 * @author Frans Englich <frans.englich@telia.com>
	 */
	class XPathExpressionFilterImpl : public Shared
	{
	public:
	    XPathExpressionFilterImpl();
	    virtual~XPathExpressionFilterImpl();

		virtual bool acceptFunctionCall(const char *name) const;
		virtual bool acceptVariableRef(const char *name) const;
		virtual bool acceptLiteral(ValueImpl *value) const;
		virtual bool acceptStep(StepImpl::StepId id) const;
		virtual bool acceptAxis(AxisImpl::AxisId id) const;
		virtual bool acceptOperator(OperatorImpl::OperatorId id) const;

	};
};
};

#endif
// vim:ts=4:noet
