/*
 * Copyright (C) 2008-2023 Apple Inc. All rights reserved.
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
#include "JITCode.h"

#include "DFGJITCode.h"
#include "FTLJITCode.h"

#include <wtf/PrintStream.h>

namespace JSC {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(DirectJITCode);

JITCode::JITCode(JITType jitType, CodePtr<JSEntryPtrTag> code, ShareAttribute shareAttribute)
    : m_jitType(jitType)
    , m_shareAttribute(shareAttribute)
    , m_addressForCall(code)
{
}

JITCode::~JITCode()
{
}

ASCIILiteral JITCode::typeName(JITType jitType)
{
    switch (jitType) {
    case JITType::None:
        return "None"_s;
    case JITType::HostCallThunk:
        return "Host"_s;
    case JITType::InterpreterThunk:
        return "LLInt"_s;
    case JITType::BaselineJIT:
        return "Baseline"_s;
    case JITType::DFGJIT:
        return "DFG"_s;
    case JITType::FTLJIT:
        return "FTL"_s;
    default:
        CRASH();
        return ""_s;
    }
}

bool JITCode::isUnlinked() const
{
    switch (m_jitType) {
    case JITType::None:
    case JITType::HostCallThunk:
    case JITType::InterpreterThunk:
    case JITType::BaselineJIT:
        return true;
    case JITType::DFGJIT:
#if ENABLE(DFG_JIT)
        return static_cast<const DFG::JITCode*>(this)->isUnlinked();
#else
        return false;
#endif
    case JITType::FTLJIT:
#if ENABLE(FTL_JIT)
        return static_cast<const FTL::JITCode*>(this)->isUnlinked();
#else
        return false;
#endif
    }
    return true;
}

void JITCode::validateReferences(const TrackedReferences&)
{
}

DFG::CommonData* JITCode::dfgCommon()
{
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

DFG::JITCode* JITCode::dfg()
{
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

FTL::JITCode* JITCode::ftl()
{
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

FTL::ForOSREntryJITCode* JITCode::ftlForOSREntry()
{
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

void JITCode::shrinkToFit(const ConcurrentJSLocker&)
{
}

const RegisterAtOffsetList* JITCode::calleeSaveRegisters() const
{
#if ENABLE(FTL_JIT)
    if (m_jitType == JITType::FTLJIT)
        return static_cast<const FTL::JITCode*>(this)->calleeSaveRegisters();
#endif
#if ENABLE(DFG_JIT)
    if (m_jitType == JITType::DFGJIT)
        return &RegisterAtOffsetList::dfgCalleeSaveRegisters();
#endif
#if !ENABLE(C_LOOP)
    return &RegisterAtOffsetList::llintBaselineCalleeSaveRegisters();
#else
    return nullptr;
#endif
}

JITCode::CodeRef<JSEntryPtrTag> JITCode::swapCodeRefForDebugger(JITCode::CodeRef<JSEntryPtrTag>)
{
    return CodeRef<JSEntryPtrTag>();
}

JITCodeWithCodeRef::JITCodeWithCodeRef(JITType jitType)
    : JITCode(jitType)
{
}

JITCodeWithCodeRef::JITCodeWithCodeRef(CodeRef<JSEntryPtrTag> ref, JITType jitType, JITCode::ShareAttribute shareAttribute)
    : JITCode(jitType, ref.code(), shareAttribute)
    , m_executableMemory(ref.executableMemory())
{
}

JITCodeWithCodeRef::~JITCodeWithCodeRef()
{
    if ((Options::dumpDisassembly() || (isOptimizingJIT(jitType()) && Options::dumpDFGDisassembly()))
        && m_executableMemory)
        dataLog("Destroying JIT code at ", pointerDump(m_executableMemory.get()), "\n");
}

void* JITCodeWithCodeRef::executableAddressAtOffset(size_t offset)
{
    RELEASE_ASSERT(m_addressForCall);
    assertIsTaggedWith<JSEntryPtrTag>(m_addressForCall.taggedPtr());
    if (!offset)
        return m_addressForCall.taggedPtr();

    char* executableAddress = untagCodePtr<char*, JSEntryPtrTag>(m_addressForCall.taggedPtr());
    return tagCodePtr<JSEntryPtrTag>(executableAddress + offset);
}

void* JITCodeWithCodeRef::dataAddressAtOffset(size_t offset)
{
    RELEASE_ASSERT(m_addressForCall);
    ASSERT(offset <= size()); // use <= instead of < because it is valid to ask for an address at the exclusive end of the code.
    return m_addressForCall.dataLocation<char*>() + offset;
}

unsigned JITCodeWithCodeRef::offsetOf(void* pointerIntoCode)
{
    RELEASE_ASSERT(m_addressForCall);
    intptr_t result = reinterpret_cast<intptr_t>(pointerIntoCode) - m_addressForCall.taggedPtr<intptr_t>();
    ASSERT(static_cast<intptr_t>(static_cast<unsigned>(result)) == result);
    return static_cast<unsigned>(result);
}

size_t JITCodeWithCodeRef::size()
{
    RELEASE_ASSERT(m_executableMemory);
    return m_executableMemory->sizeInBytes();
}

bool JITCodeWithCodeRef::contains(void* address)
{
    RELEASE_ASSERT(m_executableMemory);
    return m_executableMemory->contains(address);
}

JITCode::CodeRef<JSEntryPtrTag> JITCodeWithCodeRef::swapCodeRefForDebugger(JITCode::CodeRef<JSEntryPtrTag> ref)
{
    ASSERT(canSwapCodeRefForDebugger());
    RELEASE_ASSERT(m_addressForCall);
    RELEASE_ASSERT(ref);
    auto old = CodeRef<JSEntryPtrTag>::createSelfManagedCodeRef(m_addressForCall);
    ASSERT(!m_executableMemory);
    m_addressForCall = ref.code();
    m_executableMemory = ref.executableMemory();
    return old;
}

DirectJITCode::DirectJITCode(JITType jitType)
    : JITCodeWithCodeRef(jitType)
{
}

DirectJITCode::DirectJITCode(JITCode::CodeRef<JSEntryPtrTag> ref, CodePtr<JSEntryPtrTag> withArityCheck, JITType jitType, JITCode::ShareAttribute shareAttribute)
    : JITCodeWithCodeRef(ref, jitType, shareAttribute)
    , m_withArityCheck(withArityCheck)
{
    ASSERT(m_addressForCall);
    ASSERT(m_withArityCheck);
}

DirectJITCode::DirectJITCode(JITCode::CodeRef<JSEntryPtrTag> ref, CodePtr<JSEntryPtrTag> withArityCheck, JITType jitType, Intrinsic intrinsic, JITCode::ShareAttribute shareAttribute)
    : JITCodeWithCodeRef(ref, jitType, shareAttribute)
    , m_withArityCheck(withArityCheck)
{
    m_intrinsic = intrinsic;
    ASSERT(m_addressForCall);
    ASSERT(m_withArityCheck);
}

DirectJITCode::~DirectJITCode()
{
}

void DirectJITCode::initializeCodeRefForDFG(JITCode::CodeRef<JSEntryPtrTag> ref, CodePtr<JSEntryPtrTag> withArityCheck)
{
    RELEASE_ASSERT(!m_addressForCall);
    m_addressForCall = ref.code();
    m_executableMemory = ref.executableMemory();
    m_withArityCheck = withArityCheck;
    ASSERT(m_addressForCall);
    ASSERT(m_withArityCheck);
}

CodePtr<JSEntryPtrTag> DirectJITCode::addressForCall(ArityCheckMode arity)
{
    switch (arity) {
    case ArityCheckNotRequired:
        RELEASE_ASSERT(m_addressForCall);
        return m_addressForCall;
    case MustCheckArity:
        RELEASE_ASSERT(m_withArityCheck);
        return m_withArityCheck;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return CodePtr<JSEntryPtrTag>();
}

NativeJITCode::NativeJITCode(JITType jitType)
    : JITCodeWithCodeRef(jitType)
{
}

NativeJITCode::NativeJITCode(CodeRef<JSEntryPtrTag> ref, JITType jitType, Intrinsic intrinsic, JITCode::ShareAttribute shareAttribute)
    : JITCodeWithCodeRef(ref, jitType, shareAttribute)
{
    m_intrinsic = intrinsic;
}

NativeJITCode::~NativeJITCode()
{
}

CodePtr<JSEntryPtrTag> NativeJITCode::addressForCall(ArityCheckMode arity)
{
    RELEASE_ASSERT(m_addressForCall);
    ASSERT_UNUSED(arity, arity == ArityCheckNotRequired || arity == MustCheckArity);
    return m_addressForCall;
}

NativeDOMJITCode::NativeDOMJITCode(CodeRef<JSEntryPtrTag> ref, JITType type, Intrinsic intrinsic, const DOMJIT::Signature* signature)
    : NativeJITCode(ref, type, intrinsic)
    , m_signature(signature)
{
}

#if ENABLE(JIT)
RegisterSetBuilder JITCode::liveRegistersToPreserveAtExceptionHandlingCallSite(CodeBlock*, CallSiteIndex)
{
    return { };
}
#endif

} // namespace JSC

namespace WTF {

void printInternal(PrintStream& out, JSC::JITType type)
{
    out.print(JSC::JITCode::typeName(type));
}

} // namespace WTF

