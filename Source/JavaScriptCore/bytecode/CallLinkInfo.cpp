/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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
#include "CallLinkInfo.h"

#include "CallFrameShuffleData.h"
#include "DFGOperations.h"
#include "DFGThunks.h"
#include "FunctionCodeBlock.h"
#include "JSCInlines.h"
#include "Opcode.h"
#include "Repatch.h"
#include <wtf/ListDump.h>

#if ENABLE(JIT)
namespace JSC {

CallLinkInfo::CallType CallLinkInfo::callTypeFor(OpcodeID opcodeID)
{
    if (opcodeID == op_call || opcodeID == op_call_eval)
        return Call;
    if (opcodeID == op_call_varargs)
        return CallVarargs;
    if (opcodeID == op_construct)
        return Construct;
    if (opcodeID == op_construct_varargs)
        return ConstructVarargs;
    if (opcodeID == op_tail_call)
        return TailCall;
    ASSERT(opcodeID == op_tail_call_varargs || opcodeID == op_tail_call_forward_arguments);
    return TailCallVarargs;
}

CallLinkInfo::CallLinkInfo()
    : m_hasSeenShouldRepatch(false)
    , m_hasSeenClosure(false)
    , m_clearedByGC(false)
    , m_clearedByVirtual(false)
    , m_allowStubs(true)
    , m_clearedByJettison(false)
    , m_callType(None)
{
}

CallLinkInfo::~CallLinkInfo()
{
    clearStub();
    
    if (isOnList())
        remove();
}

void CallLinkInfo::clearStub()
{
    if (!stub())
        return;

    m_stub->clearCallNodesFor(this);
    m_stub = nullptr;
}

void CallLinkInfo::unlink(VM& vm)
{
    // We could be called even if we're not linked anymore because of how polymorphic calls
    // work. Each callsite within the polymorphic call stub may separately ask us to unlink().
    if (isLinked())
        unlinkFor(vm, *this);

    // Either we were unlinked, in which case we should not have been on any list, or we unlinked
    // ourselves so that we're not on any list anymore.
    RELEASE_ASSERT(!isOnList());
}

CodeLocationNearCall<JSInternalPtrTag> CallLinkInfo::callReturnLocation()
{
    RELEASE_ASSERT(!isDirect());
    return CodeLocationNearCall<JSInternalPtrTag>(m_callReturnLocationOrPatchableJump, NearCallMode::Regular);
}

CodeLocationJump<JSInternalPtrTag> CallLinkInfo::patchableJump()
{
    RELEASE_ASSERT(callType() == DirectTailCall);
    return CodeLocationJump<JSInternalPtrTag>(m_callReturnLocationOrPatchableJump);
}

CodeLocationDataLabelPtr<JSInternalPtrTag> CallLinkInfo::hotPathBegin()
{
    RELEASE_ASSERT(!isDirect());
    return CodeLocationDataLabelPtr<JSInternalPtrTag>(m_hotPathBeginOrSlowPathStart);
}

CodeLocationLabel<JSInternalPtrTag> CallLinkInfo::slowPathStart()
{
    RELEASE_ASSERT(isDirect());
    return m_hotPathBeginOrSlowPathStart;
}

void CallLinkInfo::setCallee(VM& vm, JSCell* owner, JSObject* callee)
{
    RELEASE_ASSERT(!isDirect());
    m_calleeOrCodeBlock.set(vm, owner, callee);
}

void CallLinkInfo::clearCallee()
{
    RELEASE_ASSERT(!isDirect());
    m_calleeOrCodeBlock.clear();
}

JSObject* CallLinkInfo::callee()
{
    RELEASE_ASSERT(!isDirect());
    return jsCast<JSObject*>(m_calleeOrCodeBlock.get());
}

void CallLinkInfo::setCodeBlock(VM& vm, JSCell* owner, FunctionCodeBlock* codeBlock)
{
    RELEASE_ASSERT(isDirect());
    m_calleeOrCodeBlock.setMayBeNull(vm, owner, codeBlock);
}

void CallLinkInfo::clearCodeBlock()
{
    RELEASE_ASSERT(isDirect());
    m_calleeOrCodeBlock.clear();
}

FunctionCodeBlock* CallLinkInfo::codeBlock()
{
    RELEASE_ASSERT(isDirect());
    return jsCast<FunctionCodeBlock*>(m_calleeOrCodeBlock.get());
}

void CallLinkInfo::setLastSeenCallee(VM& vm, const JSCell* owner, JSObject* callee)
{
    RELEASE_ASSERT(!isDirect());
    m_lastSeenCalleeOrExecutable.set(vm, owner, callee);
}

void CallLinkInfo::clearLastSeenCallee()
{
    RELEASE_ASSERT(!isDirect());
    m_lastSeenCalleeOrExecutable.clear();
}

JSObject* CallLinkInfo::lastSeenCallee() const
{
    RELEASE_ASSERT(!isDirect());
    return jsCast<JSObject*>(m_lastSeenCalleeOrExecutable.get());
}

bool CallLinkInfo::haveLastSeenCallee() const
{
    RELEASE_ASSERT(!isDirect());
    return !!m_lastSeenCalleeOrExecutable;
}

void CallLinkInfo::setExecutableDuringCompilation(ExecutableBase* executable)
{
    RELEASE_ASSERT(isDirect());
    m_lastSeenCalleeOrExecutable.setWithoutWriteBarrier(executable);
}

ExecutableBase* CallLinkInfo::executable()
{
    RELEASE_ASSERT(isDirect());
    return jsCast<ExecutableBase*>(m_lastSeenCalleeOrExecutable.get());
}

void CallLinkInfo::setMaxArgumentCountIncludingThis(unsigned value)
{
    RELEASE_ASSERT(isDirect());
    RELEASE_ASSERT(value);
    m_maxArgumentCountIncludingThis = value;
}

void CallLinkInfo::visitWeak(VM& vm)
{
    auto handleSpecificCallee = [&] (JSFunction* callee) {
        if (vm.heap.isMarked(callee->executable()))
            m_hasSeenClosure = true;
        else
            m_clearedByGC = true;
    };
    
    if (isLinked()) {
        if (stub()) {
            if (!stub()->visitWeak(vm)) {
                if (UNLIKELY(Options::verboseOSR())) {
                    dataLog(
                        "At ", m_codeOrigin, ", ", RawPointer(this), ": clearing call stub to ",
                        listDump(stub()->variants()), ", stub routine ", RawPointer(stub()),
                        ".\n");
                }
                unlink(vm);
                m_clearedByGC = true;
            }
        } else if (!vm.heap.isMarked(m_calleeOrCodeBlock.get())) {
            if (isDirect()) {
                if (UNLIKELY(Options::verboseOSR())) {
                    dataLog(
                        "Clearing call to ", RawPointer(codeBlock()), " (",
                        pointerDump(codeBlock()), ").\n");
                }
            } else {
                if (callee()->type() == JSFunctionType) {
                    if (UNLIKELY(Options::verboseOSR())) {
                        dataLog(
                            "Clearing call to ",
                            RawPointer(callee()), " (",
                            static_cast<JSFunction*>(callee())->executable()->hashFor(specializationKind()),
                            ").\n");
                    }
                    handleSpecificCallee(static_cast<JSFunction*>(callee()));
                } else {
                    if (UNLIKELY(Options::verboseOSR()))
                        dataLog("Clearing call to ", RawPointer(callee()), ".\n");
                    m_clearedByGC = true;
                }
            }
            unlink(vm);
        } else if (isDirect() && !vm.heap.isMarked(m_lastSeenCalleeOrExecutable.get())) {
            if (UNLIKELY(Options::verboseOSR())) {
                dataLog(
                    "Clearing call to ", RawPointer(executable()),
                    " because the executable is dead.\n");
            }
            unlink(vm);
            // We should only get here once the owning CodeBlock is dying, since the executable must
            // already be in the owner's weak references.
            m_lastSeenCalleeOrExecutable.clear();
        }
    }
    if (!isDirect() && haveLastSeenCallee() && !vm.heap.isMarked(lastSeenCallee())) {
        if (lastSeenCallee()->type() == JSFunctionType)
            handleSpecificCallee(jsCast<JSFunction*>(lastSeenCallee()));
        else
            m_clearedByGC = true;
        clearLastSeenCallee();
    }
}

void CallLinkInfo::setFrameShuffleData(const CallFrameShuffleData& shuffleData)
{
    m_frameShuffleData = makeUnique<CallFrameShuffleData>(shuffleData);
}

} // namespace JSC
#endif // ENABLE(JIT)

