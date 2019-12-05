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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "B3Common.h"
#include "B3Compilation.h"
#include "B3OpaqueByproducts.h"
#include "CCallHelpers.h"
#include "WasmCompilationMode.h"
#include "WasmEmbedder.h"
#include "WasmMemory.h"
#include "WasmModuleInformation.h"
#include "WasmTierUpCount.h"
#include <wtf/Expected.h>

extern "C" void dumpProcedure(void*);

namespace JSC { namespace Wasm {

class MemoryInformation;

struct CompilationContext {
    std::unique_ptr<CCallHelpers> embedderEntrypointJIT;
    std::unique_ptr<B3::OpaqueByproducts> embedderEntrypointByproducts;
    std::unique_ptr<CCallHelpers> wasmEntrypointJIT;
    std::unique_ptr<B3::OpaqueByproducts> wasmEntrypointByproducts;
};

Expected<std::unique_ptr<InternalFunction>, String> parseAndCompile(CompilationContext&, const FunctionData&, const Signature&, Vector<UnlinkedWasmToWasmCall>&, unsigned& osrEntryScratchBufferSize, const ModuleInformation&, MemoryMode, CompilationMode, uint32_t functionIndex, uint32_t loopIndexForOSREntry, TierUpCount* = nullptr);

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
