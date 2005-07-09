/*
    Copyright (C) 2005 Frans Englich <frans.englich@telia.com>

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

#include <qcstring.h>
#include <qmap.h>

#include "DOMString.h"
#include "XPathHelper.h"
#include "kdomtraversal.h"
#include "kdomxpath.h"

#include "DocumentImpl.h"
#include "Lexer.h"
#include "ExprNodeImpl.h"
#include "NodeFilterImpl.h"
#include "NodeImpl.h"
#include "NodeSetImpl.h"
#include "ParserState.h"
#include "ScopeImpl.h"
#include "TreeWalkerImpl.h"
#include "XPathExceptionImpl.h"
#include "XPathExpressionImpl.h"
#include "XPathResultImpl.h"

using namespace KDOM;
using namespace KDOM::XPath;

XPathExpressionImpl::XPathExpressionImpl(const DOMString &expr, ScopeImpl *s) 
										 : Shared(true), m_ast(0), m_scope(s)
{
	Q_ASSERT(s);

	const QCString cexpr = expr.string().utf8();
	xpathyy_scan_string(cexpr.data());

	if(xpathyyparse())
	// TODO DOMError ParserState::error()
		throw new XPathExceptionImpl(INVALID_EXPRESSION_ERR);

	m_ast = ParserState::node();
	m_ast->ref();
}

XPathExpressionImpl::~XPathExpressionImpl()
{
	m_ast->deref();
	m_scope->deref();
}

XPathResultImpl *XPathExpressionImpl::evaluate(NodeImpl * /*contextNode*/,
											   unsigned short /*type*/, void * /*result*/)
{
 	NodeFilterImpl *filter = new NodeFilterImpl(0);

	DocumentImpl* d = scope()->ownerDocument();
	TreeWalkerImpl *walker = d->createTreeWalker(d, SHOW_ELEMENT, filter, true);

	NodeImpl *node = walker->nextNode();
	NodeSetImpl *nodeset = 0;

	while(node)
	{
		nodeset->append(node);
		node = walker->nextNode();
	}

	// TODO here's a nodeset..

	return 0;
}

ExprNodeImpl *XPathExpressionImpl::ast() const
{
	return m_ast;
}

QString XPathExpressionImpl::dump() const
{
	return XPathHelper::dumpTree(m_ast);
}

ScopeImpl *XPathExpressionImpl::scope() const
{
	return m_scope;
}

// vim:ts=4:noet
