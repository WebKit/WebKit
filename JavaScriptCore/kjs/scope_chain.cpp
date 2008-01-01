/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003, 2006 Apple Inc.
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
#include "PropertyNameArray.h"
#include <stdio.h>
#include "object.h"

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

void ScopeChain::print()
{
    ScopeChainIterator scopeEnd = end();
    for (ScopeChainIterator scopeIter = begin(); scopeIter != scopeEnd; ++scopeIter) {
        JSObject* o = *scopeIter;
        PropertyNameArray propertyNames;
        // FIXME: should pass ExecState here!
        o->getPropertyNames(0, propertyNames);
        PropertyNameArray::const_iterator propEnd = propertyNames.end();

        fprintf(stderr, "----- [scope %p] -----\n", o);
        for (PropertyNameArray::const_iterator propIter = propertyNames.begin(); propIter != propEnd; propIter++) {
            Identifier name = *propIter;
            fprintf(stderr, "%s, ", name.ascii());
        }
        fprintf(stderr, "\n");
    }
}

#endif

} // namespace KJS
