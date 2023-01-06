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

#include "InternalFunction.h"
#include "WasmB3IRGenerator.h"
#include "WasmFormat.h"
#include "WasmMemory.h"
#include "WasmModuleInformation.h"
#include "WasmTypeDefinition.h"
#include <wtf/Forward.h>

namespace JSC {

class CCallHelpers;

namespace Wasm {

struct CallInformation;

void marshallJSResult(CCallHelpers& jit, const TypeDefinition&, const CallInformation& wasmFrameConvention, const RegisterAtOffsetList& savedResultRegisters);
std::unique_ptr<InternalFunction> createJSToWasmWrapper(CCallHelpers&, Callee&, const TypeDefinition&, Vector<UnlinkedWasmToWasmCall>*, const ModuleInformation&, MemoryMode, uint32_t functionIndex);

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
