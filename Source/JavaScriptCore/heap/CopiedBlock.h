/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef CopiedBlock_h
#define CopiedBlock_h

#include "HeapBlock.h"
#include "JSValue.h"
#include "JSValueInlineMethods.h"

namespace JSC {

class CopiedSpace;

class CopiedBlock : public HeapBlock {
    friend class CopiedSpace;
    friend class CopiedAllocator;
public:
    CopiedBlock(PageAllocationAligned& allocation)
        : HeapBlock(allocation)
        , m_offset(payload())
        , m_isPinned(false)
    {
        ASSERT(is8ByteAligned(static_cast<void*>(m_offset)));
#if USE(JSVALUE64)
        char* offset = static_cast<char*>(m_offset);
        memset(static_cast<void*>(offset), 0, static_cast<size_t>((reinterpret_cast<char*>(this) + allocation.size()) - offset));
#else
        JSValue emptyValue;
        JSValue* limit = reinterpret_cast_ptr<JSValue*>(reinterpret_cast<char*>(this) + allocation.size());
        for (JSValue* currentValue = reinterpret_cast<JSValue*>(m_offset); currentValue < limit; currentValue++)
            *currentValue = emptyValue;
#endif
    }

    char* payload();
    size_t size();
    size_t capacity();

private:
    void* m_offset;
    uintptr_t m_isPinned;
};

inline char* CopiedBlock::payload()
{
    return reinterpret_cast<char*>(this) + ((sizeof(CopiedBlock) + 7) & ~7);
}

inline size_t CopiedBlock::size()
{
    return static_cast<size_t>(static_cast<char*>(m_offset) - payload());
}

inline size_t CopiedBlock::capacity()
{
    return m_allocation.size();
}

} // namespace JSC

#endif
