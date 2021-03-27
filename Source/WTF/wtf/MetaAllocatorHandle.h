/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Assertions.h>
#include <wtf/MetaAllocatorPtr.h>
#include <wtf/RedBlackTree.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WTF {

class MetaAllocator;
class PrintStream;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(MetaAllocatorHandle);
class MetaAllocatorHandle : public ThreadSafeRefCounted<MetaAllocatorHandle>, public RedBlackTree<MetaAllocatorHandle, void*>::Node {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(MetaAllocatorHandle);

public:
    using MemoryPtr = MetaAllocatorPtr<HandleMemoryPtrTag>;

    WTF_EXPORT_PRIVATE ~MetaAllocatorHandle();

    MemoryPtr start() const
    {
        return m_start;
    }

    MemoryPtr end() const
    {
        return m_end;
    }

    uintptr_t startAsInteger() const
    {
        return m_start.untaggedPtr<uintptr_t>();
    }

    uintptr_t endAsInteger() const
    {
        return m_end.untaggedPtr<uintptr_t>();
    }

    size_t sizeInBytes() const
    {
        return m_end.untaggedPtr<size_t>() - m_start.untaggedPtr<size_t>();
    }
    
    bool containsIntegerAddress(uintptr_t address) const
    {
        return address >= startAsInteger() && address < endAsInteger();
    }
    
    bool contains(void* address) const
    {
        return containsIntegerAddress(reinterpret_cast<uintptr_t>(address));
    }
        
    WTF_EXPORT_PRIVATE void shrink(size_t newSizeInBytes);
    
    MetaAllocator& allocator()
    {
        return m_allocator;
    }

    void* key()
    {
        return m_start.untaggedPtr();
    }

    WTF_EXPORT_PRIVATE void dump(PrintStream& out) const;
    
private:
    MetaAllocatorHandle(MetaAllocator&, MemoryPtr start, size_t sizeInBytes);

    MetaAllocator& m_allocator;
    MemoryPtr m_start;
    MemoryPtr m_end;

    friend class MetaAllocator;
};

}
