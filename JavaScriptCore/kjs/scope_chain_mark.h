/*
 *  Copyright (C) 2003, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef scope_chain_mark_h
#define scope_chain_mark_h

#include "Activation.h"
#include "scope_chain.h"

namespace KJS {

    inline void ScopeChain::mark()
    {
        for (ScopeChainNode* n = _node; n; n = n->next) {
            JSObject* o = n->object;
            
            // An ActivationImp that is on the activation stack can't have the
            // JSObject::marked() method called on it, because it doesn't have an
            // entry in a GC mark bitmap, so we check here whether it is on the
            // stack and directly call the portion of the marking code that is
            // still relevant.
            
            if (o->isActivationObject() && static_cast<ActivationImp*>(o)->isOnStack())
                static_cast<ActivationImp*>(o)->markChildren();
            else if (!o->marked())
                o->mark();
        }
    }

} // namespace KJS

#endif
