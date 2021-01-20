/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#include "IsoTLSLayout.h"

#include "IsoTLSEntry.h"

namespace bmalloc {

DEFINE_STATIC_PER_PROCESS_STORAGE(IsoTLSLayout);

IsoTLSLayout::IsoTLSLayout(const LockHolder&)
{
}

void IsoTLSLayout::add(IsoTLSEntry* entry)
{
    static Mutex addingMutex;
    RELEASE_BASSERT(!entry->m_next);
    // IsoTLSLayout::head() does not take a lock. So we should emit memory fence to make sure that newly added entry is initialized when it is chained to this linked-list.
    // Emitting memory fence here is OK since this function is not frequently called.
    LockHolder locking(addingMutex);
    if (m_head) {
        RELEASE_BASSERT(m_tail);
        size_t offset = roundUpToMultipleOf(entry->alignment(), m_tail->extent());
        RELEASE_BASSERT(offset < UINT_MAX);
        entry->m_offset = offset;
        std::atomic_thread_fence(std::memory_order_seq_cst);
        m_tail->m_next = entry;
        m_tail = entry;
    } else {
        RELEASE_BASSERT(!m_tail);
        entry->m_offset = 0;
        std::atomic_thread_fence(std::memory_order_seq_cst);
        m_head = entry;
        m_tail = entry;
    }
}

} // namespace bmalloc

