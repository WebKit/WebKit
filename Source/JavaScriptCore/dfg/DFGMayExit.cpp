/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
        m_result |= edge.willHaveCheck();
    }
    
    bool result() const { return m_result; }
    
private:
    bool m_result;
};

} // anonymous namespace

bool mayExit(Graph& graph, Node* node)
{
    switch (node->op()) {
    case SetArgument:
    case JSConstant:
    case DoubleConstant:
    case Int52Constant:
    case MovHint:
    case SetLocal:
    case Flush:
    case Phantom:
    case Check:
    case HardPhantom:
    case GetLocal:
    case LoopHint:
    case PhantomArguments:
    case Phi:
    case Upsilon:
    case ZombieHint:
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
