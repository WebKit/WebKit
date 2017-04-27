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

#include "B3Compilation.h"
#include "RegisterAtOffsetList.h"
#include "WasmFormat.h"
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC { namespace Wasm {

class Callee : public ThreadSafeRefCounted<Callee> {
    WTF_MAKE_FAST_ALLOCATED;
public:

    // We use this when we're the JS entrypoint, we don't ascribe an index to those.
    static constexpr unsigned invalidCalleeIndex = UINT_MAX;

    static Ref<Callee> create(Wasm::Entrypoint&& entrypoint, unsigned index = invalidCalleeIndex)
    {
        Callee* callee = new Callee(WTFMove(entrypoint), index);
        return adoptRef(*callee);
    }

    void* entrypoint() const { return m_entrypoint.compilation->code().executableAddress(); }

    RegisterAtOffsetList* calleeSaveRegisters() { return &m_entrypoint.calleeSaveRegisters; }
    std::optional<unsigned> index() const
    {
        if (m_index == invalidCalleeIndex)
            return std::nullopt;
        return m_index;
    }

private:
    JS_EXPORT_PRIVATE Callee(Wasm::Entrypoint&&, unsigned index);

    Wasm::Entrypoint m_entrypoint;
    unsigned m_index;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
