/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#include <wtf/Poisoned.h>

namespace JSC {

// Let's keep the following list of poisons in alphabetical order just so it's easier to read.
#define FOR_EACH_JSC_POISON(v) \
    v(ArrayPrototype) \
    v(CodeBlock) \
    v(DateInstance) \
    v(GlobalData) \
    v(JITCode) \
    v(JSAPIWrapperObject) \
    v(JSArrayBuffer) \
    v(JSCallbackObject) \
    v(JSFunction) \
    v(JSGlobalObject) \
    v(JSScriptFetchParameters) \
    v(JSScriptFetcher) \
    v(JSWebAssemblyCodeBlock) \
    v(JSWebAssemblyInstance) \
    v(JSWebAssemblyMemory) \
    v(JSWebAssemblyModule) \
    v(JSWebAssemblyTable) \
    v(NativeCode) \
    v(ScopedArguments) \
    v(StructureTransitionTable) \
    v(UnlinkedSourceCode) \
    v(WebAssemblyFunctionBase) \
    v(WebAssemblyModuleRecord) \
    v(WebAssemblyToJSCallee) \
    v(WebAssemblyWrapperFunction) \

#define POISON_KEY_NAME(_poisonID_) g_##_poisonID_##Poison

#define DECLARE_POISON(_poisonID_) \
    extern "C" JS_EXPORT_PRIVATE uintptr_t POISON_KEY_NAME(_poisonID_); \
    using _poisonID_ ## Poison = Poison<POISON_KEY_NAME(_poisonID_)>;

FOR_EACH_JSC_POISON(DECLARE_POISON)
#undef DECLARE_POISON

struct ClassInfo;

using PoisonedClassInfoPtr = Poisoned<GlobalDataPoison, const ClassInfo*>;
using PoisonedMasmPtr = Poisoned<JITCodePoison, void*>;

void initializePoison();

} // namespace JSC
