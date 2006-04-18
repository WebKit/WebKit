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
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "scope_chain.h"
#include "reference_list.h"

namespace KJS {

void ScopeChain::push(const ScopeChain &c)
{
    ScopeChainNode **tail = &_node;
    for (ScopeChainNode *n = c._node; n; n = n->next) {
        ScopeChainNode *newNode = new ScopeChainNode(*tail, n->object);
        *tail = newNode;
        tail = &newNode->next;
    }
}

#ifndef NDEBUG

void ScopeChain::print(ExecState* exec)
{
    ScopeChainIterator scopeEnd = end();
    for (ScopeChainIterator scopeIter = begin(); scopeIter != scopeEnd; ++scopeIter) {
        JSObject* o = *scopeIter;
        ReferenceList propList = o->propList(exec, false);
        ReferenceListIterator propEnd = propList.end();

        fprintf(stderr, "----- [scope %p] -----\n", o);
        for (ReferenceListIterator propIter = propList.begin(); propIter != propEnd; propIter++) {
            Identifier name = propIter->getPropertyName(exec);
            fprintf(stderr, "%s, ", name.ascii());
        }
        fprintf(stderr, "\n");
    }
}

#endif

} // namespace KJS
