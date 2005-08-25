/*
 * step.cc - Copyright 2005 Frerich Raabe <raabe@kde.org>
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
#include "step.h"

#include "DOMStringImpl.h"
#include "NamedAttrMapImpl.h"
#include "NodeImpl.h"
#include "kdom.h"
#include "kdomxpath.h"

using namespace KDOM;

QString Step::axisAsString( AxisType axis )
{
	switch ( axis ) {
		case AncestorAxis: return "ancestor";
		case AncestorOrSelfAxis: return "ancestor-or-self";
		case AttributeAxis: return "attribute";
		case ChildAxis: return "child";
		case DescendantAxis: return "descendant";
		case DescendantOrSelfAxis: return "descendant-or-self";
		case FollowingAxis: return "following";
		case FollowingSiblingAxis: return "following-sibling";
		case NamespaceAxis: return "namespace";
		case ParentAxis: return "parent";
		case PrecedingAxis: return "preceding";
		case PrecedingSiblingAxis: return "preceding-sibling";
		case SelfAxis: return "self";
	}
	return QString();
}

Step::Step()
{
}

Step::Step( AxisType axis, const DomString &nodeTest,
            const QValueList<Predicate *> &predicates )
	: m_axis( axis ),
	m_nodeTest( nodeTest ),
	m_predicates( predicates )
{
}

Step::~Step()
{
	QValueList<Predicate *>::Iterator it = m_predicates.begin();
	QValueList<Predicate *>::Iterator end = m_predicates.end();
	for ( ; it != end; ++it ) {
		delete *it;
	}
}

DomNodeList Step::evaluate( NodeImpl *context ) const
{
	qDebug( "Evaluating step, axis='%s', nodetest='%s', %i predicates.",
	        axisAsString( m_axis ).latin1(), m_nodeTest.latin1(), m_predicates.count() );
	if ( context->nodeType() == ELEMENT_NODE ) {
		qDebug( "Context node is an element." );
		qDebug( "Context node name: %s", context->nodeName()->string().latin1() );
	} else if ( context->nodeType() == ATTRIBUTE_NODE ) {
		qDebug( "Context node is an attribute." );
		qDebug( "Context node name: %s; value = %s", context->nodeName()->string().latin1(), context->nodeValue()->string().latin1() );
	} else {
		qDebug( "Context node is of unknown type '%i'.", context->nodeType() );
	}

	DomNodeList nodes = nodesInAxis( context );
	qDebug( "\tAxis '%s' matches %i nodes.", axisAsString( m_axis ).latin1(), nodes.count() );
	nodes = nodeTestMatches( nodes );
	qDebug( "\tNodetest '%s' trims this number to %i.", m_nodeTest.latin1(), nodes.count() );

	QValueList<Predicate *>::ConstIterator it, end = m_predicates.end();
	for ( it = m_predicates.begin(); it != end; ++it ) {
		Expression::evaluationContext().size = nodes.count();
		Expression::evaluationContext().position = 1;
		Predicate *predicate = *it;
		DomNodeList::Iterator nIt = nodes.begin();
		DomNodeList::Iterator nEnd = nodes.end();
		while ( nIt != nEnd ) {
			Expression::evaluationContext().node = *nIt;
			EvaluationContext backupCtx = Expression::evaluationContext();
			if ( predicate->evaluate() ) {
				++nIt;
			} else {
				nIt = nodes.remove( nIt );
			}
			Expression::evaluationContext() = backupCtx;
			++Expression::evaluationContext().position;
		}
		qDebug( "\tPredicate trims this number to %i.", nodes.count() );
	}

	return nodes;
}

DomNodeList Step::nodesInAxis( NodeImpl *context ) const
{
	DomNodeList nodes;
	switch ( m_axis ) {
		case ChildAxis: {
			NodeImpl *n = context->firstChild();
			while ( n ) {
				nodes.append( n );
				n = n->nextSibling();
			}
			return nodes;
		}
		case DescendantAxis: 
			return getChildrenRecursively( context );
		case ParentAxis:
			nodes.append( context->parentNode() );
			return nodes;
		case AncestorAxis: {
			NodeImpl *n = context->parentNode();
			while ( n ) {
				nodes.append( n );
				n = n->parentNode();
			}
			return nodes;
		}
		case FollowingSiblingAxis: {
			if ( context->nodeType() == ATTRIBUTE_NODE ||
			     context->nodeType() == XPath::XPATH_NAMESPACE_NODE ) {
				return DomNodeList();
			}

			NodeImpl *n = context->nextSibling();
			while ( n ) {
				nodes.append( n );
				n = n->nextSibling();
			}
			return nodes;
		}
		case PrecedingSiblingAxis: {
			if ( context->nodeType() == ATTRIBUTE_NODE ||
			     context->nodeType() == XPath::XPATH_NAMESPACE_NODE ) {
				return DomNodeList();
			}

			NodeImpl *n = context->previousSibling();
			while ( n ) {
				nodes.append( n );
				n = n->previousSibling();
			}
			return nodes;
		}
		case FollowingAxis: {
			NodeImpl *p = context;
			while ( !isRootDomNode( p ) ) {
				NodeImpl *n = p->nextSibling();
				while ( n ) {
					nodes.append( n );
					DomNodeList childNodes = getChildrenRecursively( n );
					DomNodeList::ConstIterator it, end = childNodes.end();
					for ( it = childNodes.begin(); it != end; ++it ) {
						nodes.append( *it );
					}
					n = n->nextSibling();
				}
				p = p->parentNode();
			}
			return nodes;
		}
		case PrecedingAxis: {
			NodeImpl *p = context;
			while ( !isRootDomNode( p ) ) {
				NodeImpl *n = p->previousSibling();
				while ( n ) {
					nodes.append( n );
					DomNodeList childNodes = getChildrenRecursively( n );
					DomNodeList::ConstIterator it, end = childNodes.end();
					for ( it = childNodes.begin(); it != end; ++it ) {
						nodes.append( *it );
					}
					n = n->previousSibling();
				}
				p = p->parentNode();
			}
			return nodes;
		}
		case AttributeAxis: {
			if ( context->nodeType() != ELEMENT_NODE ) {
				return DomNodeList();
			}

			NodeImpl *n = context->firstChild();
			while ( n ) {
				if ( n->nodeType() == ATTRIBUTE_NODE ) {
					nodes.append( n );
				}
				n = n->nextSibling();
			}
			return nodes;
		}
		case NamespaceAxis: {
			if ( context->nodeType() != ELEMENT_NODE ) {
				return DomNodeList();
			}

			bool foundXmlNsNode = false;
			NodeImpl *node = context;
			while ( node ) {
				NamedAttrMapImpl *attrs = node->attributes();
				for ( unsigned long i = 0; i < attrs->length(); ++i ) {
					NodeImpl *n = attrs->item( i );
					if ( n->nodeName()->string().startsWith( "xmlns:" ) ) {
						nodes.append( n );
					} else if ( n->nodeName()->string() == "xmlns" &&
					            !foundXmlNsNode
					            ) {
						foundXmlNsNode = true;
						if ( !n->nodeValue()->string().isEmpty() ) {
							nodes.append( n );
						}
					}
				}
				node = node->parentNode();
			}
			return nodes;
		}
		case SelfAxis:
			nodes.append( context );
			return nodes;
		case DescendantOrSelfAxis: {
			nodes.append( context );
			DomNodeList childNodes = getChildrenRecursively( context );
			DomNodeList::ConstIterator it, end = childNodes.end();
			for ( it = childNodes.begin(); it != end; ++it ) {
				nodes.append( *it );
			}
			return nodes;
		}
		case AncestorOrSelfAxis: {
			nodes.append( context );
			NodeImpl *n = context->parentNode();
			while ( n ) {
				nodes.append( n );
				n = n->parentNode();
			}
			return nodes;
		}
		default:
			qDebug( "Unknown axis '%s' passed to nodesInAxis.",
			        axisAsString( m_axis ).latin1() );
	}

	return DomNodeList();
}

DomNodeList Step::nodeTestMatches( const DomNodeList &nodes_ ) const
{
	DomNodeList nodes = nodes_;
	if ( m_nodeTest == "*" ) {
		if ( m_axis == AttributeAxis ) {
			DomNodeList::Iterator it = nodes.begin();
			DomNodeList::Iterator end = nodes.end();
			while ( it != end ) {
				if ( ( *it )->nodeType() == ATTRIBUTE_NODE ) {
					it = nodes.remove( it );
				} else {
					++it;
				}
			}
		} else if ( m_axis == NamespaceAxis ) {
			return nodes;
		} else {
			DomNodeList::Iterator it = nodes.begin();
			DomNodeList::Iterator end = nodes.end();
			while ( it != end ) {
				if ( ( *it )->nodeType() == ELEMENT_NODE ) {
					it = nodes.remove( it );
				} else {
					++it;
				}
			}
		}
	} else if ( m_nodeTest == "text()" ) {
		DomNodeList::Iterator it = nodes.begin();
		DomNodeList::Iterator end = nodes.end();
		while ( it != end ) {
			if ( ( *it )->nodeType() == TEXT_NODE ) {
				it = nodes.remove( it );
			} else {
				++it;
			}
		}
	} else if ( m_nodeTest == "comment()" ) {
		DomNodeList::Iterator it = nodes.begin();
		DomNodeList::Iterator end = nodes.end();
		while ( it != end ) {
			if ( ( *it )->nodeType() == COMMENT_NODE ) {
				it = nodes.remove( it );
			} else {
				++it;
			}
		}
	} else if ( m_nodeTest.startsWith( "processing-instruction" ) ) {
		QString str = m_nodeTest;
		QString param = str.mid( str.find( ':' ) + 1 );
		if ( param.isEmpty() ) {
			DomNodeList::Iterator it = nodes.begin();
			DomNodeList::Iterator end = nodes.end();
			while ( it != end ) {
				if ( ( *it )->nodeType() == PROCESSING_INSTRUCTION_NODE ) {
					it = nodes.remove( it );
				} else {
					++it;
				}
			}
		} else {
			DomNodeList::Iterator it = nodes.begin();
			DomNodeList::Iterator end = nodes.end();
			while ( it != end ) {
				if ( ( *it )->nodeType() == PROCESSING_INSTRUCTION_NODE ||
				     ( *it )->nodeName()->string() != param ) {
					it = nodes.remove( it );
				} else {
					++it;
				}
			}
		}
	} else if ( m_nodeTest == "node()" ) {
		return nodes;
	} else {
		if ( m_axis == AttributeAxis ) {
			DomNodeList::Iterator it = nodes.begin();
			DomNodeList::Iterator end = nodes.end();
			while ( it != end ) {
				if ( ( *it )->nodeType() == ATTRIBUTE_NODE ||
				     ( *it )->nodeName()->string() != m_nodeTest ) {
					it = nodes.remove( it );
				} else {
					++it;
				}
			}
		} else if ( m_axis == NamespaceAxis ) {
			qWarning( "DomNodetest '%s' on axis 'namespace' is not implemented yet.", m_nodeTest.latin1() );
		} else {
			DomNodeList::Iterator it = nodes.begin();
			DomNodeList::Iterator end = nodes.end();
			while ( it != end ) {
				if ( ( *it )->nodeType() != ELEMENT_NODE ||
				     ( *it )->nodeName()->string() != m_nodeTest ) {
					it = nodes.remove( it );
				} else {
					++it;
				}
			}
		}
	}

	return nodes;
}

void Step::optimize()
{
	QValueList<Predicate *>::ConstIterator it, end = m_predicates.end();
	for ( it = m_predicates.begin(); it != end; ++it ) {
		( *it )->optimize();
	}
}

QString Step::dump() const
{
	QString s = QString( "<step axis=\"%1\" nodetest=\"%2\">" ).arg( axisAsString( m_axis ) ).arg( m_nodeTest ).local8Bit().data();
	QValueList<Predicate *>::ConstIterator it, end = m_predicates.end();
	for ( it = m_predicates.begin(); it != end; ++it ) {
		s += ( *it )->dump();
	}
	s += "</step>";
	return s;
}

