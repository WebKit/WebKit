/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * (C) 2001 Peter Kelly (pmk@post.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "dom/dom_exception.h"
#include "xml/dom_docimpl.h"

using namespace DOM;

NodeIteratorImpl::NodeIteratorImpl(NodeImpl *_root, unsigned long _whatToShow,
				   NodeFilter _filter, bool _entityReferenceExpansion)
{
    m_root = _root;
    m_whatToShow = _whatToShow;
    m_filter = _filter;
    m_expandEntityReferences = _entityReferenceExpansion;

    m_referenceNode = _root;
    m_inFront = false;

    m_doc = m_root->getDocument();
    m_doc->attachNodeIterator(this);
    m_doc->ref();

    m_detached = false;
}

NodeIteratorImpl::~NodeIteratorImpl()
{
    m_doc->detachNodeIterator(this);
    m_doc->deref();
}

NodeImpl *NodeIteratorImpl::root()
{
    return m_root;
}

unsigned long NodeIteratorImpl::whatToShow()
{
    return m_whatToShow;
}

NodeFilter NodeIteratorImpl::filter()
{
    return m_filter;
}

bool NodeIteratorImpl::expandEntityReferences()
{
    return m_expandEntityReferences;
}

NodeImpl *NodeIteratorImpl::nextNode( int &exceptioncode )
{
    if (m_detached) {
	exceptioncode = DOMException::INVALID_STATE_ERR;
	return 0;
    }

    if (!m_referenceNode) {
	m_inFront = true;
	return 0;
    }

    if (!m_inFront) {
	m_inFront = true;
	if (isAccepted(m_referenceNode) == NodeFilter::FILTER_ACCEPT)
	    return m_referenceNode;
    }

    NodeImpl *_tempCurrent = getNextNode(m_referenceNode);
    while( _tempCurrent ) {
	m_referenceNode = _tempCurrent;
	if(isAccepted(_tempCurrent) == NodeFilter::FILTER_ACCEPT)
	    return m_referenceNode;
      _tempCurrent = getNextNode(_tempCurrent);
    }

    return 0;
}

NodeImpl *NodeIteratorImpl::getNextNode(NodeImpl *n)
{
  /*  1. my first child
   *  2. my next sibling
   *  3. my parents sibling, or their parents sibling (loop)
   *  4. not found
   */

  if( !n )
    return 0;

  if( n->hasChildNodes() )
    return n->firstChild();

  if( n->nextSibling() )
    return n->nextSibling();

  if( m_root == n)
     return 0;

  NodeImpl *parent = n->parentNode();
  while( parent )
    {
      n = parent->nextSibling();
      if( n )
        return n;

      if( m_root == parent )
        return 0;

      parent = parent->parentNode();
    }
  return 0;
}

NodeImpl *NodeIteratorImpl::previousNode( int &exceptioncode )
{
    if (m_detached) {
	exceptioncode = DOMException::INVALID_STATE_ERR;
	return 0;
    }

    if (!m_referenceNode) {
	m_inFront = false;
	return 0;
    }

    if (m_inFront) {
	m_inFront = false;
	if (isAccepted(m_referenceNode) == NodeFilter::FILTER_ACCEPT)
	    return m_referenceNode;
    }

    NodeImpl *_tempCurrent = getPreviousNode(m_referenceNode);
    while( _tempCurrent ) {
	m_referenceNode = _tempCurrent;
	if(isAccepted(_tempCurrent) == NodeFilter::FILTER_ACCEPT)
	    return m_referenceNode;
	_tempCurrent = getPreviousNode(_tempCurrent);
    }

    return 0;
}

NodeImpl *NodeIteratorImpl::getPreviousNode(NodeImpl *n)
{
/* 1. my previous sibling.lastchild
 * 2. my previous sibling
 * 3. my parent
 */
  NodeImpl *_tempCurrent;

  if( !n )
    return 0;

  _tempCurrent = n->previousSibling();
  if( _tempCurrent )
    {
      if( _tempCurrent->lastChild() )
        {
          while( _tempCurrent->lastChild() )
            _tempCurrent = _tempCurrent->lastChild();
          return _tempCurrent;
        }
      else
        return _tempCurrent;
    }


  if(n == m_root)
    return 0;

  return n->parentNode();


}

void NodeIteratorImpl::detach(int &/*exceptioncode*/)
{
    m_doc->detachNodeIterator(this);
    m_detached = true;
}


void NodeIteratorImpl::notifyBeforeNodeRemoval(NodeImpl *removed)
{
    // make sure the deleted node is with the root (but not the root itself)
    if (removed == m_root)
	return;

    NodeImpl *maybeRoot = removed->parentNode();
    while (maybeRoot && maybeRoot != m_root)
	maybeRoot = maybeRoot->parentNode();
    if (!maybeRoot)
	return;

    // did I get deleted, or one of my parents?
    NodeImpl *_tempDeleted = m_referenceNode;
    while( _tempDeleted && _tempDeleted != removed)
        _tempDeleted = _tempDeleted->parentNode();

    if( !_tempDeleted )  // someone that didn't consern me got deleted
        return;

    if( !m_inFront)
    {
        NodeImpl *_next = getNextNode(_tempDeleted);
        if( _next )
            m_referenceNode = _next;
        else
        {
	    // deleted node was at end of list
            m_inFront = true;
            m_referenceNode = getPreviousNode(_tempDeleted);
        }
    }
    else {
	NodeImpl *_prev = getPreviousNode(_tempDeleted);
	if ( _prev )
	    m_referenceNode = _prev;
	else
	{
	    // deleted node was at start of list
	    m_inFront = false;
	    m_referenceNode = getNextNode(_tempDeleted);
	}
    }

}

short NodeIteratorImpl::isAccepted(NodeImpl *n)
{
  // if XML is implemented we have to check expandEntityRerefences in this function
  if( ( ( 1 << n->nodeType()-1) & m_whatToShow) != 0 )
    {
        if(!m_filter.isNull())
            return m_filter.acceptNode(n);
        else
	    return NodeFilter::FILTER_ACCEPT;
    }
    return NodeFilter::FILTER_SKIP;
}

// --------------------------------------------------------------


NodeFilterImpl::NodeFilterImpl()
{
    m_customNodeFilter = 0;
}

NodeFilterImpl::~NodeFilterImpl()
{
    if (m_customNodeFilter)
	m_customNodeFilter->deref();
}

short NodeFilterImpl::acceptNode(const Node &n)
{
    if (m_customNodeFilter)
	return m_customNodeFilter->acceptNode(n);
    else
	return NodeFilter::FILTER_ACCEPT;
}

void NodeFilterImpl::setCustomNodeFilter(CustomNodeFilter *custom)
{
    m_customNodeFilter = custom;
    if (m_customNodeFilter)
	m_customNodeFilter->ref();
}

CustomNodeFilter *NodeFilterImpl::customNodeFilter()
{
    return m_customNodeFilter;
}

// --------------------------------------------------------------

TreeWalkerImpl::TreeWalkerImpl()
{
    m_filter = 0;
    m_whatToShow = 0x0000FFFF;
    m_expandEntityReferences = true;
}

TreeWalkerImpl::TreeWalkerImpl(const TreeWalkerImpl &other)
    : khtml::Shared<TreeWalkerImpl>()
{
    m_expandEntityReferences = other.m_expandEntityReferences;
    m_filter = other.m_filter;
    m_whatToShow = other.m_whatToShow;
    m_currentNode = other.m_currentNode;
    m_rootNode = other.m_rootNode;
}

TreeWalkerImpl::TreeWalkerImpl(Node n, NodeFilter *f)
{
  m_currentNode = n;
  m_rootNode = n;
  m_whatToShow = 0x0000FFFF;
  m_filter = f;
}

TreeWalkerImpl::TreeWalkerImpl(Node n, long _whatToShow, NodeFilter *f)
{
  m_currentNode = n;
  m_rootNode = n;
  m_whatToShow = _whatToShow;
  m_filter = f;
}

TreeWalkerImpl &TreeWalkerImpl::operator = (const TreeWalkerImpl &other)
{
  m_expandEntityReferences = other.m_expandEntityReferences;
  m_filter = other.m_filter;
  m_whatToShow = other.m_whatToShow;
  m_currentNode = other.m_currentNode;
  return *this;
}

TreeWalkerImpl::~TreeWalkerImpl()
{
    if(m_filter)
      {
        delete m_filter;
        m_filter = 0;
      }
}





Node TreeWalkerImpl::getRoot()
{
    // ###
    return 0;
}

unsigned long TreeWalkerImpl::getWhatToShow()
{
    // ###
    return 0;
}

NodeFilter TreeWalkerImpl::getFilter()
{
    // ###
    return 0;
}

bool TreeWalkerImpl::getExpandEntityReferences()
{
    // ###
    return 0;
}

Node TreeWalkerImpl::getCurrentNode()
{
    return m_currentNode;
}

void TreeWalkerImpl::setWhatToShow(long _whatToShow)
{
  // do some testing wether this is an accepted value
  m_whatToShow = _whatToShow;
}

void TreeWalkerImpl::setFilter(NodeFilter *_filter)
{
  // ### allow setting of filter to 0?
  if(_filter)
    m_filter = _filter;
}

void TreeWalkerImpl::setExpandEntityReferences(bool value)
{
  m_expandEntityReferences = value;
}

void TreeWalkerImpl::setCurrentNode( const Node n )
{
    if( !n.isNull() )
    {
        m_rootNode = n;
        m_currentNode = n;
    }
//     else
//         throw( DOMException::NOT_SUPPORTED_ERR );
}

Node TreeWalkerImpl::parentNode(  )
{
    Node n = getParentNode(m_currentNode);
    if( !n.isNull() )
        m_currentNode = n;
    return n;
}


Node TreeWalkerImpl::firstChild(  )
{
    Node n = getFirstChild(m_currentNode);
    if( !n.isNull() )
        m_currentNode = n;
    return n;
}


Node TreeWalkerImpl::lastChild(  )
{
    Node n = getLastChild(m_currentNode);
    if( !n.isNull() )
        m_currentNode = n;
    return n;
}

Node TreeWalkerImpl::previousSibling(  )
{
    Node n = getPreviousSibling(m_currentNode);
    if( !n.isNull() )
        m_currentNode = n;
    return n;
}

Node TreeWalkerImpl::nextSibling(  )
{
    Node n = getNextSibling(m_currentNode);
    if( !n.isNull() )
        m_currentNode = n;
    return n;
}

Node TreeWalkerImpl::previousNode(  )
{
/* 1. my previous sibling.lastchild
 * 2. my previous sibling
 * 3. my parent
 */

    Node n = getPreviousSibling(m_currentNode);
    if( n.isNull() )
    {
        n = getParentNode(m_currentNode);
        if( !n.isNull() )      //parent
        {
            m_currentNode = n;
            return m_currentNode;
        }
        else                  // parent failed.. no previous node
            return Node();
    }

    Node child = getLastChild(n);
    if( !child.isNull() )     // previous siblings last child
    {
        m_currentNode = child;
        return m_currentNode;
    }
    else                      // previous sibling
    {
        m_currentNode = n;
        return m_currentNode;
    }
    return Node();            // should never get here!
}

Node TreeWalkerImpl::nextNode(  )
{
/*  1. my first child
 *  2. my next sibling
 *  3. my parents sibling, or their parents sibling (loop)
 *  4. not found
 */

    Node n = getFirstChild(m_currentNode);
    if( !n.isNull()  ) // my first child
    {
        m_currentNode = n;
        return n;
    }

    n = getNextSibling(m_currentNode); // my next sibling
    if( !n.isNull() )
    {
        m_currentNode = n;
        return m_currentNode;
    }
    Node parent = getParentNode(m_currentNode);
    while( !parent.isNull() ) // parents sibling
    {
        n = getNextSibling(parent);
        if( !n.isNull() )
        {
          m_currentNode = n;
          return m_currentNode;
        }
        else
            parent = getParentNode(parent);
    }
    return Node();
}

short TreeWalkerImpl::isAccepted(Node n)
{
    // if XML is implemented we have to check expandEntityRerefences in this function
  if( ( ( 1 << n.nodeType()-1 ) & m_whatToShow) != 0 )
    {
      if(m_filter)
        return m_filter->acceptNode(n);
      else
        return NodeFilter::FILTER_ACCEPT;
    }
  return NodeFilter::FILTER_SKIP;
}

Node TreeWalkerImpl::getParentNode(Node n)
{
     short _result = NodeFilter::FILTER_ACCEPT;

    if( n == m_rootNode /*|| n.isNull()*/ )
      return Node();

    Node _tempCurrent = n.parentNode();

    if( _tempCurrent.isNull() )
      return Node();

    _result = isAccepted(_tempCurrent );
    if(_result == NodeFilter::FILTER_ACCEPT)
      return _tempCurrent;       // match found

    return getParentNode(_tempCurrent);
}

Node TreeWalkerImpl::getFirstChild(Node n)
{
    short _result;

    if( n.isNull() || n.firstChild().isNull() )
        return Node();
    n = n.firstChild();

    _result = isAccepted(n);

    switch(_result)
    {
         case NodeFilter::FILTER_ACCEPT:
           return n;
           break;
        case NodeFilter::FILTER_SKIP:
          if( n.hasChildNodes() )
                return getFirstChild(n);
            else
                return getNextSibling(n);
            break;

        case NodeFilter::FILTER_REJECT:
            return getNextSibling(n);
            break;
    }
    return Node();      // should never get here!
}

Node TreeWalkerImpl::getLastChild(Node n)
{
    short _result;

    if( n.isNull() || n.lastChild().isNull() )
        return Node();
    n = n.lastChild();
    _result = isAccepted(n);
    switch(_result)
    {
        case NodeFilter::FILTER_ACCEPT:
            return n;
            break;

        case NodeFilter::FILTER_SKIP:
            if( n.hasChildNodes() )
                return getLastChild(n);
            else
                return getPreviousSibling(n);
            break;

        case NodeFilter::FILTER_REJECT:
            return getPreviousSibling(n);
            break;
    }
    return Node();
}

Node TreeWalkerImpl::getPreviousSibling(Node n)
{
    short _result;
    Node _tempCurrent;

    if( n.isNull() )
        return Node();
    //first the cases if we have a previousSibling
    _tempCurrent = n.previousSibling();
    if( !_tempCurrent.isNull() )
    {
        _result = isAccepted(_tempCurrent);
        switch(_result)
        {
            case NodeFilter::FILTER_ACCEPT:
                return _tempCurrent;
                break;

            case NodeFilter::FILTER_SKIP:
            {
                Node nskip = getLastChild(_tempCurrent);
                if( !nskip.isNull() )
                    return nskip;
                return getPreviousSibling(_tempCurrent);
                break;
            }

            case NodeFilter::FILTER_REJECT:
                return getPreviousSibling(_tempCurrent);
                break;
        }
    }
    // now the case if we don't have previous sibling
    else
    {
        _tempCurrent = _tempCurrent.parentNode();
        if(_tempCurrent.isNull() || _tempCurrent == m_rootNode)
            return Node();
        _result = isAccepted(_tempCurrent);
        if(_result == NodeFilter::FILTER_SKIP)
            return getPreviousSibling(_tempCurrent);

        return Node();

    }
    return Node();  // should never get here!
}

Node TreeWalkerImpl::getNextSibling(Node n)
{
    Node _tempCurrent;
    short _result;

    if( n.isNull() || _tempCurrent == m_rootNode)
        return Node();

    _tempCurrent = n.nextSibling();
    if( !_tempCurrent.isNull() )
    {
        _result = isAccepted(_tempCurrent);
        switch(_result)
        {
            case NodeFilter::FILTER_ACCEPT:
                return _tempCurrent;
                break;

            case NodeFilter::FILTER_SKIP:
            {
                Node nskip = getFirstChild(_tempCurrent);
                if( !nskip.isNull() )
                    return nskip;
                return getNextSibling(_tempCurrent);
                break;
            }

            case NodeFilter::FILTER_REJECT:
                return getNextSibling(_tempCurrent);
                break;
        }
    }
    else
    {
        _tempCurrent = _tempCurrent.parentNode();
        if(_tempCurrent.isNull() || _tempCurrent == m_rootNode)
            return Node();
        _result = isAccepted(_tempCurrent);
        if(_result == NodeFilter::FILTER_SKIP)
            return getNextSibling(_tempCurrent);

        return Node();
    }
    return Node();
}

