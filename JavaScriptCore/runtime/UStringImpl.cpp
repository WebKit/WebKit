/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
#include "UStringImpl.h"

#include "Identifier.h"
#include "UString.h"
#include "UTF8.h"

using namespace WTF::Unicode;
using namespace std;

namespace JSC {
 
SharedUChar* UStringImpl::baseSharedBuffer()
{
    ASSERT((bufferOwnership() == BufferShared)
        || ((bufferOwnership() == BufferOwned) && !m_dataBuffer.asPtr<void*>()));

    if (bufferOwnership() != BufferShared)
        m_dataBuffer = UntypedPtrAndBitfield(SharedUChar::create(new OwnFastMallocPtr<UChar>(m_data)).releaseRef(), BufferShared);

    return m_dataBuffer.asPtr<SharedUChar*>();
}

SharedUChar* UStringImpl::sharedBuffer()
{
    if (m_length < s_minLengthToShare)
        return 0;
    ASSERT(!isStatic());

    UStringImpl* owner = bufferOwnerString();
    if (owner->bufferOwnership() == BufferInternal)
        return 0;

    return owner->baseSharedBuffer();
}

void UStringImpl::destroy()
{
    ASSERT(!isStatic());
    checkConsistency();

    if (identifierTable())
        Identifier::remove(this);

    if (bufferOwnership() != BufferInternal) {
        if (bufferOwnership() == BufferOwned)
            fastFree(m_data);
        else if (bufferOwnership() == BufferSubstring)
            m_dataBuffer.asPtr<UStringImpl*>()->deref();
        else {
            ASSERT(bufferOwnership() == BufferShared);
            m_dataBuffer.asPtr<SharedUChar*>()->deref();
        }
    }

    delete this;
}

// Golden ratio - arbitrary start value to avoid mapping all 0's to all 0's
// or anything like that.
const unsigned PHI = 0x9e3779b9U;

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html
unsigned UStringImpl::computeHash(const UChar* s, int len)
{
    unsigned l = len;
    uint32_t hash = PHI;
    uint32_t tmp;

    int rem = l & 1;
    l >>= 1;

    // Main loop
    for (; l > 0; l--) {
        hash += s[0];
        tmp = (s[1] << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        s += 2;
        hash += hash >> 11;
    }

    // Handle end case
    if (rem) {
        hash += s[0];
        hash ^= hash << 11;
        hash += hash >> 17;
    }

    // Force "avalanching" of final 127 bits
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 2;
    hash += hash >> 15;
    hash ^= hash << 10;

    // this avoids ever returning a hash code of 0, since that is used to
    // signal "hash not computed yet", using a value that is likely to be
    // effectively the same as 0 when the low bits are masked
    if (hash == 0)
        hash = 0x80000000;

    return hash;
}

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html
unsigned UStringImpl::computeHash(const char* s, int l)
{
    // This hash is designed to work on 16-bit chunks at a time. But since the normal case
    // (above) is to hash UTF-16 characters, we just treat the 8-bit chars as if they
    // were 16-bit chunks, which should give matching results

    uint32_t hash = PHI;
    uint32_t tmp;

    size_t rem = l & 1;
    l >>= 1;

    // Main loop
    for (; l > 0; l--) {
        hash += static_cast<unsigned char>(s[0]);
        tmp = (static_cast<unsigned char>(s[1]) << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        s += 2;
        hash += hash >> 11;
    }

    // Handle end case
    if (rem) {
        hash += static_cast<unsigned char>(s[0]);
        hash ^= hash << 11;
        hash += hash >> 17;
    }

    // Force "avalanching" of final 127 bits
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 2;
    hash += hash >> 15;
    hash ^= hash << 10;

    // this avoids ever returning a hash code of 0, since that is used to
    // signal "hash not computed yet", using a value that is likely to be
    // effectively the same as 0 when the low bits are masked
    if (hash == 0)
        hash = 0x80000000;

    return hash;
}

}
