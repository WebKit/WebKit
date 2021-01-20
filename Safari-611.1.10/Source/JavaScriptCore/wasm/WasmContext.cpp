/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "WasmContext.h"

#if ENABLE(WEBASSEMBLY)

namespace JSC { namespace Wasm {

uint64_t* Context::scratchBufferForSize(size_t size)
{
    if (!size)
        return nullptr;

    auto locker = holdLock(m_scratchBufferLock);
    if (size > m_sizeOfLastScratchBuffer) {
        m_sizeOfLastScratchBuffer = size * 2;

        auto newBuffer = makeUniqueArray<uint64_t>(m_sizeOfLastScratchBuffer);
        RELEASE_ASSERT(newBuffer);
        m_scratchBuffers.append(WTFMove(newBuffer));
    }
    // Scanning scratch buffers for GC is not necessary since while performing OSR entry, we do not perform GC.
    return m_scratchBuffers.last().get();
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
