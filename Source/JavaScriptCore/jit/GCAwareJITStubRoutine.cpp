/*
 * Copyright (C) 2012-2021 Apple Inc. All rights reserved.
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
#include "GCAwareJITStubRoutine.h"

#if ENABLE(JIT)

#include "AccessCase.h"
#include "CodeBlock.h"
#include "DFGCommonData.h"
#include "Heap.h"
#include "VM.h"
#include "JITStubRoutineSet.h"
#include "JSCellInlines.h"
#include <wtf/RefPtr.h>

namespace JSC {

GCAwareJITStubRoutine::GCAwareJITStubRoutine(Type type, const MacroAssemblerCodeRef<JITStubRoutinePtrTag>& code, JSCell* owner)
    : JITStubRoutine(type, code)
    , m_owner(owner)
{
}

void GCAwareJITStubRoutine::makeGCAware(VM& vm, bool isCodeImmutable)
{
    vm.heap.m_jitStubRoutines->add(this);
    m_isGCAware = true;
    m_isCodeImmutable = isCodeImmutable;
}

void GCAwareJITStubRoutine::observeZeroRefCountImpl()
{
    if (m_isJettisoned || !m_isGCAware) {
        // This case is needed for when the system shuts down. It may be that
        // the JIT stub routine set gets deleted before we get around to deleting
        // this guy. In that case the GC informs us that we're jettisoned already
        // and that we should delete ourselves as soon as the ref count reaches
        // zero.
IGNORE_GCC_WARNINGS_BEGIN("sequence-point")
        delete this;
IGNORE_GCC_WARNINGS_END
        return;
    }
    
    RELEASE_ASSERT(!m_refCount);

    m_isJettisoned = true;
}

void GCAwareJITStubRoutine::deleteFromGC()
{
    ASSERT(m_isJettisoned);
    ASSERT(!m_refCount);
    ASSERT(!m_mayBeExecuting);

IGNORE_GCC_WARNINGS_BEGIN("sequence-point")
    delete this;
IGNORE_GCC_WARNINGS_END
}

PolymorphicAccessJITStubRoutine::PolymorphicAccessJITStubRoutine(Type type, const MacroAssemblerCodeRef<JITStubRoutinePtrTag>& code, VM& vm, FixedVector<RefPtr<AccessCase>>&& cases, FixedVector<StructureID>&& weakStructures, JSCell* owner)
    : GCAwareJITStubRoutine(type, code, owner)
    , m_vm(vm)
    , m_cases(WTFMove(cases))
    , m_weakStructures(WTFMove(weakStructures))
{
}

void PolymorphicAccessJITStubRoutine::observeZeroRefCountImpl()
{
    if (m_vm.m_sharedJITStubs)
        m_vm.m_sharedJITStubs->remove(this);
    Base::observeZeroRefCountImpl();
}

unsigned PolymorphicAccessJITStubRoutine::computeHash(const FixedVector<RefPtr<AccessCase>>& cases, const FixedVector<StructureID>& weakStructures)
{
    Hasher hasher;
    for (auto& key : cases)
        WTF::add(hasher, key->hash());
    for (auto& structureID : weakStructures)
        WTF::add(hasher, structureID.bits());
    return hasher.hash();
}

MarkingGCAwareJITStubRoutine::MarkingGCAwareJITStubRoutine(
    Type type, const MacroAssemblerCodeRef<JITStubRoutinePtrTag>& code, VM& vm, FixedVector<RefPtr<AccessCase>>&& cases, FixedVector<StructureID>&& weakStructures, JSCell* owner,
    const Vector<JSCell*>& cells, Vector<std::unique_ptr<OptimizingCallLinkInfo>>&& callLinkInfos)
    : PolymorphicAccessJITStubRoutine(type, code, vm, WTFMove(cases), WTFMove(weakStructures), owner)
    , m_cells(cells.size())
    , m_callLinkInfos(WTFMove(callLinkInfos))
{
    for (unsigned i = cells.size(); i--;)
        m_cells[i].set(vm, owner, cells[i]);
}

template<typename Visitor>
ALWAYS_INLINE void MarkingGCAwareJITStubRoutine::markRequiredObjectsInternalImpl(Visitor& visitor)
{
    for (auto& entry : m_cells)
        visitor.append(entry);
}

void MarkingGCAwareJITStubRoutine::markRequiredObjectsImpl(AbstractSlotVisitor& visitor)
{
    markRequiredObjectsInternalImpl(visitor);
}
void MarkingGCAwareJITStubRoutine::markRequiredObjectsImpl(SlotVisitor& visitor)
{
    markRequiredObjectsInternalImpl(visitor);
}

bool MarkingGCAwareJITStubRoutine::visitWeakImpl(VM& vm)
{
    for (auto& callLinkInfo : m_callLinkInfos) {
        if (callLinkInfo)
            callLinkInfo->visitWeak(vm);
    }
    return PolymorphicAccessJITStubRoutine::visitWeakImpl(vm);
}

CallLinkInfo* MarkingGCAwareJITStubRoutine::callLinkInfoAtImpl(const ConcurrentJSLocker&, unsigned index)
{
    if (index < m_callLinkInfos.size())
        return m_callLinkInfos[index].get();
    return nullptr;
}

GCAwareJITStubRoutineWithExceptionHandler::GCAwareJITStubRoutineWithExceptionHandler(const MacroAssemblerCodeRef<JITStubRoutinePtrTag>& code, VM& vm, FixedVector<RefPtr<AccessCase>>&& cases, FixedVector<StructureID>&& weakStructures, JSCell* owner, const Vector<JSCell*>& cells, Vector<std::unique_ptr<OptimizingCallLinkInfo>>&& callLinkInfos,
    CodeBlock* codeBlockForExceptionHandlers, DisposableCallSiteIndex exceptionHandlerCallSiteIndex)
    : MarkingGCAwareJITStubRoutine(JITStubRoutine::Type::GCAwareJITStubRoutineWithExceptionHandlerType, code, vm, WTFMove(cases), WTFMove(weakStructures), owner, cells, WTFMove(callLinkInfos))
    , m_codeBlockWithExceptionHandler(codeBlockForExceptionHandlers)
#if ENABLE(DFG_JIT)
    , m_codeOriginPool(&m_codeBlockWithExceptionHandler->codeOrigins())
#endif
    , m_exceptionHandlerCallSiteIndex(exceptionHandlerCallSiteIndex)
{
    RELEASE_ASSERT(m_codeBlockWithExceptionHandler);
    ASSERT(!!m_codeBlockWithExceptionHandler->handlerForIndex(exceptionHandlerCallSiteIndex.bits()));
}

GCAwareJITStubRoutineWithExceptionHandler::~GCAwareJITStubRoutineWithExceptionHandler()
{
#if ENABLE(DFG_JIT)
    // We delay deallocation of m_exceptionHandlerCallSiteIndex until GCAwareJITStubRoutineWithExceptionHandler gets destroyed.
    // This means that CallSiteIndex can be reserved correctly so long as the code owned by GCAwareJITStubRoutineWithExceptionHandler is on the stack.
    // This is important since CallSite can be queried so long as this code is on the stack: StackVisitor can retreive CallSiteIndex from the stack.
    ASSERT(!isCompilationThread() && !Thread::mayBeGCThread());
    if (m_codeOriginPool)
        m_codeOriginPool->removeDisposableCallSiteIndex(m_exceptionHandlerCallSiteIndex);
#endif
}

void GCAwareJITStubRoutineWithExceptionHandler::observeZeroRefCountImpl()
{
#if ENABLE(DFG_JIT)
    if (m_codeBlockWithExceptionHandler) {
        m_codeBlockWithExceptionHandler->removeExceptionHandlerForCallSite(m_exceptionHandlerCallSiteIndex);
        m_codeBlockWithExceptionHandler = nullptr;
    }
#endif

    Base::observeZeroRefCountImpl();
}


Ref<PolymorphicAccessJITStubRoutine> createICJITStubRoutine(
    const MacroAssemblerCodeRef<JITStubRoutinePtrTag>& code,
    FixedVector<RefPtr<AccessCase>>&& cases,
    FixedVector<StructureID>&& weakStructures,
    VM& vm,
    JSCell* owner,
    bool makesCalls,
    const Vector<JSCell*>& cells,
    Vector<std::unique_ptr<OptimizingCallLinkInfo>>&& callLinkInfos,
    CodeBlock* codeBlockForExceptionHandlers,
    DisposableCallSiteIndex exceptionHandlerCallSiteIndex)
{
    if (!makesCalls) {
        // Allocating CallLinkInfos means we should have calls.
#if ASSERT_ENABLED
        for (auto& callLinkInfo : callLinkInfos)
            ASSERT(!callLinkInfo);
#endif
        return adoptRef(*new PolymorphicAccessJITStubRoutine(JITStubRoutine::Type::PolymorphicAccessJITStubRoutineType, code, vm, WTFMove(cases), WTFMove(weakStructures), owner));
    }
    
    if (codeBlockForExceptionHandlers) {
        RELEASE_ASSERT(JSC::JITCode::isOptimizingJIT(codeBlockForExceptionHandlers->jitType()));
        auto stub = adoptRef(*new GCAwareJITStubRoutineWithExceptionHandler(code, vm, WTFMove(cases), WTFMove(weakStructures), owner, cells, WTFMove(callLinkInfos), codeBlockForExceptionHandlers, exceptionHandlerCallSiteIndex));
        constexpr bool isCodeImmutable = false;
        stub->makeGCAware(vm, isCodeImmutable);
        return stub;
    }

    bool hasCallLinkInfo = false;
    for (auto& callLinkInfo : callLinkInfos) {
        if (callLinkInfo) {
            hasCallLinkInfo = true;
            break;
        }
    }

    if (cells.isEmpty() && !hasCallLinkInfo) {
        auto stub = adoptRef(*new PolymorphicAccessJITStubRoutine(JITStubRoutine::Type::PolymorphicAccessJITStubRoutineType, code, vm, WTFMove(cases), WTFMove(weakStructures), owner));
        constexpr bool isCodeImmutable = false;
        stub->makeGCAware(vm, isCodeImmutable);
        return stub;
    }
    
    auto stub = adoptRef(*new MarkingGCAwareJITStubRoutine(JITStubRoutine::Type::MarkingGCAwareJITStubRoutineType, code, vm, WTFMove(cases), WTFMove(weakStructures), owner, cells, WTFMove(callLinkInfos)));
    constexpr bool isCodeImmutable = false;
    stub->makeGCAware(vm, isCodeImmutable);
    return stub;
}

} // namespace JSC

#endif // ENABLE(JIT)

