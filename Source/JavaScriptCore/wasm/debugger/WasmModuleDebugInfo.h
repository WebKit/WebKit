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
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Platform.h>

#if ENABLE(WEBASSEMBLY_DEBUGGER)

#include <JavaScriptCore/JSExportMacros.h>
#include <cstdint>
#include <wtf/DataLog.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace JSC {
namespace Wasm {

struct Type;
struct ModuleInformation;
class FunctionCodeIndex;

struct FunctionDebugInfo {
    JS_EXPORT_PRIVATE UncheckedKeyHashSet<uint32_t>* findNextInstructions(uint32_t offset);
    void addNextInstruction(uint32_t offset, uint32_t nextInstruction);
    void addLocalType(Type);

    using OffsetToNextInstructions = UncheckedKeyHashMap<uint32_t, UncheckedKeyHashSet<uint32_t>, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;
    OffsetToNextInstructions offsetToNextInstructions;
    Vector<Type> locals;
};

struct ModuleDebugInfo {
    WTF_MAKE_TZONE_ALLOCATED(ModuleDebugInfo);

public:
    ModuleDebugInfo(ModuleInformation& moduleInfo)
        : moduleInfo(moduleInfo)
    {
    }

    void takeSource(Vector<uint8_t>&& source) { this->source = WTF::move(source); }
    FunctionDebugInfo& ensureFunctionDebugInfo(FunctionCodeIndex);

    Ref<ModuleInformation> moduleInfo;
    uint32_t id { 0 };
    Vector<uint8_t> source;
    using FunctionIndexToData = UncheckedKeyHashMap<size_t, FunctionDebugInfo, DefaultHash<size_t>, WTF::UnsignedWithZeroKeyHashTraits<size_t>>;
    FunctionIndexToData functionIndexToData;
};

} // namespace Wasm
} // namespace JSC

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
