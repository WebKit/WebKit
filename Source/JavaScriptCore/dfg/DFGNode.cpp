/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
#include "DFGNode.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGPromotedHeapLocation.h"
#include "JSCInlines.h"
#include "JSImmutableButterfly.h"

namespace JSC { namespace DFG {

const char Node::HashSetTemplateInstantiationString[] = "::JSC::DFG::Node*";

bool MultiPutByOffsetData::writesStructures() const
{
    for (unsigned i = variants.size(); i--;) {
        if (variants[i].writesStructures())
            return true;
    }
    return false;
}

bool MultiPutByOffsetData::reallocatesStorage() const
{
    for (unsigned i = variants.size(); i--;) {
        if (variants[i].reallocatesStorage())
            return true;
    }
    return false;
}

void BranchTarget::dump(PrintStream& out) const
{
    if (!block)
        return;
    
    out.print(*block);
    
    if (count == count) // If the count is not NaN, then print it.
        out.print("/w:", count);
}

bool Node::hasVariableAccessData(Graph& graph)
{
    switch (op()) {
    case Phi:
        return graph.m_form != SSA;
    case GetLocal:
    case SetLocal:
    case SetArgumentDefinitely:
    case SetArgumentMaybe:
    case Flush:
    case PhantomLocal:
        return true;
    default:
        return false;
    }
}

void Node::remove(Graph& graph)
{
    switch (op()) {
    case MultiGetByOffset: {
        MultiGetByOffsetData& data = multiGetByOffsetData();
        StructureSet set;
        for (MultiGetByOffsetCase& getCase : data.cases) {
            getCase.set().forEach(
                [&] (RegisteredStructure structure) {
                    set.add(structure.get());
                });
        }
        convertToCheckStructure(graph.addStructureSet(set));
        return;
    }
        
    case MatchStructure: {
        MatchStructureData& data = matchStructureData();
        RegisteredStructureSet set;
        for (MatchStructureVariant& variant : data.variants)
            set.add(variant.structure);
        convertToCheckStructure(graph.addStructureSet(set));
        return;
    }
        
    default:
        if (flags() & NodeHasVarArgs) {
            unsigned targetIndex = 0;
            for (unsigned i = 0; i < numChildren(); ++i) {
                Edge& edge = graph.varArgChild(this, i);
                if (!edge)
                    continue;
                if (edge.willHaveCheck()) {
                    Edge& dst = graph.varArgChild(this, targetIndex++);
                    std::swap(dst, edge);
                    continue;
                }
                edge = Edge();
            }
            setOpAndDefaultFlags(CheckVarargs);
            children.setNumChildren(targetIndex);
        } else {
            children = children.justChecks();
            setOpAndDefaultFlags(Check);
        }
        return;
    }
}

void Node::removeWithoutChecks()
{
    children = AdjacencyList();
    setOpAndDefaultFlags(Check);
}

void Node::replaceWith(Graph& graph, Node* other)
{
    remove(graph);
    setReplacement(other);
}

void Node::replaceWithWithoutChecks(Node* other)
{
    removeWithoutChecks();
    setReplacement(other);
}

void Node::convertToIdentity()
{
    RELEASE_ASSERT(child1());
    RELEASE_ASSERT(!child2());
    NodeFlags result = canonicalResultRepresentation(this->result());
    setOpAndDefaultFlags(Identity);
    setResult(result);
}

void Node::convertToIdentityOn(Node* child)
{
    children.reset();
    clearFlags(NodeHasVarArgs);
    child1() = child->defaultEdge();
    NodeFlags output = canonicalResultRepresentation(this->result());
    NodeFlags input = canonicalResultRepresentation(child->result());
    if (output == input) {
        setOpAndDefaultFlags(Identity);
        setResult(output);
        return;
    }
    switch (output) {
    case NodeResultDouble:
        setOpAndDefaultFlags(DoubleRep);
        switch (input) {
        case NodeResultInt52:
            child1().setUseKind(Int52RepUse);
            return;
        case NodeResultJS:
            child1().setUseKind(NumberUse);
            return;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }
    case NodeResultInt52:
        setOpAndDefaultFlags(Int52Rep);
        switch (input) {
        case NodeResultDouble:
            child1().setUseKind(DoubleRepAnyIntUse);
            return;
        case NodeResultJS:
            child1().setUseKind(AnyIntUse);
            return;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }
    case NodeResultJS:
        setOpAndDefaultFlags(ValueRep);
        switch (input) {
        case NodeResultDouble:
            child1().setUseKind(DoubleRepUse);
            return;
        case NodeResultInt52:
            child1().setUseKind(Int52RepUse);
            return;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
}

void Node::convertToLazyJSConstant(Graph& graph, LazyJSValue value)
{
    m_op = LazyJSConstant;
    m_flags &= ~NodeMustGenerate;
    m_opInfo = graph.m_lazyJSValues.add(value);
    children.reset();
}

void Node::convertToNewArrayBuffer(FrozenValue* immutableButterfly)
{
    setOpAndDefaultFlags(NewArrayBuffer);
    NewArrayBufferData data { };
    data.indexingMode = immutableButterfly->cast<JSImmutableButterfly*>()->indexingMode();
    data.vectorLengthHint = immutableButterfly->cast<JSImmutableButterfly*>()->toButterfly()->vectorLength();
    children.reset();
    m_opInfo = immutableButterfly;
    m_opInfo2 = data.asQuadWord;
}

void Node::convertToDirectCall(FrozenValue* executable)
{
    NodeType newOp = LastNodeType;
    switch (op()) {
    case Call:
        newOp = DirectCall;
        break;
    case Construct:
        newOp = DirectConstruct;
        break;
    case TailCallInlinedCaller:
        newOp = DirectTailCallInlinedCaller;
        break;
    case TailCall:
        newOp = DirectTailCall;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    
    m_op = newOp;
    m_opInfo = executable;
}

void Node::convertToCallDOM(Graph& graph)
{
    ASSERT(op() == Call);
    ASSERT(signature());

    Edge edges[3];
    // Skip the first one. This is callee.
    RELEASE_ASSERT(numChildren() <= 4);
    for (unsigned i = 1; i < numChildren(); ++i)
        edges[i - 1] = graph.varArgChild(this, i);

    setOpAndDefaultFlags(CallDOM);
    children.setChild1(edges[0]);
    children.setChild2(edges[1]);
    children.setChild3(edges[2]);

    if (!signature()->effect.mustGenerate())
        clearFlags(NodeMustGenerate);
}

void Node::convertToRegExpExecNonGlobalOrStickyWithoutChecks(FrozenValue* regExp)
{
    ASSERT(op() == RegExpExec);
    setOpAndDefaultFlags(RegExpExecNonGlobalOrSticky);
    children.child1() = Edge(children.child1().node(), KnownCellUse);
    children.child2() = Edge(children.child3().node(), KnownStringUse);
    children.child3() = Edge();
    m_opInfo = regExp;
}

void Node::convertToRegExpMatchFastGlobalWithoutChecks(FrozenValue* regExp)
{
    ASSERT(op() == RegExpMatchFast);
    setOpAndDefaultFlags(RegExpMatchFastGlobal);
    children.child1() = Edge(children.child1().node(), KnownCellUse);
    children.child2() = Edge(children.child3().node(), KnownStringUse);
    children.child3() = Edge();
    m_opInfo = regExp;
}

String Node::tryGetString(Graph& graph)
{
    if (hasConstant())
        return constant()->tryGetString(graph);
    if (hasLazyJSValue())
        return lazyJSValue().tryGetString(graph);
    return String();
}

PromotedLocationDescriptor Node::promotedLocationDescriptor()
{
    return PromotedLocationDescriptor(static_cast<PromotedLocationKind>(m_opInfo.as<uint32_t>()), m_opInfo2.as<uint32_t>());
}

} } // namespace JSC::DFG

namespace WTF {

using namespace JSC;
using namespace JSC::DFG;

void printInternal(PrintStream& out, SwitchKind kind)
{
    switch (kind) {
    case SwitchImm:
        out.print("SwitchImm");
        return;
    case SwitchChar:
        out.print("SwitchChar");
        return;
    case SwitchString:
        out.print("SwitchString");
        return;
    case SwitchCell:
        out.print("SwitchCell");
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void printInternal(PrintStream& out, Node* node)
{
    if (!node) {
        out.print("-");
        return;
    }
    out.print("@", node->index());
    if (node->hasDoubleResult())
        out.print("<Double>");
    else if (node->hasInt52Result())
        out.print("<Int52>");
}

} // namespace WTF

#endif // ENABLE(DFG_JIT)

