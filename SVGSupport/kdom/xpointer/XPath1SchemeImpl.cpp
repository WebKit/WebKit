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

#include "DOMString.h"
#include "Node.h"

#include "DocumentImpl.h"
#include "NodeImpl.h"
#include "XPath1SchemeImpl.h"
#include "XPathEvaluatorImpl.h"
#include "XPathExpressionImpl.h"
#include "XPathResultImpl.h"
#include "XPointerResultImpl.h"

using namespace KDOM;
using namespace KDOM::XPointer;

XPath1SchemeImpl::XPath1SchemeImpl(const DOMString &schemeData, NBCImpl *nbc)
: PointerPartImpl("xpointer", schemeData, nbc), m_expr(0)
{
	m_expr->ref();
}

XPath1SchemeImpl::~XPath1SchemeImpl()
{
	m_expr->deref();
}

XPointerResultImpl *XPath1SchemeImpl::evaluate(NodeImpl *) const
{
	return new XPointerResultImpl(XPointerResult::NO_MATCH);
}

// vim:ts=4:noet
