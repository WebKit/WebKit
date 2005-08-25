/*
 * path.cc - Copyright 2005 Frerich Raabe <raabe@kde.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "path.h"

#include "NodeImpl.h"

#include <qvaluelist.h>

using namespace KDOM;

Path::Path()
	: m_absolute( false )
{
}

Path::~Path()
{
	QValueList<Step *>::Iterator it, end = m_steps.end();
	for ( it = m_steps.begin(); it != end; ++it ) {
		delete *it;
	}
}

void Path::addStep( Step *step )
{
	m_steps.append( step );
}

void Path::optimize()
{
	QValueList<Step *>::Iterator it, end = m_steps.end();
	for ( it = m_steps.begin(); it != end; ++it ) {
		( *it )->optimize();
	}
}

Value Path::doEvaluate() const
{
	if ( m_absolute ) {
		qDebug( "Evaluating absolute path expression with %i location steps.", m_steps.count() );
	} else {
		qDebug( "Evaluating relative path expression with %i location steps.", m_steps.count() );
	}
	
	DomNodeList inDomNodes, outDomNodes;

	/* For absolute location paths, the context node is ignored - the
	 * document's root node is used instead.
	 */
	NodeImpl *context = Expression::evaluationContext().node;
	if ( m_absolute ) {
		while ( context->parentNode() ) {
			context = context->parentNode();
		}
	}

	inDomNodes.append( context );

	QValueList<Step *>::ConstIterator it = m_steps.begin();
	QValueList<Step *>::ConstIterator end = m_steps.end();
	for ( ; it != end; ++it ) {
		Step *step = *it;
		for ( unsigned int i = 0; i < inDomNodes.count(); ++i ) {
			DomNodeList matches = step->evaluate( inDomNodes[i] );
			DomNodeList::ConstIterator it, end = matches.end();
			for ( it = matches.begin(); it != end; ++it ) {
				outDomNodes.append( *it );
			}
		}
		inDomNodes = outDomNodes;
		outDomNodes.clear();
	}

	return Value( inDomNodes );
}

QString Path::dump() const
{
	QString s = "<path absolute=\"";
	s += m_absolute ? "true" : "false";
	s += "\">";
	QValueList<Step *>::ConstIterator it, end = m_steps.end();
	for ( it = m_steps.begin(); it != end; ++it ) {
		s += ( *it )->dump();
	}
	s += "</path>";
	return s;
}

