/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "FTLDataSection.h"

#if ENABLE(FTL_JIT)

#include <wtf/Assertions.h>
#include <wtf/DataLog.h>
#include <wtf/FastMalloc.h>

namespace JSC { namespace FTL {

DataSection::DataSection(size_t size, unsigned alignment)
    : m_size(size)
{
    RELEASE_ASSERT(WTF::bitCount(alignment) == 1);
    
    const unsigned nativeAlignment = 8;
    
    alignment = std::max(nativeAlignment, alignment);
    
    size_t allocatedSize = size + alignment - nativeAlignment;
    m_allocationBase = fastMalloc(allocatedSize);
    
    m_base = bitwise_cast<void*>(
        (bitwise_cast<uintptr_t>(m_allocationBase) + alignment - 1) & ~static_cast<uintptr_t>(alignment - 1));
    
    RELEASE_ASSERT(!(bitwise_cast<uintptr_t>(m_base) & (alignment - 1)));
    RELEASE_ASSERT(bitwise_cast<uintptr_t>(m_base) + size <= bitwise_cast<uintptr_t>(m_allocationBase) + allocatedSize);
}

DataSection::~DataSection()
{
    fastFree(m_allocationBase);
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

