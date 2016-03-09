/*
 * Copyright (C) 2013, 2014, 2016 Apple Inc. All rights reserved.
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
#include "DFGNodeAllocator.h"
#include "DFGPromotedHeapLocation.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

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

unsigned Node::index() const
{
    return NodeAllocator::allocatorOf(this)->indexOf(this);
}

bool Node::hasVariableAccessData(Graph& graph)
{
    switch (op()) {
    case Phi:
        return graph.m_form != SSA;
    case GetLocal:
    case SetLocal:
    case SetArgument:
    case Flush:
    case PhantomLocal:
        return true;
    default:
        return false;
    }
}

void Node::remove()
{
    ASSERT(!(flags() & NodeHasVarArgs));
    
    children = children.justChecks();
    
    setOpAndDefaultFlags(Check);
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
            child1().setUseKind(DoubleRepMachineIntUse);
            return;
        case NodeResultJS:
            child1().setUseKind(MachineIntUse);
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
    m_opInfo = bitwise_cast<uintptr_t>(graph.m_lazyJSValues.add(value));
    children.reset();
}

void Node::convertToPutHint(const PromotedLocationDescriptor& descriptor, Node* base, Node* value)
{
    m_op = PutHint;
    m_opInfo = descriptor.imm1().m_value;
    m_opInfo2 = descriptor.imm2().m_value;
    child1() = base->defaultEdge();
    child2() = value->defaultEdge();
    child3() = Edge();
}

void Node::convertToPutStructureHint(Node* structure)
{
    ASSERT(m_op == PutStructure);
    ASSERT(structure->castConstant<Structure*>() == transition()->next);
    convertToPutHint(StructurePLoc, child1().node(), structure);
}

void Node::convertToPutByOffsetHint()
{
    ASSERT(m_op == PutByOffset);
    convertToPutHint(
        PromotedLocationDescriptor(NamedPropertyPLoc, storageAccessData().identifierNumber),
        child2().node(), child3().node());
}

void Node::convertToPutClosureVarHint()
{
    ASSERT(m_op == PutClosureVar);
    convertToPutHint(
        PromotedLocationDescriptor(ClosureVarPLoc, scopeOffset().offset()),
        child1().node(), child2().node());
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
    return PromotedLocationDescriptor(static_cast<PromotedLocationKind>(m_opInfo), m_opInfo2);
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

