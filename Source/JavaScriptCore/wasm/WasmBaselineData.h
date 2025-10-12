/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/WasmCallProfile.h>
#include <JavaScriptCore/WasmCallee.h>
#include <JavaScriptCore/WasmCallingConvention.h>
#include <wtf/text/WTFString.h>

namespace JSC::Wasm {

// Per-instance side data for wasm baseline execution (IPInt and BBQ).
// Mainly for profiling / IC.
class BaselineData final : public ThreadSafeRefCounted<BaselineData>, public TrailingArray<BaselineData, CallProfile> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(BaselineData);
    WTF_MAKE_NONMOVABLE(BaselineData);
    using TrailingArrayType = TrailingArray<BaselineData, CallProfile>;
    friend TrailingArrayType;
public:

    static Ref<BaselineData> create(const IPIntCallee& callee)
    {
        return adoptRef(*new (fastMalloc(allocationSize(callee.numCallProfiles()))) BaselineData(callee.numCallProfiles()));
    }

private:
    BaselineData(unsigned size)
        : TrailingArrayType(size)
    {
    }
};

} // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
