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

#include "config.h"
#include "WasmCallProfile.h"

#include "WasmMergedProfile.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(WEBASSEMBLY)

namespace JSC::Wasm {

CallProfile::~CallProfile()
{
    // Do not use ::polymorphic as it does not extract a callee when it is already megamorphic.
    if (m_boxedCallee & Polymorphic) {
        if (auto* poly = std::bit_cast<PolymorphicCallee*>(static_cast<uintptr_t>(m_boxedCallee & ~calleeMask)))
            delete poly;
    }
}

auto CallProfile::makePolymorphic() -> PolymorphicCallee*
{
    ASSERT(monomorphic(m_boxedCallee));
    auto* poly = PolymorphicCallee::create(maxPolymorphicCallees, this).release();
    poly->at(0).m_count = m_count - 1; // The current ongoing count should not be included.
    poly->at(0).m_boxedCallee = m_boxedCallee;
    EncodedJSValue boxedCallee = (std::bit_cast<uintptr_t>(poly) | Polymorphic);
    WTF::storeStoreFence();
    m_boxedCallee = boxedCallee;
    return poly;
}

} // namespace JSC::Wasm

#endif
