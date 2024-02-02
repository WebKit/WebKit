/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "WasmCallsiteCollection.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmCallee.h"
#include "WasmCalleeGroup.h"
#include "WasmMachineThreads.h"
#include <wtf/DataLog.h>
#include <wtf/Locker.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace Wasm {

#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
namespace WasmCallsiteCollectionInternal {
static constexpr bool verbose = false;
}
#endif

void CallsiteCollection::addCallsites(const AbstractLocker& calleeGroupLocker, CalleeGroup& calleeGroup, const FixedVector<UnlinkedWasmToWasmCall>& callsites)
{
    UNUSED_PARAM(calleeGroupLocker);
    unsigned functionImportCount = calleeGroup.functionImportCount();
    for (auto& callsite : callsites) {
        unsigned calleeIndex = callsite.functionIndexSpace - functionImportCount;
        m_callsites[calleeIndex].append(Callsite {
            callsite.callLocation,
            { }
        });
    }
}

void CallsiteCollection::addCalleeGroupCallsites(const AbstractLocker& calleeGroupLocker, CalleeGroup& calleeGroup, Vector<Vector<UnlinkedWasmToWasmCall>>&& callsitesList)
{
    UNUSED_PARAM(calleeGroupLocker);
    unsigned functionImportCount = calleeGroup.functionImportCount();
    for (auto& callsites : callsitesList) {
        for (auto& callsite : callsites) {
            unsigned calleeIndex = callsite.functionIndexSpace - functionImportCount;
            m_callsites[calleeIndex].append(Callsite {
                callsite.callLocation,
                { }
            });
        }
    }
    m_calleeGroupCallsites = WTFMove(callsitesList);
}

#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
void CallsiteCollection::updateCallsitesToCallUs(const AbstractLocker& calleeGroupLocker, CalleeGroup& calleeGroup, CodeLocationLabel<WasmEntryPtrTag> entrypoint, uint32_t functionIndex, uint32_t functionIndexSpace)
{
    UNUSED_PARAM(calleeGroupLocker);
    UNUSED_PARAM(functionIndexSpace);

    for (auto& callsite : m_callsites[functionIndex])
        callsite.m_target = MacroAssembler::prepareForAtomicRepatchNearCallConcurrently(callsite.m_callLocation, entrypoint);

    // It's important to make sure we do this before we make any of the code we just compiled visible. If we didn't, we could end up
    // where we are tiering up some function A to A' and we repatch some function B to call A' instead of A. Another CPU could see
    // the updates to B but still not have reset its cache of A', which would lead to all kinds of badness.
    resetInstructionCacheOnAllThreads();
    WTF::storeStoreFence(); // This probably isn't necessary but it's good to be paranoid.

    calleeGroup.m_wasmIndirectCallEntryPoints[functionIndex] = entrypoint;
    calleeGroup.m_wasmIndirectCallWasmCallees[functionIndex] = nullptr;

    for (auto& callsite : m_callsites[functionIndex]) {
        dataLogLnIf(WasmCallsiteCollectionInternal::verbose, "Repatching call at: ", RawPointer(callsite.m_callLocation.dataLocation()), " to ", RawPointer(entrypoint.taggedPtr()));
        MacroAssembler::repatchNearCall(callsite.m_callLocation, callsite.m_target);
    }
}
#endif

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
