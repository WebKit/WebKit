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

#include "config.h"
#include "DFGMayExit.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGNode.h"
#include "Operations.h"

namespace JSC { namespace DFG {

namespace {

class EdgeMayExit {
public:
    EdgeMayExit()
        : m_result(false)
    {
    }
    
    void operator()(Node*, Edge edge)
    {
        if (edge.willHaveCheck()) {
            m_result = true;
            return;
        }

        switch (edge.useKind()) {
        // These are shady because nodes that have these use kinds will typically exit for
        // unrelated reasons. For example CompareEq doesn't usually exit, but if it uses ObjectUse
        // then it will.
        case ObjectUse:
        case ObjectOrOtherUse:
            m_result = true;
            break;
            
        // These are shady because they check the structure even if the type of the child node
        // passes the StringObject type filter.
        case StringObjectUse:
        case StringOrStringObjectUse:
            m_result = true;
            break;
            
        default:
            break;
        }
    }
    
    bool result() const { return m_result; }
    
private:
    bool m_result;
};

} // anonymous namespace

bool mayExit(Graph& graph, Node* node)
{
    switch (node->op()) {
    // This is a carefully curated list of nodes that definitely do not exit. We try to be very
    // conservative when maintaining this list, because adding new node types to it doesn't
    // generally make things a lot better but it might introduce insanely subtle bugs.
    case SetArgument:
    case JSConstant:
    case DoubleConstant:
    case Int52Constant:
    case MovHint:
    case SetLocal:
    case Flush:
    case Phantom:
    case Check:
    case GetLocal:
    case LoopHint:
    case Phi:
    case Upsilon:
    case ZombieHint:
    case BottomValue:
    case PutHint:
    case PhantomNewObject:
    case PutStack:
    case KillStack:
    case GetStack:
    case GetCallee:
    case GetArgumentCount:
    case GetScope:
    case PhantomLocal:
    case CountExecution:
    case Jump:
    case Branch:
    case Unreachable:
    case DoubleRep:
    case Int52Rep:
    case ValueRep:
        break;
        
    default:
        // If in doubt, return true.
        return true;
    }

    EdgeMayExit functor;
    DFG_NODE_DO_TO_CHILDREN(graph, node, functor);
    return functor.result();
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
