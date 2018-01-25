/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "AllocationFailureMode.h"
#include <climits>

namespace JSC {

class GCDeferralContext;
class Heap;
class VM;

class Allocator {
public:
    Allocator() { }
    
    explicit Allocator(unsigned offset)
        : m_offset(offset)
    {
    }
    
    void* allocate(VM&, GCDeferralContext*, AllocationFailureMode) const;
    
    // This version calls FailureFunc if we have a null allocator or if the TLC hasn't been resized
    // to include this allocator.
    template<typename FailureFunc>
    void* tryAllocate(VM&, GCDeferralContext*, AllocationFailureMode, const FailureFunc&) const;
    
    unsigned cellSize(Heap&) const;
    
    unsigned offset() const { return m_offset; }
    
    bool operator==(const Allocator& other) const { return m_offset == other.offset(); }
    bool operator!=(const Allocator& other) const { return !(*this == other); }
    explicit operator bool() const { return *this != Allocator(); }
    
private:
    unsigned m_offset { UINT_MAX };
};

} // namespace JSC

