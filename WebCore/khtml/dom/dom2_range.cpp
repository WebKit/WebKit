/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Gunnstein Lye (gunnstein@netcom.no)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2004 Apple Computer, Inc.
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
 *
 */

#include "dom/dom_exception.h"

#include "xml/dom_docimpl.h"
#include "xml/dom2_rangeimpl.h"

namespace DOM {

Range::Range()
{
    // a range can't exist by itself - it must be associated with a document
    impl = 0;
}

Range::Range(const Document rootContainer)
{
    if(rootContainer.handle())
    {
	impl = new RangeImpl(rootContainer.handle()->docPtr());
	impl->ref();
    }
    else
	impl = 0;
}

Range::Range(const Range &other)
{
    impl = other.impl;
    if (impl) impl->ref();
}

Range::Range(const Node startContainer, const long startOffset, const Node endContainer, const long endOffset)
{
    if (startContainer.isNull() || endContainer.isNull()) {
        throw DOMException(DOMException::NOT_FOUND_ERR);
    }

    if (!startContainer.handle()->getDocument() ||
        startContainer.handle()->getDocument() != endContainer.handle()->getDocument()) {
        throw DOMException(DOMException::WRONG_DOCUMENT_ERR);
    }

    impl = new RangeImpl(startContainer.handle()->docPtr(),startContainer.handle(),startOffset,endContainer.handle(),endOffset);
    impl->ref();
}

Range::Range(RangeImpl *i)
{
    impl = i;
    if (impl) impl->ref();
}

Range &Range::operator = (const Range &other)
{
    if ( impl != other.impl ) {
    if (impl) impl->deref();
    impl = other.impl;
    if (impl) impl->ref();
    }
    return *this;
}

Range::~Range()
{
    if (impl) impl->deref();
}

Node Range::startContainer() const
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    NodeImpl *r = impl->startContainer(exceptioncode);
    throwException(exceptioncode);
    return r;
}

long Range::startOffset() const
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    long r = impl->startOffset(exceptioncode);
    throwException(exceptioncode);
    return r;

}

Node Range::endContainer() const
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    NodeImpl *r = impl->endContainer(exceptioncode);
    throwException(exceptioncode);
    return r;
}

long Range::endOffset() const
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    long r = impl->endOffset(exceptioncode);
    throwException(exceptioncode);
    return r;
}

bool Range::collapsed() const
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    bool r = impl->collapsed(exceptioncode);
    throwException(exceptioncode);
    return r;
}

Node Range::commonAncestorContainer()
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    NodeImpl *r = impl->commonAncestorContainer(exceptioncode);
    throwException(exceptioncode);
    return r;
}

void Range::setStart( const Node &refNode, long offset )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    impl->setStart(refNode.handle(),offset,exceptioncode);
    throwException(exceptioncode);
}

void Range::setEnd( const Node &refNode, long offset )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    impl->setEnd(refNode.handle(),offset,exceptioncode);
    throwException(exceptioncode);
}

void Range::setStartBefore( const Node &refNode )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);


    int exceptioncode = 0;
    impl->setStartBefore(refNode.handle(),exceptioncode);
    throwException(exceptioncode);
}

void Range::setStartAfter( const Node &refNode )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    impl->setStartAfter(refNode.handle(),exceptioncode);
    throwException(exceptioncode);
}

void Range::setEndBefore( const Node &refNode )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    impl->setEndBefore(refNode.handle(),exceptioncode);
    throwException(exceptioncode);
}

void Range::setEndAfter( const Node &refNode )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    impl->setEndAfter(refNode.handle(),exceptioncode);
    throwException(exceptioncode);
}

void Range::collapse( bool toStart )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    impl->collapse(toStart,exceptioncode);
    throwException(exceptioncode);
}

void Range::selectNode( const Node &refNode )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    impl->selectNode(refNode.handle(),exceptioncode);
    throwException(exceptioncode);
}

void Range::selectNodeContents( const Node &refNode )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    impl->selectNodeContents(refNode.handle(),exceptioncode);
    throwException(exceptioncode);
}

short Range::compareBoundaryPoints( CompareHow how, const Range &sourceRange )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    short r = impl->compareBoundaryPoints(how,sourceRange.handle(),exceptioncode);
    throwException(exceptioncode);
    return r;
}

bool Range::boundaryPointsValid(  )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    return impl->boundaryPointsValid();
}

void Range::deleteContents(  )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    impl->deleteContents(exceptioncode);
    throwException(exceptioncode);
}

DocumentFragment Range::extractContents(  )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    DocumentFragmentImpl *r = impl->extractContents(exceptioncode);
    throwException(exceptioncode);
    return r;
}

DocumentFragment Range::cloneContents(  )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    DocumentFragmentImpl *r = impl->cloneContents(exceptioncode);
    throwException(exceptioncode);
    return r;
}

void Range::insertNode( const Node &newNode )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    impl->insertNode(newNode.handle(),exceptioncode);
    throwException(exceptioncode);
}

void Range::surroundContents( const Node &newParent )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    impl->surroundContents(newParent.handle(),exceptioncode);
    throwException(exceptioncode);
}

Range Range::cloneRange(  )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    RangeImpl *r = impl->cloneRange(exceptioncode);
    throwException(exceptioncode);
    return r;
}

DOMString Range::toString(  )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    DOMString r = impl->toString(exceptioncode);
    throwException(exceptioncode);
    return r;

}

DOMString Range::toHTML(  )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    return impl->toHTML();
}

DocumentFragment Range::createContextualFragment( DOMString &html )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    DocumentFragmentImpl *r = impl->createContextualFragment(html, exceptioncode);
    throwException(exceptioncode);
    return r;
}


void Range::detach(  )
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    impl->detach(exceptioncode);
    throwException(exceptioncode);
}

bool Range::isDetached() const
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    return impl->isDetached();
}

RangeImpl *Range::handle() const
{
    return impl;
}

bool Range::isNull() const
{
    return (impl == 0);
}

void Range::throwException(int exceptioncode) const
{
    if (!exceptioncode)
        return;

    // ### also check for CSS & other exceptions?
    if (exceptioncode >= RangeException::_EXCEPTION_OFFSET && exceptioncode <= RangeException::_EXCEPTION_MAX)
        throw RangeException(static_cast<RangeException::RangeExceptionCode>(exceptioncode-RangeException::_EXCEPTION_OFFSET));
    else
        throw DOMException(exceptioncode);
}

}
