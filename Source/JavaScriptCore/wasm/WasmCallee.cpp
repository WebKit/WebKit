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

#include "config.h"
#include "WasmCallee.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmCalleeRegistry.h"

namespace JSC { namespace Wasm {

Callee::Callee(Wasm::CompilationMode compilationMode, Entrypoint&& entrypoint)
    : m_compilationMode(compilationMode)
    , m_entrypoint(WTFMove(entrypoint))
{
    CalleeRegistry::singleton().registerCallee(this);
}

Callee::Callee(Wasm::CompilationMode compilationMode, Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name)
    : m_compilationMode(compilationMode)
    , m_entrypoint(WTFMove(entrypoint))
    , m_indexOrName(index, WTFMove(name))
{
    CalleeRegistry::singleton().registerCallee(this);
}

Callee::~Callee()
{
    CalleeRegistry::singleton().unregisterCallee(this);
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
