/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
#include "PolymorphicCallStubRoutine.h"

#if ENABLE(JIT)

#include "AccessCase.h"
#include "CallLinkInfo.h"
#include "CodeBlock.h"
#include "FullCodeOrigin.h"
#include "JSCJSValueInlines.h"
#include "LinkBuffer.h"

namespace JSC {

PolymorphicCallNode::~PolymorphicCallNode()
{
    if (isOnList())
        remove();
}

void PolymorphicCallNode::unlink(VM& vm)
{
    if (m_callLinkInfo) {
        dataLogLnIf(Options::dumpDisassembly(), "Unlinking polymorphic call at ", m_callLinkInfo->doneLocation(), ", bc#", m_callLinkInfo->codeOrigin().bytecodeIndex());
        m_callLinkInfo->unlink(vm);
    }

    if (isOnList())
        remove();
}

void PolymorphicCallNode::clearCallLinkInfo()
{
    m_callLinkInfo = nullptr;
}

void PolymorphicCallCase::dump(PrintStream& out) const
{
    out.print("<variant = ", m_variant, ", codeBlock = ", pointerDump(m_codeBlock), ">");
}

PolymorphicCallStubRoutine::PolymorphicCallStubRoutine(
    const MacroAssemblerCodeRef<JITStubRoutinePtrTag>& codeRef, VM& vm, const JSCell* owner, CallFrame* callerFrame,
    CallLinkInfo& info, const Vector<PolymorphicCallCase>& cases,
    UniqueArray<uint32_t>&& fastCounts)
    : GCAwareJITStubRoutine(codeRef)
    , m_variants(cases.size())
    , m_fastCounts(WTFMove(fastCounts))
{
    for (unsigned index = 0; index < cases.size(); ++index) {
        const PolymorphicCallCase& callCase = cases[index];
        m_variants[index].set(vm, owner, callCase.variant().rawCalleeCell());
        if (shouldDumpDisassemblyFor(callerFrame->codeBlock()))
            dataLog("Linking polymorphic call in ", FullCodeOrigin(callerFrame->codeBlock(), callerFrame->codeOrigin()), " to ", callCase.variant(), ", codeBlock = ", pointerDump(callCase.codeBlock()), "\n");
        if (CodeBlock* codeBlock = callCase.codeBlock())
            codeBlock->linkIncomingPolymorphicCall(callerFrame, m_callNodes.add(&info));
    }
    WTF::storeStoreFence();
    makeGCAware(vm);
}

PolymorphicCallStubRoutine::~PolymorphicCallStubRoutine() { }

CallVariantList PolymorphicCallStubRoutine::variants() const
{
    CallVariantList result;
    for (size_t i = 0; i < m_variants.size(); ++i)
        result.append(CallVariant(m_variants[i].get()));
    return result;
}

bool PolymorphicCallStubRoutine::hasEdges() const
{
    // The FTL does not count edges in its poly call stub routines. If the FTL went poly call, then
    // it's not meaningful to keep profiling - we can just leave it at that. Remember, the FTL would
    // have had full edge profiling from the DFG, and based on this information, it would have
    // decided to go poly.
    //
    // There probably are very-difficult-to-imagine corner cases where the FTL not doing edge
    // profiling is bad for polyvariant inlining. But polyvariant inlining is profitable sometimes
    // while not having to increment counts is profitable always. So, we let the FTL run faster and
    // not keep counts.
    return !!m_fastCounts;
}

CallEdgeList PolymorphicCallStubRoutine::edges() const
{
    RELEASE_ASSERT(m_fastCounts);
    
    CallEdgeList result;
    for (size_t i = 0; i < m_variants.size(); ++i)
        result.append(CallEdge(CallVariant(m_variants[i].get()), m_fastCounts[i]));
    return result;
}

void PolymorphicCallStubRoutine::clearCallNodesFor(CallLinkInfo* info)
{
    for (Bag<PolymorphicCallNode>::iterator iter = m_callNodes.begin(); !!iter; ++iter) {
        PolymorphicCallNode& node = **iter;
        // All nodes should point to info, but okay to be a little paranoid.
        if (node.hasCallLinkInfo(info))
            node.clearCallLinkInfo();
    }
}

bool PolymorphicCallStubRoutine::visitWeak(VM& vm)
{
    bool isStillLive = true;
    forEachDependentCell([&](JSCell* cell) {
        isStillLive &= vm.heap.isMarked(cell);
    });
    return isStillLive;
}

template<typename Visitor>
ALWAYS_INLINE void PolymorphicCallStubRoutine::markRequiredObjectsInternalImpl(Visitor& visitor)
{
    for (auto& variant : m_variants)
        visitor.append(variant);
}

void PolymorphicCallStubRoutine::markRequiredObjectsInternal(AbstractSlotVisitor& visitor)
{
    markRequiredObjectsInternalImpl(visitor);
}
void PolymorphicCallStubRoutine::markRequiredObjectsInternal(SlotVisitor& visitor)
{
    markRequiredObjectsInternalImpl(visitor);
}

} // namespace JSC

#endif // ENABLE(JIT)
