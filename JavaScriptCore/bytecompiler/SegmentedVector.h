/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#ifndef SegmentedVector_h
#define SegmentedVector_h

#include <wtf/Vector.h>

namespace JSC {

    // SegmentedVector is just like Vector, but it doesn't move the values
    // stored in its buffer when it grows. Therefore, it is safe to keep
    // pointers into a SegmentedVector.
    template <typename T, size_t SegmentSize> class SegmentedVector {
    public:
        SegmentedVector()
            : m_size(0)
        {
            m_segments.append(&m_inlineSegment);
        }

        ~SegmentedVector()
        {
            deleteAllSegments();
        }

        size_t size() const { return m_size; }

        T& at(size_t index)
        {
            if (index < SegmentSize)
                return m_inlineSegment[index];
            return segmentFor(index)->at(subscriptFor(index));
        }

        T& operator[](size_t index)
        {
            return at(index);
        }

        T& last()
        {
            return at(size() - 1);
        }

        template <typename U> void append(const U& value)
        {
            ++m_size;

            if (m_size <= SegmentSize) {
                m_inlineSegment.uncheckedAppend(value);
                return;
            }

            if (!segmentExistsFor(m_size - 1))
                m_segments.append(new Segment);
            segmentFor(m_size - 1)->uncheckedAppend(value);
        }

        void removeLast()
        {
            if (m_size <= SegmentSize)
                m_inlineSegment.removeLast();
            else
                segmentFor(m_size - 1)->removeLast();
            --m_size;
        }

        void grow(size_t size)
        {
            ASSERT(size > m_size);
            ensureSegmentsFor(size);
            m_size = size;
        }

        void clear()
        {
            deleteAllSegments();
            m_segments.resize(1);
            m_inlineSegment.clear();
            m_size = 0;
        }

    private:
        typedef Vector<T, SegmentSize> Segment;
        
        void deleteAllSegments()
        {
            // Skip the first segment, because it's our inline segment, which was
            // not created by new.
            for (size_t i = 1; i < m_segments.size(); i++)
                delete m_segments[i];
        }
        
        bool segmentExistsFor(size_t index)
        {
            return index / SegmentSize < m_segments.size();
        }
        
        Segment* segmentFor(size_t index)
        {
            return m_segments[index / SegmentSize];
        }
        
        size_t subscriptFor(size_t index)
        {
            return index % SegmentSize;
        }
        
        void ensureSegmentsFor(size_t size)
        {
            size_t segmentCount = m_size / SegmentSize;
            if (m_size % SegmentSize)
                ++segmentCount;
            segmentCount = std::max<size_t>(segmentCount, 1); // We always have at least our inline segment.

            size_t neededSegmentCount = size / SegmentSize;
            if (size % SegmentSize)
                ++neededSegmentCount;

            // Fill up to N - 1 segments.
            size_t end = neededSegmentCount - 1;
            for (size_t i = segmentCount - 1; i < end; ++i)
                ensureSegment(i, SegmentSize);
            
            // Grow segment N to accomodate the remainder.
            ensureSegment(end, subscriptFor(size - 1) + 1);
        }

        void ensureSegment(size_t segmentIndex, size_t size)
        {
            ASSERT(segmentIndex <= m_segments.size());
            if (segmentIndex == m_segments.size())
                m_segments.append(new Segment);
            m_segments[segmentIndex]->grow(size);
        }

        size_t m_size;
        Segment m_inlineSegment;
        Vector<Segment*, 32> m_segments;
    };

} // namespace JSC

#endif // SegmentedVector_h
