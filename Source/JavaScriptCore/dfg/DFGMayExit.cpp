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
        // FIXME: Maybe this should call mayHaveTypeCheck(edge.useKind()) instead.
        // https://bugs.webkit.org/show_bug.cgi?id=148545
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

ExitMode mayExit(Graph& graph, Node* node)
{
    ExitMode result = DoesNotExit;
    
    switch (node->op()) {
    // This is a carefully curated list of nodes that definitely do not exit. We try to be very
    // conservative when maintaining this list, because adding new node types to it doesn't
    // generally make things a lot better but it might introduce subtle bugs.
    case SetArgument:
    case JSConstant:
    case DoubleConstant:
    case LazyJSConstant:
    case Int52Constant:
    case MovHint:
    case SetLocal:
    case Flush:
    case Phantom:
    case Check:
    case Identity:
    case GetLocal:
    case LoopHint:
    case Phi:
    case Upsilon:
    case ZombieHint:
    case ExitOK:
    case BottomValue:
    case PutHint:
    case PhantomNewObject:
    case PutStack:
    case KillStack:
    case GetStack:
    case GetCallee:
    case GetArgumentCount:
    case GetRestLength:
    case GetScope:
    case PhantomLocal:
    case CountExecution:
    case Jump:
    case Branch:
    case Unreachable:
    case DoubleRep:
    case Int52Rep:
    case ValueRep:
    case ExtractOSREntryLocal:
    case LogicalNot:
    case NotifyWrite:
    case PutStructure:
    case StoreBarrier:
    case PutByOffset:
    case PutClosureVar:
        break;

    case StrCat:
    case Call:
    case Construct:
    case CallVarargs:
    case ConstructVarargs:
    case CallForwardVarargs:
    case ConstructForwardVarargs:
    case MaterializeCreateActivation:
    case MaterializeNewObject:
    case NewFunction:
    case NewArrowFunction:
    case NewGeneratorFunction:
    case NewStringObject:
    case CreateActivation:
        result = ExitsForExceptions;
        break;

    default:
        // If in doubt, return true.
        return Exits;
    }

    EdgeMayExit functor;
    DFG_NODE_DO_TO_CHILDREN(graph, node, functor);
    if (functor.result())
        result = Exits;
    
    return result;
}

} } // namespace JSC::DFG

namespace WTF {

using namespace JSC::DFG;

void printInternal(PrintStream& out, ExitMode mode)
{
    switch (mode) {
    case DoesNotExit:
        out.print("DoesNotExit");
        return;
    case ExitsForExceptions:
        out.print("ExitsForExceptions");
        return;
    case Exits:
        out.print("Exits");
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WTF

#endif // ENABLE(DFG_JIT)
