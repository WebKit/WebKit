/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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

#include "config.h"
#include "CommentImpl.h"

#include "AtomicString.h"
#include "DocumentImpl.h"
#include "dom_node.h"

namespace WebCore {

CommentImpl::CommentImpl(DocumentImpl *doc, const DOMString &_text)
    : CharacterDataImpl(doc, _text)
{
}

CommentImpl::CommentImpl(DocumentImpl *doc)
    : CharacterDataImpl(doc)
{
}

CommentImpl::~CommentImpl()
{
}

const AtomicString& CommentImpl::localName() const
{
    return commentAtom;
}

DOMString CommentImpl::nodeName() const
{
    return commentAtom.domString();
}

unsigned short CommentImpl::nodeType() const
{
    return Node::COMMENT_NODE;
}

PassRefPtr<NodeImpl> CommentImpl::cloneNode(bool /*deep*/)
{
    return getDocument()->createComment( str );
}

// DOM Section 1.1.1
bool CommentImpl::childTypeAllowed( unsigned short /*type*/ )
{
    return false;
}

DOMString CommentImpl::toString() const
{
    // FIXME: substitute entity references as needed!
    return DOMString("<!--") + nodeValue() + "-->";
}

} // namespace WebCore
