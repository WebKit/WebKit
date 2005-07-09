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

#include <kstaticdeleter.h>

#include "DOMString.h"
#include <kdom/Shared.h>
#include "kdomxpath.h"

#include "AxisImpl.h"
#include "FunctionCallImpl.h"
#include "LiteralImpl.h"
#include "OperatorImpl.h"
#include "StepImpl.h"
#include "VariableRefImpl.h"
#include "XPathCustomExceptionImpl.h"
#include "XPathExpressionFilterImpl.h"
#include "XPathFactory1Impl.h"
#include "XPathFactoryBaseImpl.h"

using namespace KDOM;
using namespace KDOM::XPath;

static KStaticDeleter<XPathFactoryBaseImpl> instanceDeleter;
XPathFactoryBaseImpl *XPathFactoryBaseImpl::s_factory = 0;

XPathFactoryBaseImpl::XPathFactoryBaseImpl() : m_exprFilter(0)
{
}

XPathFactoryBaseImpl::~XPathFactoryBaseImpl()
{
	if(m_exprFilter)
		m_exprFilter->deref();
}

XPathFactoryBaseImpl *XPathFactoryBaseImpl::defaultFactory()
{
	if(!s_factory)
		s_factory = instanceDeleter.setObject(s_factory, new XPathFactory1Impl());
 
	return s_factory;
}

void XPathFactoryBaseImpl::setDefaultFactory(XPathFactoryBaseImpl *factory)
{
    delete s_factory;
    s_factory = factory;
}

void XPathFactoryBaseImpl::setExpressionFilter(XPathExpressionFilterImpl *filter)
{
	KDOM_SAFE_SET(m_exprFilter, filter);
}

XPathExpressionFilterImpl *XPathFactoryBaseImpl::expressionFilter() const
{
	return m_exprFilter;
}

FunctionCallImpl *XPathFactoryBaseImpl::createFunctionCall(const char *name)
{
	if(m_exprFilter && !m_exprFilter->acceptFunctionCall(name))
		throw new XPathCustomExceptionImpl(FORBIDDEN_EXPRESSION_COMPONENT);

	return _createFunctionCall(name);
}

VariableRefImpl *XPathFactoryBaseImpl::createVariableRef(const char *name)
{
	if(m_exprFilter && !m_exprFilter->acceptVariableRef(name))
		throw new XPathCustomExceptionImpl(FORBIDDEN_EXPRESSION_COMPONENT);

	return _createVariableRef(name);
}

LiteralImpl *XPathFactoryBaseImpl::createLiteral(ValueImpl *value)
{
	if(m_exprFilter && !m_exprFilter->acceptLiteral(value))
		throw new XPathCustomExceptionImpl(FORBIDDEN_EXPRESSION_COMPONENT);

	return _createLiteral(value);
}

StepImpl *XPathFactoryBaseImpl::createStep(StepImpl::StepId id)
{
	if(m_exprFilter && !m_exprFilter->acceptStep(id))
		throw new XPathCustomExceptionImpl(FORBIDDEN_EXPRESSION_COMPONENT);

	return _createStep(id);
}

AxisImpl *XPathFactoryBaseImpl::createAxis(AxisImpl::AxisId id)
{
	if(m_exprFilter && !m_exprFilter->acceptAxis(id))
		throw new XPathCustomExceptionImpl(FORBIDDEN_EXPRESSION_COMPONENT);

	return _createAxis(id);
}

OperatorImpl *XPathFactoryBaseImpl::createOperator(OperatorImpl::OperatorId id)
{
	if(m_exprFilter && !m_exprFilter->acceptOperator(id))
		throw new XPathCustomExceptionImpl(FORBIDDEN_EXPRESSION_COMPONENT);

	return _createOperator(id);
}

// vim:ts=4:noet
