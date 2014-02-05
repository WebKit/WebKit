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

#ifndef GCSegmentedArray_h
#define GCSegmentedArray_h

#include "HeapBlock.h"
#include <wtf/Vector.h>

namespace JSC {

class BlockAllocator;
class DeadBlock;

template <typename T>
class GCArraySegment : public HeapBlock<GCArraySegment<T>> {
public:
    GCArraySegment(Region* region)
        : HeapBlock<GCArraySegment>(region)
#if !ASSERT_DISABLED
        , m_top(0)
#endif
    {
    }

    static GCArraySegment* create(DeadBlock*);

    T* data()
    {
        return bitwise_cast<T*>(this + 1);
    }

    static const size_t blockSize = 4 * KB;

#if !ASSERT_DISABLED
    size_t m_top;
#endif
};

template <typename T>
class GCSegmentedArray {
public:
    GCSegmentedArray(BlockAllocator&);
    ~GCSegmentedArray();

    void append(T);

    bool canRemoveLast();
    const T removeLast();
    bool refill();
    
    size_t size();
    bool isEmpty();

    void fillVector(Vector<T>&);
    void clear();

protected:
    template <size_t size> struct CapacityFromSize {
        static const size_t value = (size - sizeof(GCArraySegment<T>)) / sizeof(T);
    };

    void expand();
    
    size_t postIncTop();
    size_t preDecTop();
    void setTopForFullSegment();
    void setTopForEmptySegment();
    size_t top();
    
    void validatePrevious();

    DoublyLinkedList<GCArraySegment<T>> m_segments;
    BlockAllocator& m_blockAllocator;

    JS_EXPORT_PRIVATE static const size_t s_segmentCapacity = CapacityFromSize<GCArraySegment<T>::blockSize>::value;
    size_t m_top;
    size_t m_numberOfSegments;
};

} // namespace JSC

#endif // GCSegmentedArray_h
