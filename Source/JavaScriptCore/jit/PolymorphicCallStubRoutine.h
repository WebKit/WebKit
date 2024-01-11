/*
 * Copyright (C) 2015-2022 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(JIT)

#include "CallEdge.h"
#include "CallLinkInfoBase.h"
#include "CallVariant.h"
#include "GCAwareJITStubRoutine.h"
#include <wtf/ButterflyArray.h>
#include <wtf/Noncopyable.h>
#include <wtf/UniqueArray.h>
#include <wtf/Vector.h>

namespace JSC {

class CallLinkInfo;

class PolymorphicCallNode final : public CallLinkInfoBase {
    WTF_MAKE_NONCOPYABLE(PolymorphicCallNode);
public:
    PolymorphicCallNode(CallLinkInfo* info)
        : CallLinkInfoBase(CallSiteType::PolymorphicCallNode)
        , m_callLinkInfo(info)
    {
    }
    
    void unlinkImpl(VM&);

    bool hasCallLinkInfo(CallLinkInfo* info) { return m_callLinkInfo == info; }
    void clearCallLinkInfo();
    
private:
    CallLinkInfo* m_callLinkInfo;
};

class PolymorphicCallCase {
public:
    PolymorphicCallCase()
        : m_codeBlock(nullptr)
    {
    }
    
    PolymorphicCallCase(CallVariant variant, CodeBlock* codeBlock)
        : m_variant(variant)
        , m_codeBlock(codeBlock)
    {
    }
    
    CallVariant variant() const { return m_variant; }
    CodeBlock* codeBlock() const { return m_codeBlock; }
    
    void dump(PrintStream&) const;
    
private:
    CallVariant m_variant;
    CodeBlock* const m_codeBlock;
};

class PolymorphicCallStubRoutine final : public GCAwareJITStubRoutine, public ButterflyArray<PolymorphicCallStubRoutine, void*, CallSlot> {
public:
    friend class JITStubRoutine;

    CallVariantList variants() const;
    bool hasEdges() const;
    CallEdgeList edges() const;

    void clearCallNodesFor(CallLinkInfo*);

    template<typename Functor>
    void forEachDependentCell(const Functor& functor)
    {
        for (auto& variant : m_variants)
            functor(variant.get());
    }

    static Ref<PolymorphicCallStubRoutine> create(
        const MacroAssemblerCodeRef<JITStubRoutinePtrTag>& code, VM& vm, JSCell* owner,
        CallFrame* callerFrame, CallLinkInfo& callLinkInfo, const Vector<PolymorphicCallCase, 16>& cases,
        UniqueArray<uint32_t>&& fastCounts, bool notUsingCounting)
    {
        return adoptRef(*createImpl(0, 0, code, vm, owner, callerFrame, callLinkInfo, cases, WTFMove(fastCounts), notUsingCounting));
    }

    static Ref<PolymorphicCallStubRoutine> create(const MacroAssemblerCodeRef<JITStubRoutinePtrTag>& code, VM& vm, JSCell* owner, CallFrame* callerFrame, CallLinkInfo& callLinkInfo, const Vector<PolymorphicCallCase, 16>& cases, const Vector<CallSlot, 16>& callSlots, bool notUsingCounting)
    {
        return adoptRef(*createImpl(0, callSlots.size(), code, vm, owner, callerFrame, callLinkInfo, cases, callSlots, notUsingCounting));
    }

    PolymorphicCallStubRoutine(unsigned headerSize, unsigned trailingSize,
        const MacroAssemblerCodeRef<JITStubRoutinePtrTag>&, VM&, JSCell* owner,
        CallFrame* callerFrame, CallLinkInfo&, const Vector<PolymorphicCallCase, 16>&,
        UniqueArray<uint32_t>&& fastCounts, bool notUsingCounting);

    PolymorphicCallStubRoutine(unsigned headerSize, unsigned trailingSize, const MacroAssemblerCodeRef<JITStubRoutinePtrTag>&, VM&, JSCell* owner, CallFrame* callerFrame, CallLinkInfo&, const Vector<PolymorphicCallCase, 16>&, const Vector<CallSlot, 16>&, bool notUsingCounting);

    using ButterflyArray<PolymorphicCallStubRoutine, void*, CallSlot>::operator delete;

private:
    template<typename Visitor> void markRequiredObjectsInternalImpl(Visitor&);
    void markRequiredObjectsImpl(AbstractSlotVisitor&);
    void markRequiredObjectsImpl(SlotVisitor&);

    bool visitWeakImpl(VM&);

    FixedVector<WriteBarrier<JSCell>> m_variants;
    UniqueArray<uint32_t> m_fastCounts;
    Bag<PolymorphicCallNode> m_callNodes;
    bool m_notUsingCounting : 1 { false };
};

} // namespace JSC

#endif // ENABLE(JIT)
