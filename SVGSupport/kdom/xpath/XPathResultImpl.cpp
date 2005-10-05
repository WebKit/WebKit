/*
 * XPathResultImpl.cpp - Copyright 2005 Frerich Raabe <raabe@kde.org>
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
#include "config.h"
#include "XPathResultImpl.h"
#include "XPathExceptionImpl.h"

#include "kdomxpath.h"

#include "DOMStringImpl.h"
#include "NodeImpl.h"

using namespace KDOM;
using namespace KDOM::XPath;

XPathResultImpl::XPathResultImpl()
{
}

XPathResultImpl::XPathResultImpl( const Value &value )
    : m_value( value )
{
    switch ( m_value.type() ) {
        case Value::Boolean:
            m_resultType = BOOLEAN_TYPE;
            break;
        case Value::Number:
            m_resultType = NUMBER_TYPE;
            break;
        case Value::String:
            m_resultType = STRING_TYPE;
            break;
        case Value::Nodeset:
            m_resultType = UNORDERED_NODE_ITERATOR_TYPE;
            m_nodeIterator = m_value.toNodeset().begin();
    }
}

void XPathResultImpl::convertTo( unsigned short type )
{
    switch ( type ) {
        case ANY_TYPE:
            break;
        case NUMBER_TYPE:
            m_resultType = type;
            m_value = Value( m_value.toNumber() );
            break;
        case STRING_TYPE:
            m_resultType = type;
            m_value = Value( m_value.toString() );
            break;
        case BOOLEAN_TYPE:
            m_resultType = type;
            m_value = Value( m_value.toBoolean() );
            break;
        case UNORDERED_NODE_ITERATOR_TYPE:
        case ORDERED_NODE_ITERATOR_TYPE:
        case UNORDERED_NODE_SNAPSHOT_TYPE:
        case ORDERED_NODE_SNAPSHOT_TYPE:
        case ANY_UNORDERED_NODE_TYPE:
        case FIRST_ORDERED_NODE_TYPE:
            if ( !m_value.isNodeset() ) {
                throw new XPathExceptionImpl( TYPE_ERR );
                return;
            }
            m_resultType = type;
        default:
            qWarning( "Cannot convert XPathResultImpl to unknown type '%u'!", type );
    }
}

unsigned short XPathResultImpl::resultType() const
{
    return m_resultType;
}

double XPathResultImpl::numberValue() const
{
    if ( resultType() != NUMBER_TYPE ) {
        throw new XPathExceptionImpl( TYPE_ERR );
    }
    return m_value.toNumber();
}

DOMStringImpl *XPathResultImpl::stringValue() const
{
    if ( resultType() != STRING_TYPE ) {
        throw new XPathExceptionImpl( TYPE_ERR );
    }
    return new DOMStringImpl( m_value.toString() );
}

bool XPathResultImpl::booleanValue() const
{
    if ( resultType() != BOOLEAN_TYPE ) {
        throw new XPathExceptionImpl( TYPE_ERR );
    }
    return m_value.toBoolean();
}

NodeImpl *XPathResultImpl::singleNodeValue() const
{
    if ( resultType() != ANY_UNORDERED_NODE_TYPE &&
         resultType() != FIRST_ORDERED_NODE_TYPE ) {
        throw new XPathExceptionImpl( TYPE_ERR );
    }
    return 0;
}

bool XPathResultImpl::invalidIteratorState() const
{
    if ( resultType() != UNORDERED_NODE_ITERATOR_TYPE &&
         resultType() != ORDERED_NODE_ITERATOR_TYPE ) {
        return false;
    }
    // XXX How to tell whether the document was changed since this
    // result was returned?
    return true;
}

unsigned long XPathResultImpl::snapshotLength() const
{
    if ( resultType() != UNORDERED_NODE_SNAPSHOT_TYPE &&
         resultType() != ORDERED_NODE_SNAPSHOT_TYPE ) {
        throw new XPathExceptionImpl( TYPE_ERR );
    }
    return m_value.toNodeset().count();
}

NodeImpl *XPathResultImpl::iterateNext()
{
    if ( resultType() != UNORDERED_NODE_ITERATOR_TYPE &&
         resultType() != ORDERED_NODE_ITERATOR_TYPE ) {
        throw new XPathExceptionImpl( TYPE_ERR );
    }
    // XXX How to tell whether the document was changed since this
    // result was returned? We need to throw an INVALID_STATE_ERR if that
    // is the case.
    if ( m_nodeIterator == m_value.toNodeset().end() ) {
        return 0;
    }
    return *m_nodeIterator++;
}

NodeImpl *XPathResultImpl::snapshotItem( unsigned long index )
{
    if ( resultType() != UNORDERED_NODE_SNAPSHOT_TYPE &&
         resultType() != ORDERED_NODE_SNAPSHOT_TYPE ) {
        throw new XPathExceptionImpl( TYPE_ERR );
    }
    return m_value.toNodeset()[index];
}

