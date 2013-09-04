/*
 * Copyright (C) 2008, 2012, 2013 Apple Inc. All rights reserved.
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

#include "Operations.h"
#include <wtf/PrintStream.h>

namespace JSC {

JITCode::JITCode(JITType jitType)
    : m_jitType(jitType)
{
}

JITCode::~JITCode()
{
}

#if ENABLE(JIT)
JSValue JITCode::execute(JSStack* stack, CallFrame* callFrame, VM* vm)
{
    JSValue result = JSValue::decode(ctiTrampoline(executableAddress(), stack, callFrame, 0, 0, vm));
    return vm->exception() ? jsNull() : result;
}
#endif

DFG::CommonData* JITCode::dfgCommon()
{
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

DFG::JITCode* JITCode::dfg()
{
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

FTL::JITCode* JITCode::ftl()
{
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

FTL::ForOSREntryJITCode* JITCode::ftlForOSREntry()
{
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

PassRefPtr<JITCode> JITCode::hostFunction(JITCode::CodeRef code)
{
    return adoptRef(new DirectJITCode(code, HostCallThunk));
}

DirectJITCode::DirectJITCode(JITType jitType)
    : JITCode(jitType)
{
}

DirectJITCode::DirectJITCode(const JITCode::CodeRef ref, JITType jitType)
    : JITCode(jitType)
    , m_ref(ref)
{
}

DirectJITCode::~DirectJITCode()
{
}

void DirectJITCode::initializeCodeRef(const JITCode::CodeRef ref)
{
    RELEASE_ASSERT(!m_ref);
    m_ref = ref;
}

JITCode::CodePtr DirectJITCode::addressForCall()
{
    RELEASE_ASSERT(m_ref);
    return m_ref.code();
}

void* DirectJITCode::executableAddressAtOffset(size_t offset)
{
    RELEASE_ASSERT(m_ref);
    return reinterpret_cast<char*>(m_ref.code().executableAddress()) + offset;
}

void* DirectJITCode::dataAddressAtOffset(size_t offset)
{
    RELEASE_ASSERT(m_ref);
    ASSERT(offset <= size()); // use <= instead of < because it is valid to ask for an address at the exclusive end of the code.
    return reinterpret_cast<char*>(m_ref.code().dataLocation()) + offset;
}

unsigned DirectJITCode::offsetOf(void* pointerIntoCode)
{
    RELEASE_ASSERT(m_ref);
    intptr_t result = reinterpret_cast<intptr_t>(pointerIntoCode) - reinterpret_cast<intptr_t>(m_ref.code().executableAddress());
    ASSERT(static_cast<intptr_t>(static_cast<unsigned>(result)) == result);
    return static_cast<unsigned>(result);
}

size_t DirectJITCode::size()
{
    RELEASE_ASSERT(m_ref);
    return m_ref.size();
}

bool DirectJITCode::contains(void* address)
{
    RELEASE_ASSERT(m_ref);
    return m_ref.executableMemory()->contains(address);
}

} // namespace JSC

namespace WTF {

void printInternal(PrintStream& out, JSC::JITCode::JITType type)
{
    switch (type) {
    case JSC::JITCode::None:
        out.print("None");
        return;
    case JSC::JITCode::HostCallThunk:
        out.print("Host");
        return;
    case JSC::JITCode::InterpreterThunk:
        out.print("LLInt");
        return;
    case JSC::JITCode::BaselineJIT:
        out.print("Baseline");
        return;
    case JSC::JITCode::DFGJIT:
        out.print("DFG");
        return;
    case JSC::JITCode::FTLJIT:
        out.print("FTL");
        return;
    default:
        CRASH();
        return;
    }
}

} // namespace WTF

