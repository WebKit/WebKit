/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2002 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */

#include "scope_chain.h"

#include "object.h"

namespace KJS {

ScopeChainNode::ScopeChainNode(ScopeChainNode *n, ObjectImp *o)
    : next(n), object(o), nodeAndObjectRefCount(1), nodeOnlyRefCount(0)
{
    o->ref();
}

inline void ScopeChain::ref() const
{
    for (ScopeChainNode *n = _node; n; n = n->next) {
        if (n->nodeAndObjectRefCount++ != 0)
            break;
        n->object->ref();
    }
}

ScopeChain::ScopeChain(const NoRefScopeChain &c)
    : _node(c._node)
{
    ref();
}

ScopeChain &ScopeChain::operator=(const ScopeChain &c)
{
    c.ref();
    deref();
    _node = c._node;
    return *this;
}

void ScopeChain::push(ObjectImp *o)
{
    _node = new ScopeChainNode(_node, o);
}

void ScopeChain::pop()
{
    ScopeChainNode *oldNode = _node;
    assert(oldNode);
    ScopeChainNode *newNode = oldNode->next;
    _node = newNode;
    
    // Three cases:
    //   1) This was not the last reference of the old node.
    //      In this case we move our ref from the old to the new node.
    //   2) This was the last reference of the old node, but there are garbage collected references.
    //      In this case, the new node doesn't get any new ref, and the object is deref'd.
    //   3) This was the last reference of the old node.
    //      In this case the object is deref'd and the entire node goes.
    if (--oldNode->nodeAndObjectRefCount != 0) {
        if (newNode)
            ++newNode->nodeAndObjectRefCount;
    } else {
        oldNode->object->deref();
        if (oldNode->nodeOnlyRefCount == 0)
            delete oldNode;
    }
}

void ScopeChain::release()
{
    ScopeChainNode *n = _node;
    do {
        ScopeChainNode *next = n->next;
        n->object->deref();
        if (n->nodeOnlyRefCount == 0)
            delete n;
        n = next;
    } while (n && --n->nodeAndObjectRefCount == 0);
}

inline void NoRefScopeChain::ref() const
{
    for (ScopeChainNode *n = _node; n; n = n->next)
        if (n->nodeOnlyRefCount++ != 0)
            break;
}

NoRefScopeChain &NoRefScopeChain::operator=(const ScopeChain &c)
{
    c.ref();
    deref();
    _node = c._node;
    return *this;
}

void NoRefScopeChain::mark()
{
    for (ScopeChainNode *n = _node; n; n = n->next) {
        ObjectImp *o = n->object;
        if (!o->marked())
            o->mark();
    }
}

void NoRefScopeChain::release()
{
    ScopeChainNode *n = _node;
    do {
        ScopeChainNode *next = n->next;
        if (n->nodeAndObjectRefCount == 0)
            delete n;
        n = next;
    } while (n && --n->nodeOnlyRefCount == 0);
}

} // namespace KJS
