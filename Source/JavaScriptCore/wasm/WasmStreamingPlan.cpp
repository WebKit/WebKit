/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "WasmStreamingPlan.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmCallee.h"
#include "WasmLLIntPlan.h"
#include "WasmNameSection.h"
#include "WasmSignatureInlines.h"
#include <wtf/DataLog.h>
#include <wtf/Locker.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace Wasm {

namespace WasmStreamingPlanInternal {
static constexpr bool verbose = false;
}

StreamingPlan::StreamingPlan(Context* context, Ref<ModuleInformation>&& info, Ref<LLIntPlan>&& plan, uint32_t functionIndex, CompletionTask&& task)
    : Base(context, WTFMove(info), WTFMove(task))
    , m_plan(WTFMove(plan))
    , m_functionIndex(functionIndex)
{
    dataLogLnIf(WasmStreamingPlanInternal::verbose, "Starting Streaming plan for ", functionIndex, " of module info: ", RawPointer(&m_moduleInformation.get()));
}

void StreamingPlan::work(CompilationEffort)
{
    m_plan->compileFunction(m_functionIndex);
    dataLogLnIf(WasmStreamingPlanInternal::verbose, "Finished Streaming ", m_functionIndex);
    Locker locker { m_lock };
    complete();
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
