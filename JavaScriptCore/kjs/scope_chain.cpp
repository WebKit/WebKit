/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003 Apple Computer, Inc.
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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "scope_chain.h"

#include "object.h"

namespace KJS {

inline void ScopeChain::ref() const
{
    for (ScopeChainNode *n = _node; n; n = n->next) {
        if (n->refCount++ != 0)
            break;
    }
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
    assert(o);
    _node = new ScopeChainNode(_node, o);
}

void ScopeChain::push(const ScopeChain &c)
{
    ScopeChainNode **tail = &_node;
    for (ScopeChainNode *n = c._node; n; n = n->next) {
        ScopeChainNode *newNode = new ScopeChainNode(*tail, n->object);
        *tail = newNode;
        tail = &newNode->next;
    }
}

void ScopeChain::pop()
{
    ScopeChainNode *oldNode = _node;
    assert(oldNode);
    ScopeChainNode *newNode = oldNode->next;
    _node = newNode;
    
    if (--oldNode->refCount != 0) {
        if (newNode)
            ++newNode->refCount;
    } else {
        delete oldNode;
    }
}

void ScopeChain::release()
{
    // This function is only called by deref(),
    // Deref ensures these conditions are true.
    assert(_node && _node->refCount == 0);
    ScopeChainNode *n = _node;
    do {
        ScopeChainNode *next = n->next;
        delete n;
        n = next;
    } while (n && --n->refCount == 0);
}

void ScopeChain::mark()
{
    for (ScopeChainNode *n = _node; n; n = n->next) {
        ObjectImp *o = n->object;
        if (!o->marked())
            o->mark();
    }
}

ObjectImp *ScopeChain::bottom() const
{
    ScopeChainNode *last = 0;
    for (ScopeChainNode *n = _node; n; n = n->next) {
	if (!n->next) {
	    last = n;
	}
    }
    if (!last) {
	return 0;
    }

    return last->object;
}

} // namespace KJS
