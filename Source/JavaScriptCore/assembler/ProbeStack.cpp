/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "ProbeStack.h"

#include <memory>

#if ENABLE(MASM_PROBE)

namespace JSC {
namespace Probe {

Page::Page(void* baseAddress)
    : m_baseLogicalAddress(baseAddress)
{
    memcpy(&m_buffer, baseAddress, s_pageSize);
}

void Page::flushWrites()
{
    uintptr_t dirtyBits = m_dirtyBits;
    size_t offset = 0;
    while (dirtyBits) {
        // Find start.
        if (dirtyBits & 1) {
            size_t startOffset = offset;
            // Find end.
            do {
                dirtyBits = dirtyBits >> 1;
                offset += s_chunkSize;
            } while (dirtyBits & 1);

            size_t size = offset - startOffset;
            uint8_t* src = reinterpret_cast<uint8_t*>(&m_buffer) + startOffset;
            uint8_t* dst = reinterpret_cast<uint8_t*>(m_baseLogicalAddress) + startOffset;
            memcpy(dst, src, size);
        }
        dirtyBits = dirtyBits >> 1;
        offset += s_chunkSize;
    }
    m_dirtyBits = 0;
}

Stack::Stack(Stack&& other)
    : m_newStackPointer(other.m_newStackPointer)
    , m_lowWatermark(other.m_lowWatermark)
    , m_stackBounds(WTFMove(other.m_stackBounds))
    , m_pages(WTFMove(other.m_pages))
{
#if !ASSERT_DISABLED
    other.m_isValid = false;
#endif
}

bool Stack::hasWritesToFlush()
{
    return std::any_of(m_pages.begin(), m_pages.end(), [] (auto& it) { return it.value->hasWritesToFlush(); });
}

void Stack::flushWrites()
{
    for (auto it = m_pages.begin(); it != m_pages.end(); ++it)
        it->value->flushWritesIfNeeded();
}

Page* Stack::ensurePageFor(void* address)
{
    // Since the machine stack is always allocated in units of whole pages, asserting
    // that the address is contained in the stack is sufficient to infer that its page
    // is also contained in the stack.
    RELEASE_ASSERT(m_stackBounds.contains(address));

    // We may have gotten here because of a cache miss. So, look up the page first
    // before allocating a new one,
    void* baseAddress = Page::baseAddressFor(address);
    auto it = m_pages.find(baseAddress);
    if (LIKELY(it != m_pages.end()))
        m_lastAccessedPage = it->value.get();
    else {
        std::unique_ptr<Page> page = std::make_unique<Page>(baseAddress);
        auto result = m_pages.add(baseAddress, WTFMove(page));
        m_lastAccessedPage = result.iterator->value.get();
    }
    m_lastAccessedPageBaseAddress = baseAddress;
    return m_lastAccessedPage;
}

} // namespace Probe
} // namespace JSC

#endif // ENABLE(MASM_PROBE)
