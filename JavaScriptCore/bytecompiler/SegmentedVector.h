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
            return m_segments.last()->last();
        }

        template <typename U> void append(const U& value)
        {
            if (!(m_size % SegmentSize) && m_size)
                m_segments.append(new Segment);
            m_segments.last()->uncheckedAppend(value);
            m_size++;
        }

        void removeLast()
        {
            ASSERT(m_size);
            m_size--;
            m_segments.last()->removeLast();
            if (!(m_size % SegmentSize) && m_size >= SegmentSize) {
                delete m_segments.last();
                m_segments.removeLast();
            }
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

        void resize(size_t size)
        {
            if (size < m_size)
                shrink(size);
            else if (size > m_size)
                grow(size);
            ASSERT(size == m_size);
        }

    private:
        void shrink(size_t size)
        {
            ASSERT(size < m_size);
            size_t numSegments = size / SegmentSize;
            size_t extra = size % SegmentSize;
            if (extra)
                numSegments++;
            if (!numSegments) {
                for (size_t i = 1; i < m_segments.size(); i++)
                    delete m_segments[i];
                m_segments.resize(1);
                m_inlineSegment.resize(0);
                return;
            }

            for (size_t i = numSegments; i < m_segments.size(); i++)
                delete m_segments[i];

            m_segments.resize(numSegments);
            if (extra)
                m_segments.last()->resize(extra);
            m_size = size;
        }

        void grow(size_t size)
        {
            ASSERT(size > m_size);
            if (size <= SegmentSize) {
                m_inlineSegment.resize(size);
                m_size = size;
                return;
            }

            size_t numSegments = size / SegmentSize;
            size_t extra = size % SegmentSize;
            if (extra)
                numSegments++;
            size_t oldSize = m_segments.size();

            if (numSegments == oldSize) {
                m_segments.last()->resize(extra);
                m_size = size;
                return;
            }

            m_segments.last()->resize(SegmentSize);

            m_segments.resize(numSegments);

            ASSERT(oldSize < m_segments.size());
            for (size_t i = oldSize; i < (numSegments - 1); i++) {
                Segment* segment = new Segment;
                segment->resize(SegmentSize);
                m_segments[i] = segment;
            }

            Segment* segment = new Segment;
            segment->resize(extra ? extra : SegmentSize);
            m_segments[numSegments - 1] = segment;
            m_size = size;
        }

        typedef Vector<T, SegmentSize> Segment;
        size_t m_size;
        Segment m_inlineSegment;
        Vector<Segment*, 32> m_segments;
    };

} // namespace JSC

#endif // SegmentedVector_h
