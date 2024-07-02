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

#include "CachedCall.h"
#include "CallLinkInfo.h"
#include "CodeBlock.h"
#include "FullCodeOrigin.h"
#include "JSCJSValueInlines.h"
#include "LinkBuffer.h"

namespace JSC {

void PolymorphicCallNode::unlinkOrUpgradeImpl(VM& vm, CodeBlock* oldCodeBlock, CodeBlock* newCodeBlock)
{
    // We first remove itself from the linked-list before unlinking callLinkInfo.
    // The reason is that callLinkInfo can potentially link PolymorphicCallNode's stub itself, and it may destroy |this| (the other CallLinkInfo
    // does not do it since it is not chained in PolymorphicCallStubRoutine).
    if (isOnList())
        remove();

    if (!m_cleared) {
        if (!newCodeBlock || !owner()->upgradeIfPossible(vm, oldCodeBlock, newCodeBlock, m_index)) {
            m_cleared = true;
            CallLinkInfo* callLinkInfo = owner()->callLinkInfo();
            dataLogLnIf(Options::dumpDisassembly(), "Unlinking polymorphic call bc#", callLinkInfo->codeOrigin().bytecodeIndex());
            callLinkInfo->unlinkOrUpgrade(vm, oldCodeBlock, newCodeBlock);
        }
    }
}

void PolymorphicCallNode::clear()
{
    m_cleared = true;
}

PolymorphicCallStubRoutine* PolymorphicCallNode::owner()
{
    return bitwise_cast<PolymorphicCallStubRoutine*>(this - m_index + m_totalSize);
}

void PolymorphicCallCase::dump(PrintStream& out) const
{
    out.print("<variant = ", m_variant, ", codeBlock = ", pointerDump(m_codeBlock), ">");
}

PolymorphicCallStubRoutine::PolymorphicCallStubRoutine(unsigned headerSize, unsigned trailingSize, const MacroAssemblerCodeRef<JITStubRoutinePtrTag>& code, VM& vm, JSCell* owner, CallFrame* callerFrame, CallLinkInfo& callLinkInfo, const Vector<CallSlot, 16>& callSlots, bool notUsingCounting, bool isClosureCall)
    : GCAwareJITStubRoutine(Type::PolymorphicCallStubRoutineType, code, owner)
    , ButterflyArray<PolymorphicCallStubRoutine, PolymorphicCallNode, CallSlot>(headerSize, trailingSize)
    , m_callLinkInfo(&callLinkInfo)
    , m_notUsingCounting(notUsingCounting)
    , m_isClosureCall(isClosureCall)
{
    for (unsigned index = 0; index < callSlots.size(); ++index) {
        auto& slot = trailingSpan()[index];
        slot = callSlots[index];

        if (callerFrame && !callerFrame->isNativeCalleeFrame())
            dataLogLnIf(shouldDumpDisassemblyFor(callerFrame->codeBlock()), "Linking polymorphic call in ", FullCodeOrigin(callerFrame->codeBlock(), callerFrame->codeOrigin()), " to ", CallVariant(slot.m_calleeOrExecutable), ", codeBlock = ", pointerDump(slot.m_codeBlock));

        auto& callNode = leadingSpan()[index];
        callNode.initialize(index, headerSize);
        if (CodeBlock* codeBlock = slot.m_codeBlock)
            codeBlock->linkIncomingCall(owner, &callNode);

        vm.writeBarrier(owner, slot.m_calleeOrExecutable);
    }

    WTF::storeStoreFence();
    bool isCodeImmutable = true;
    makeGCAware(vm, isCodeImmutable);
}

bool PolymorphicCallStubRoutine::upgradeIfPossible(VM&, CodeBlock* oldCodeBlock, CodeBlock* newCodeBlock, uint8_t index)
{
    // It is possible that we can just upgrade the CallSlot and continue using this PolymorphicCallStubRoutine instead of unlinking CallLinkInfo.
    auto& callNode = leadingSpan()[index];
    auto& slot = trailingSpan()[index];

    if (callNode.isOnList())
        return false;

    if (slot.m_codeBlock != oldCodeBlock)
        return false;

    auto target = newCodeBlock->jitCode()->addressForCall(slot.m_arityCheckMode);
    slot.m_codeBlock = newCodeBlock;
    slot.m_target = target;
    newCodeBlock->linkIncomingCall(nullptr, &callNode); // This is just relinking. So owner and caller frame can be nullptr.
    return true;
}

CallVariantList PolymorphicCallStubRoutine::variants() const
{
    CallVariantList result;
    forEachDependentCell([&](JSCell* cell) {
        result.append(CallVariant(cell));
    });
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
    return !m_notUsingCounting;
}

CallEdgeList PolymorphicCallStubRoutine::edges() const
{
    CallEdgeList result;
    unsigned index = 0;
    forEachDependentCell([&](JSCell* cell) {
        unsigned count = trailingSpan()[index].m_count;
        result.append(CallEdge(CallVariant(cell), count));
        ++index;
    });
    return result;
}

void PolymorphicCallStubRoutine::clearCallNodesFor(CallLinkInfo*)
{
    for (auto& callNode : leadingSpan())
        callNode.clear();
}

bool PolymorphicCallStubRoutine::visitWeakImpl(VM& vm)
{
    bool isStillLive = true;
    for (unsigned i = 0, size = std::size(trailingSpan()) - 1; i < size; ++i) {
        auto& slot = trailingSpan()[i];
        if (!slot.m_calleeOrExecutable) {
            isStillLive = false;
            continue;
        }
        if (!vm.heap.isMarked(slot.m_calleeOrExecutable)) {
            slot = CallSlot();
            isStillLive = false;
            continue;
        }
    }
    return isStillLive;
}

void PolymorphicCallStubRoutine::markRequiredObjectsImpl(AbstractSlotVisitor&)
{
}

void PolymorphicCallStubRoutine::markRequiredObjectsImpl(SlotVisitor&)
{
}

void PolymorphicCallStubRoutine::destroy(PolymorphicCallStubRoutine* derived)
{
    delete derived;
}

} // namespace JSC
