/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
#include "FTLOSRExit.h"

#if ENABLE(FTL_JIT)

#include "B3StackmapGenerationParams.h"
#include "FTLJITCode.h"
#include "FTLState.h"

namespace JSC { namespace FTL {

using namespace B3;
using namespace DFG;

OSRExitDescriptor::OSRExitDescriptor(
    DataFormat profileDataFormat, MethodOfGettingAValueProfile valueProfile,
    unsigned numberOfArguments, unsigned numberOfLocals, unsigned numberOfTmps)
    : m_profileDataFormat(profileDataFormat)
    , m_valueProfile(valueProfile)
    , m_values(numberOfArguments, numberOfLocals, numberOfTmps)
{
}

void OSRExitDescriptor::validateReferences(const TrackedReferences& trackedReferences)
{
    for (unsigned i = m_values.size(); i--;)
        m_values[i].validateReferences(trackedReferences);
    
    for (ExitTimeObjectMaterialization* materialization : m_materializations)
        materialization->validateReferences(trackedReferences);
}

Ref<OSRExitHandle> OSRExitDescriptor::emitOSRExit(
    State& state, ExitKind exitKind, const NodeOrigin& nodeOrigin, CCallHelpers& jit,
    const StackmapGenerationParams& params, uint32_t dfgNodeIndex, unsigned offset)
{
    Ref<OSRExitHandle> handle =
        prepareOSRExitHandle(state, exitKind, nodeOrigin, params, dfgNodeIndex, offset);
    handle->emitExitThunk(state, jit);
    return handle;
}

Ref<OSRExitHandle> OSRExitDescriptor::emitOSRExitLater(
    State& state, ExitKind exitKind, const NodeOrigin& nodeOrigin,
    const StackmapGenerationParams& params, uint32_t dfgNodeIndex, unsigned offset)
{
    RefPtr<OSRExitHandle> handle =
        prepareOSRExitHandle(state, exitKind, nodeOrigin, params, dfgNodeIndex, offset);
    params.addLatePath(
        [handle, &state] (CCallHelpers& jit) {
            handle->emitExitThunk(state, jit);
        });
    return handle.releaseNonNull();
}

Ref<OSRExitHandle> OSRExitDescriptor::prepareOSRExitHandle(
    State& state, ExitKind exitKind, const NodeOrigin& nodeOrigin,
    const StackmapGenerationParams& params, uint32_t dfgNodeIndex, unsigned offset)
{
    FixedVector<B3::ValueRep> valueReps(params.size() - offset);
    for (unsigned i = offset, indexInValueReps = 0; i < params.size(); ++i, ++indexInValueReps)
        valueReps[indexInValueReps] = params[i];
    unsigned index = state.jitCode->m_osrExit.size();
    state.jitCode->m_osrExit.append(OSRExit(this, exitKind, nodeOrigin.forExit, nodeOrigin.semantic, nodeOrigin.wasHoisted, dfgNodeIndex, WTFMove(valueReps)));
    return adoptRef(*new OSRExitHandle(index, state.jitCode.get()));
}

OSRExit::OSRExit(
    OSRExitDescriptor* descriptor, ExitKind exitKind, CodeOrigin codeOrigin,
    CodeOrigin codeOriginForExitProfile, bool wasHoisted, uint32_t dfgNodeIndex, FixedVector<B3::ValueRep>&& valueReps)
    : OSRExitBase(exitKind, codeOrigin, codeOriginForExitProfile, wasHoisted, dfgNodeIndex)
    , m_descriptor(descriptor)
    , m_valueReps(WTFMove(valueReps))
{
}

CodeLocationJump<JSInternalPtrTag> OSRExit::codeLocationForRepatch(CodeBlock* ftlCodeBlock) const
{
    UNUSED_PARAM(ftlCodeBlock);
    return m_patchableJump;
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)
