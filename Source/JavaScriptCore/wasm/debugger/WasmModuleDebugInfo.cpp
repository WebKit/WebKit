/*
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
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
 * EXEMPLARY, OR CONSEQUARY DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WasmModuleDebugInfo.h"

#if ENABLE(WEBASSEMBLY_DEBUGGER)

#include "Options.h"
#include "WasmIPIntGenerator.h"
#include "WasmModuleInformation.h"
#include <wtf/DataLog.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

// Forward declaration to ensure proper linkage
namespace JSC {
namespace Wasm {
void parseForDebugInfo(std::span<const uint8_t>, const RTT&, ModuleInformation&, FunctionCodeIndex, FunctionDebugInfo&);
}
}

namespace JSC {
namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ModuleDebugInfo);

UncheckedKeyHashSet<uint32_t>* FunctionDebugInfo::findNextInstructions(uint32_t offset)
{
    auto itr = offsetToNextInstructions.find(offset);
    return itr == offsetToNextInstructions.end() ? nullptr : &itr->value;
}

void FunctionDebugInfo::addNextInstruction(uint32_t offset, uint32_t nextInstruction)
{
    dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleDebugInfo] addNextInstruction offset:", RawHex(offset), " nextInstruction:", RawHex(nextInstruction));
    offsetToNextInstructions.add(offset, UncheckedKeyHashSet<uint32_t>()).iterator->value.add(nextInstruction);
}

void FunctionDebugInfo::addLocalType(Type type)
{
    dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleDebugInfo] addLocalType type:", type);
    locals.append(type);
}


} // namespace Wasm
} // namespace JSC

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
