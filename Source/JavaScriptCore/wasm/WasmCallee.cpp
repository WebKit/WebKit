/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "WasmCallee.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmCalleeRegistry.h"

namespace JSC { namespace Wasm {

Callee::Callee(Wasm::CompilationMode compilationMode)
    : m_compilationMode(compilationMode)
{
    CalleeRegistry::singleton().registerCallee(this);
}

Callee::Callee(Wasm::CompilationMode compilationMode, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name)
    : m_compilationMode(compilationMode)
    , m_indexOrName(index, WTFMove(name))
{
    CalleeRegistry::singleton().registerCallee(this);
}

Callee::~Callee()
{
    CalleeRegistry::singleton().unregisterCallee(this);
}


inline void repatchMove(const AbstractLocker&, CodeLocationDataLabelPtr<WasmEntryPtrTag> moveLocation, Callee& targetCallee)
{
    MacroAssembler::repatchPointer(moveLocation, CalleeBits::boxWasm(&targetCallee));
}

inline void repatchCall(const AbstractLocker&, CodeLocationNearCall<WasmEntryPtrTag> callLocation, Callee& targetCallee)
{
    MacroAssembler::repatchNearCall(callLocation, CodeLocationLabel<WasmEntryPtrTag>(targetCallee.code()));
}

void BBQCallee::addCaller(const AbstractLocker&, LinkBuffer& linkBuffer, UnlinkedMoveAndCall call)
{
    auto moveLocation = linkBuffer.locationOf<WasmEntryPtrTag>(call.moveLocation);
    auto callLocation = linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call.callLocation);
    m_moveLocations.append(moveLocation);
    m_callLocations.append(callLocation);
}

void BBQCallee::addAndLinkCaller(const AbstractLocker& locker, LinkBuffer& linkBuffer, UnlinkedMoveAndCall call)
{
    RELEASE_ASSERT(entrypoint().compilation);

    auto moveLocation = linkBuffer.locationOf<WasmEntryPtrTag>(call.moveLocation);
    auto callLocation = linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call.callLocation);
    m_moveLocations.append(moveLocation);
    m_callLocations.append(callLocation);

    Callee& targetCallee = m_replacement ? static_cast<Callee&>(*m_replacement) : *this;
    repatchMove(locker, moveLocation, targetCallee);
    repatchCall(locker, callLocation, targetCallee);
}

void BBQCallee::repatchCallers(const AbstractLocker& locker, Callee& targetCallee)
{
    RELEASE_ASSERT(targetCallee.entrypoint().compilation);
    for (auto moveLocation : m_moveLocations)
        repatchMove(locker, moveLocation, targetCallee);
    for (auto callLocation : m_callLocations)
        repatchCall(locker, callLocation, targetCallee);
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
