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

#if ENABLE(WEBASSEMBLY_OMGJIT)

#include "B3Common.h"
#include "B3Procedure.h"
#include "CCallHelpers.h"
#include "JITCompilation.h"
#include "JITOpaqueByproducts.h"
#include "PCToCodeOriginMap.h"
#include "WasmBBQDisassembler.h"
#include "WasmCompilationContext.h"
#include "WasmCompilationMode.h"
#include "WasmJS.h"
#include "WasmMemory.h"
#include "WasmModuleInformation.h"
#include "WasmTierUpCount.h"
#include <wtf/Box.h>
#include <wtf/Expected.h>

extern "C" void dumpProcedure(void*);

namespace JSC {

namespace Wasm {

Expected<std::unique_ptr<InternalFunction>, String> parseAndCompileB3(CompilationContext&, OptimizingJITCallee&, const FunctionData&, const TypeDefinition&, Vector<UnlinkedWasmToWasmCall>&, const ModuleInformation&, MemoryMode, CompilationMode, uint32_t functionIndex, std::optional<bool> hasExceptionHandlers, uint32_t loopIndexForOSREntry, TierUpCount* = nullptr);

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_OMGJIT)
