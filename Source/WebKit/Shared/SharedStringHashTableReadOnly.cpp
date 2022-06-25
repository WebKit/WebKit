/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SharedStringHashTableReadOnly.h"

#include "SharedMemory.h"

namespace WebKit {

using namespace WebCore;

#if ASSERT_ENABLED
static inline bool isPowerOf2(unsigned v)
{
    // Taken from http://www.cs.utk.edu/~vose/c-stuff/bithacks.html

    return !(v & (v - 1)) && v;
}
#endif

static inline unsigned doubleHash(unsigned key)
{
    key = ~key + (key >> 23);
    key ^= (key << 12);
    key ^= (key >> 7);
    key ^= (key << 2);
    key ^= (key >> 20);
    return key;
}

SharedStringHashTableReadOnly::SharedStringHashTableReadOnly()
{
}

SharedStringHashTableReadOnly::~SharedStringHashTableReadOnly()
{
}

void SharedStringHashTableReadOnly::setSharedMemory(RefPtr<SharedMemory>&& sharedMemory)
{
    m_sharedMemory = WTFMove(sharedMemory);

    if (m_sharedMemory) {
        ASSERT(!(m_sharedMemory->size() % sizeof(SharedStringHash)));
        m_table = static_cast<SharedStringHash*>(m_sharedMemory->data());
        m_tableSize = m_sharedMemory->size() / sizeof(SharedStringHash);
        ASSERT(isPowerOf2(m_tableSize));
        m_tableSizeMask = m_tableSize - 1;
    } else {
        m_table = nullptr;
        m_tableSize = 0;
        m_tableSizeMask = 0;
    }
}

bool SharedStringHashTableReadOnly::contains(SharedStringHash sharedStringHash) const
{
    auto* slot = findSlot(sharedStringHash);
    return slot && *slot;
}

SharedStringHash* SharedStringHashTableReadOnly::findSlot(SharedStringHash sharedStringHash) const
{
    if (!m_sharedMemory)
        return nullptr;

    int k = 0;
    SharedStringHash* table = m_table;
    int sizeMask = m_tableSizeMask;
    unsigned h = static_cast<unsigned>(sharedStringHash);
    int i = h & sizeMask;

    SharedStringHash* entry;
    while (1) {
        entry = table + i;

        // Check if we've reached the end of the table.
        if (!*entry)
            return entry;

        if (*entry == sharedStringHash)
            return entry;

        if (!k)
            k = 1 | doubleHash(h);
        i = (i + k) & sizeMask;
    }
}

} // namespace WebKit
