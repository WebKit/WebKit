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

    template <typename T, size_t SegmentSize> class SegmentedVector {
    public:
        SegmentedVector()
            : m_size(0)
            , m_currentSegmentIndex(0)
        {
            m_segments.append(&m_inlineSegment);
        }

        ~SegmentedVector()
        {
            for (size_t i = 1; i < m_segments.size(); i++)
                delete m_segments[i];
        }

        T& last()
        {
            ASSERT(m_size);
            return m_segments[m_currentSegmentIndex]->last();
        }

        template <typename U> void append(const U& value)
        {
            if (!(m_size % SegmentSize) && m_size) {
                if (m_currentSegmentIndex == m_segments.size() - 1)
                    m_segments.append(new Segment);
                m_currentSegmentIndex++;
            }
            m_segments[m_currentSegmentIndex]->uncheckedAppend(value);
            m_size++;
        }

        void removeLast()
        {
            ASSERT(m_size);
            m_size--;
            m_segments[m_currentSegmentIndex]->removeLast();
            if (!(m_size % SegmentSize) && m_size >= SegmentSize)
                m_currentSegmentIndex--;
        }

        size_t size() const
        {
            return m_size;
        }

        T& operator[](size_t index)
        {
            ASSERT(index < m_size);
            if (index < SegmentSize)
                return m_inlineSegment[index];
            return m_segments[index / SegmentSize]->at(index % SegmentSize);
        }

        void grow(size_t newSize)
        {
            if (newSize <= m_size)
                return;

            if (newSize <= SegmentSize) {
                m_inlineSegment.resize(newSize);
                m_size = newSize;
                return;
            }

            size_t newNumSegments = newSize / SegmentSize;
            size_t extra = newSize % SegmentSize;
            if (extra)
                newNumSegments++;
            size_t oldNumSegments = m_segments.size();

            if (newNumSegments == oldNumSegments) {
                m_segments.last()->resize(extra);
                m_size = newSize;
                return;
            }

            m_segments.last()->resize(SegmentSize);

            m_segments.resize(newNumSegments);

            ASSERT(oldNumSegments < m_segments.size());
            for (size_t i = oldNumSegments; i < (newNumSegments - 1); i++) {
                Segment* segment = new Segment;
                segment->resize(SegmentSize);
                m_segments[i] = segment;
            }

            Segment* segment = new Segment;
            segment->resize(extra ? extra : SegmentSize);
            m_currentSegmentIndex = newNumSegments - 1;
            m_segments[m_currentSegmentIndex] = segment;
            m_size = newSize;
        }

        void clear()
        {
            for (size_t i = 1; i < m_segments.size(); i++)
                delete m_segments[i];
            m_segments.resize(1);
            m_inlineSegment.resize(0);
            m_currentSegmentIndex = 0;
            m_size = 0;
        }

    private:
        typedef Vector<T, SegmentSize> Segment;

        size_t m_size;
        size_t m_currentSegmentIndex;

        Segment m_inlineSegment;
        Vector<Segment*, 32> m_segments;
    };

} // namespace JSC

#endif // SegmentedVector_h
