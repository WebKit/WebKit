/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef DFGPromoteHeapAccess_h
#define DFGPromoteHeapAccess_h

#if ENABLE(DFG_JIT)

#include "DFGNode.h"
#include "DFGPromotedHeapLocation.h"

namespace JSC { namespace DFG {

template<typename WriteFunctor, typename ReadFunctor>
void promoteHeapAccess(Node* node, const WriteFunctor& write, const ReadFunctor& read)
{
    switch (node->op()) {
    case CheckStructure: {
        if (node->child1()->isPhantomObjectAllocation())
            read(PromotedHeapLocation(StructurePLoc, node->child1()));
        break;
    }
    
    case GetByOffset:
    case GetGetterSetterByOffset: {
        if (node->child2()->isPhantomObjectAllocation()) {
            unsigned identifierNumber = node->storageAccessData().identifierNumber;
            read(PromotedHeapLocation(NamedPropertyPLoc, node->child2(), identifierNumber));
        }
        break;
    }

    case MultiGetByOffset: {
        if (node->child1()->isPhantomObjectAllocation()) {
            unsigned identifierNumber = node->multiGetByOffsetData().identifierNumber;
            read(PromotedHeapLocation(NamedPropertyPLoc, node->child1(), identifierNumber));
        }
        break;
    }

    case GetClosureVar:
        if (node->child1()->isPhantomActivationAllocation())
            read(PromotedHeapLocation(ClosureVarPLoc, node->child1(), node->scopeOffset().offset()));
        break;

    case SkipScope:
        if (node->child1()->isPhantomActivationAllocation())
            read(PromotedHeapLocation(ActivationScopePLoc, node->child1()));
        break;

    case GetScope:
        if (node->child1()->isPhantomFunctionAllocation())
            read(PromotedHeapLocation(FunctionActivationPLoc, node->child1()));
        break;

    case GetExecutable:
        if (node->child1()->isPhantomFunctionAllocation())
            read(PromotedHeapLocation(FunctionExecutablePLoc, node->child1()));
        break;

    case PutHint: {
        ASSERT(node->child1()->isPhantomAllocation());
        write(
            PromotedHeapLocation(node->child1().node(), node->promotedLocationDescriptor()),
            node->child2());
        break;
    }

    default:
        break;
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGPromoteHeapAccess_h

