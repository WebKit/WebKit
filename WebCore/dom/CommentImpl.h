/*
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
 *
 */

#ifndef DOM_CommentImpl_h
#define DOM_CommentImpl_h

#include "CharacterDataImpl.h"

namespace WebCore {

class CommentImpl : public CharacterDataImpl
{
public:
    CommentImpl(DocumentImpl*, const DOMString &_text);
    CommentImpl(DocumentImpl*);
    virtual ~CommentImpl();

    // DOM methods overridden from  parent classes
    const AtomicString& localName() const;
    virtual DOMString nodeName() const;
    virtual unsigned short nodeType() const;
    virtual PassRefPtr<NodeImpl> cloneNode(bool deep);

    // Other methods (not part of DOM)
    virtual bool isCommentNode() const { return true; }
    virtual bool childTypeAllowed( unsigned short type );

    virtual DOMString toString() const;
};

} // namespace WebCore

#endif // DOM_CommentImpl_h
