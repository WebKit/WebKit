/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
#include "Region.h"

#include <stdio.h>
#include <wtf/text/TextStream.h>

// A region class based on the paper "Scanline Coherent Shape Algebra"
// by Jonathan E. Steinhart from the book "Graphics Gems II".
//
// This implementation uses two vectors instead of linked list, and
// also compresses regions when possible.

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(Region);

Region::Region()
{
}

Region::Region(const IntRect& rect)
    : m_bounds(rect)
{
}

Region::Region(const Region& other)
    : m_bounds(other.m_bounds)
    , m_shape(other.copyShape())
{
}

Region::Region(Region&& other)
    : m_bounds(WTFMove(other.m_bounds))
    , m_shape(WTFMove(other.m_shape))
{
}

Region::~Region()
{
}

Region& Region::operator=(const Region& other)
{
    m_bounds = other.m_bounds;
    m_shape = other.copyShape();
    return *this;
}

Region& Region::operator=(Region&& other)
{
    m_bounds = WTFMove(other.m_bounds);
    m_shape = WTFMove(other.m_shape);
    return *this;
}

Vector<IntRect, 1> Region::rects() const
{
    Vector<IntRect, 1> rects;

    if (!m_shape) {
        if (!m_bounds.isEmpty())
            rects.uncheckedAppend(m_bounds);
        return rects;
    }

    for (Shape::SpanIterator span = m_shape->spans_begin(), end = m_shape->spans_end(); span != end && span + 1 != end; ++span) {
        int y = span->y;
        int height = (span + 1)->y - y;

        for (Shape::SegmentIterator segment = m_shape->segments_begin(span), end = m_shape->segments_end(span); segment != end && segment + 1 != end; segment += 2) {
            int x = *segment;
            int width = *(segment + 1) - x;

            rects.append(IntRect(x, y, width, height));
        }
    }

    return rects;
}

bool Region::contains(const Region& region) const
{
    if (!m_bounds.contains(region.m_bounds))
        return false;

    if (!m_shape)
        return true;

    return Shape::compareShapes<Shape::CompareContainsOperation>(*m_shape, region.m_shape ? *region.m_shape : Shape(region.m_bounds));
}

bool Region::contains(const IntPoint& point) const
{
    if (!m_bounds.contains(point))
        return false;

    if (!m_shape)
        return true;

    for (Shape::SpanIterator span = m_shape->spans_begin(), end = m_shape->spans_end(); span != end && span + 1 != end; ++span) {
        int y = span->y;
        int maxY = (span + 1)->y;

        if (y > point.y())
            break;
        if (maxY <= point.y())
            continue;

        for (Shape::SegmentIterator segment = m_shape->segments_begin(span), end = m_shape->segments_end(span); segment != end && segment + 1 != end; segment += 2) {
            int x = *segment;
            int maxX = *(segment + 1);

            if (x > point.x())
                break;
            if (maxX > point.x())
                return true;
        }
    }

    return false;
}

bool Region::intersects(const Region& region) const
{
    if (!m_bounds.intersects(region.m_bounds))
        return false;

    if (!m_shape && !region.m_shape)
        return true;

    return Shape::compareShapes<Shape::CompareIntersectsOperation>(m_shape ? *m_shape : m_bounds, region.m_shape ? *region.m_shape : region.m_bounds);
}

uint64_t Region::totalArea() const
{
    uint64_t totalArea = 0;

    for (auto& rect : rects())
        totalArea += (rect.width() * rect.height());

    return totalArea;
}

template<typename CompareOperation>
bool Region::Shape::compareShapes(const Shape& aShape, const Shape& bShape)
{
    bool result = CompareOperation::defaultResult;

    Shape::SpanIterator aSpan = aShape.spans_begin();
    Shape::SpanIterator aSpanEnd = aShape.spans_end();
    Shape::SpanIterator bSpan = bShape.spans_begin();
    Shape::SpanIterator bSpanEnd = bShape.spans_end();

    bool aHadSegmentInPreviousSpan = false;
    bool bHadSegmentInPreviousSpan = false;
    while (aSpan != aSpanEnd && aSpan + 1 != aSpanEnd && bSpan != bSpanEnd && bSpan + 1 != bSpanEnd) {
        int aY = aSpan->y;
        int aMaxY = (aSpan + 1)->y;
        int bY = bSpan->y;
        int bMaxY = (bSpan + 1)->y;

        Shape::SegmentIterator aSegment = aShape.segments_begin(aSpan);
        Shape::SegmentIterator aSegmentEnd = aShape.segments_end(aSpan);
        Shape::SegmentIterator bSegment = bShape.segments_begin(bSpan);
        Shape::SegmentIterator bSegmentEnd = bShape.segments_end(bSpan);

        // Look for a non-overlapping part of the spans. If B had a segment in its previous span, then we already tested A against B within that span.
        bool aHasSegmentInSpan = aSegment != aSegmentEnd;
        bool bHasSegmentInSpan = bSegment != bSegmentEnd;
        if (aY < bY && !bHadSegmentInPreviousSpan && aHasSegmentInSpan && CompareOperation::aOutsideB(result))
            return result;
        if (bY < aY && !aHadSegmentInPreviousSpan && bHasSegmentInSpan && CompareOperation::bOutsideA(result))
            return result;

        aHadSegmentInPreviousSpan = aHasSegmentInSpan;
        bHadSegmentInPreviousSpan = bHasSegmentInSpan;

        bool spansOverlap = bMaxY > aY && bY < aMaxY;
        if (spansOverlap) {
            while (aSegment != aSegmentEnd && bSegment != bSegmentEnd) {
                int aX = *aSegment;
                int aMaxX = *(aSegment + 1);
                int bX = *bSegment;
                int bMaxX = *(bSegment + 1);

                bool segmentsOverlap = bMaxX > aX && bX < aMaxX;
                if (segmentsOverlap && CompareOperation::aOverlapsB(result))
                    return result;
                if (aX < bX && CompareOperation::aOutsideB(result))
                    return result;
                if (bX < aX && CompareOperation::bOutsideA(result))
                    return result;

                if (aMaxX < bMaxX)
                    aSegment += 2;
                else if (bMaxX < aMaxX)
                    bSegment += 2;
                else {
                    aSegment += 2;
                    bSegment += 2;
                }
            }

            if (aSegment != aSegmentEnd && CompareOperation::aOutsideB(result))
                return result;
            if (bSegment != bSegmentEnd && CompareOperation::bOutsideA(result))
                return result;
        }

        if (aMaxY < bMaxY)
            aSpan += 1;
        else if (bMaxY < aMaxY)
            bSpan += 1;
        else {
            aSpan += 1;
            bSpan += 1;
        }
    }

    if (aSpan != aSpanEnd && aSpan + 1 != aSpanEnd && CompareOperation::aOutsideB(result))
        return result;
    if (bSpan != bSpanEnd && bSpan + 1 != bSpanEnd && CompareOperation::bOutsideA(result))
        return result;

    return result;
}

struct Region::Shape::CompareContainsOperation {
    static constexpr bool defaultResult = true;
    inline static bool aOutsideB(bool& /* result */) { return false; }
    inline static bool bOutsideA(bool& result) { result = false; return true; }
    inline static bool aOverlapsB(bool& /* result */) { return false; }
};

struct Region::Shape::CompareIntersectsOperation {
    static constexpr bool defaultResult = false;
    inline static bool aOutsideB(bool& /* result */) { return false; }
    inline static bool bOutsideA(bool& /* result */) { return false; }
    inline static bool aOverlapsB(bool& result) { result = true; return true; }
};

Region::Shape::Shape(const IntRect& rect)
    : m_segments({ rect.x(), rect.maxX() })
    , m_spans({ { rect.y(), 0 }, { rect.maxY(), 2 } })
{
}

void Region::Shape::appendSpan(int y)
{
    m_spans.append({ y, m_segments.size() });
}

bool Region::Shape::canCoalesce(SegmentIterator begin, SegmentIterator end)
{
    if (m_spans.isEmpty())
        return false;

    SegmentIterator lastSpanBegin = m_segments.data() + m_spans.last().segmentIndex;
    SegmentIterator lastSpanEnd = m_segments.data() + m_segments.size();

    // Check if both spans have an equal number of segments.
    if (lastSpanEnd - lastSpanBegin != end - begin)
        return false;

    // Check if both spans are equal.
    if (!std::equal(begin, end, lastSpanBegin))
        return false;

    // Since the segments are equal the second segment can just be ignored.
    return true;
}

void Region::Shape::appendSpan(int y, SegmentIterator begin, SegmentIterator end)
{
    if (canCoalesce(begin, end))
        return;
  
    appendSpan(y);
    m_segments.appendRange(begin, end);
}

void Region::Shape::appendSpans(const Shape& shape, SpanIterator begin, SpanIterator end)
{
    for (SpanIterator it = begin; it != end; ++it)
        appendSpan(it->y, shape.segments_begin(it), shape.segments_end(it));
}

void Region::Shape::appendSegment(int x)
{
    m_segments.append(x);
}

Region::Shape::SpanIterator Region::Shape::spans_begin() const
{
    return m_spans.data();
}

Region::Shape::SpanIterator Region::Shape::spans_end() const
{
    return m_spans.data() + m_spans.size();
}

Region::Shape::SegmentIterator Region::Shape::segments_begin(SpanIterator it) const
{
    ASSERT(it >= m_spans.data());
    ASSERT(it < m_spans.data() + m_spans.size());

    // Check if this span has any segments.
    if (it->segmentIndex == m_segments.size())
        return 0;

    return &m_segments[it->segmentIndex];
}

Region::Shape::SegmentIterator Region::Shape::segments_end(SpanIterator it) const
{
    ASSERT(it >= m_spans.data());
    ASSERT(it < m_spans.data() + m_spans.size());

    // Check if this span has any segments.
    if (it->segmentIndex == m_segments.size())
        return 0;

    ASSERT(it + 1 < m_spans.data() + m_spans.size());
    size_t segmentIndex = (it + 1)->segmentIndex;

    ASSERT_WITH_SECURITY_IMPLICATION(segmentIndex <= m_segments.size());
    return m_segments.data() + segmentIndex;
}

#ifndef NDEBUG
void Region::Shape::dump() const
{
    for (auto span = spans_begin(), end = spans_end(); span != end; ++span) {
        printf("%6d: (", span->y);

        for (auto segment = segments_begin(span), end = segments_end(span); segment != end; ++segment)
            printf("%d ", *segment);
        printf(")\n");
    }

    printf("\n");
}
#endif

IntRect Region::Shape::bounds() const
{
    if (isEmpty())
        return IntRect();

    SpanIterator span = spans_begin();
    int minY = span->y;

    SpanIterator lastSpan = spans_end() - 1;
    int maxY = lastSpan->y;

    int minX = std::numeric_limits<int>::max();
    int maxX = std::numeric_limits<int>::min();

    while (span != lastSpan) {
        SegmentIterator firstSegment = segments_begin(span);
        SegmentIterator lastSegment = segments_end(span) - 1;

        if (firstSegment && lastSegment) {
            ASSERT(firstSegment != lastSegment);

            if (*firstSegment < minX)
                minX = *firstSegment;

            if (*lastSegment > maxX)
                maxX = *lastSegment;
        }

        ++span;
    }

    ASSERT(minX <= maxX);
    ASSERT(minY <= maxY);

    return IntRect(minX, minY, maxX - minX, maxY - minY);
}

void Region::Shape::translate(const IntSize& offset)
{
    for (size_t i = 0; i < m_segments.size(); ++i)
        m_segments[i] += offset.width();
    for (size_t i = 0; i < m_spans.size(); ++i)
        m_spans[i].y += offset.height();
}

enum {
    Shape1,
    Shape2,
};

template<typename Operation>
Region::Shape Region::Shape::shapeOperation(const Shape& shape1, const Shape& shape2)
{
    COMPILE_ASSERT(!(!Operation::shouldAddRemainingSegmentsFromSpan1 && Operation::shouldAddRemainingSegmentsFromSpan2), invalid_segment_combination);
    COMPILE_ASSERT(!(!Operation::shouldAddRemainingSpansFromShape1 && Operation::shouldAddRemainingSpansFromShape2), invalid_span_combination);

    Shape result;
    if (Operation::trySimpleOperation(shape1, shape2, result))
        return result;

    SpanIterator spans1 = shape1.spans_begin();
    SpanIterator spans1End = shape1.spans_end();

    SpanIterator spans2 = shape2.spans_begin();
    SpanIterator spans2End = shape2.spans_end();

    SegmentIterator segments1 = 0;
    SegmentIterator segments1End = 0;

    SegmentIterator segments2 = 0;
    SegmentIterator segments2End = 0;

    // Iterate over all spans.
    while (spans1 != spans1End && spans2 != spans2End) {
        int y = 0;
        int test = spans1->y - spans2->y;

        if (test <= 0) {
            y = spans1->y;

            segments1 = shape1.segments_begin(spans1);
            segments1End = shape1.segments_end(spans1);
            ++spans1;
        }
        if (test >= 0) {
            y = spans2->y;

            segments2 = shape2.segments_begin(spans2);
            segments2End = shape2.segments_end(spans2);
            ++spans2;
        }

        int flag = 0;
        int oldFlag = 0;

        SegmentIterator s1 = segments1;
        SegmentIterator s2 = segments2;

        Vector<int, 32> segments;

        // Now iterate over the segments in each span and construct a new vector of segments.
        while (s1 != segments1End && s2 != segments2End) {
            int test = *s1 - *s2;
            int x;

            if (test <= 0) {
                x = *s1;
                flag = flag ^ 1;
                ++s1;
            }
            if (test >= 0) {
                x = *s2;
                flag = flag ^ 2;
                ++s2;
            }

            if (flag == Operation::opCode || oldFlag == Operation::opCode)
                segments.append(x);
            
            oldFlag = flag;
        }

        // Add any remaining segments.
        if (Operation::shouldAddRemainingSegmentsFromSpan1 && s1 != segments1End)
            segments.appendRange(s1, segments1End);
        else if (Operation::shouldAddRemainingSegmentsFromSpan2 && s2 != segments2End)
            segments.appendRange(s2, segments2End);

        // Add the span.
        if (!segments.isEmpty() || !result.isEmpty())
            result.appendSpan(y, segments.data(), segments.data() + segments.size());
    }

    // Add any remaining spans.
    if (Operation::shouldAddRemainingSpansFromShape1 && spans1 != spans1End)
        result.appendSpans(shape1, spans1, spans1End);
    else if (Operation::shouldAddRemainingSpansFromShape2 && spans2 != spans2End)
        result.appendSpans(shape2, spans2, spans2End);

    return result;
}

struct Region::Shape::UnionOperation {
    static bool trySimpleOperation(const Shape& shape1, const Shape& shape2, Shape& result)
    {
        if (shape1.isEmpty()) {
            result = shape2;
            return true;
        }
        
        return false;
    }

    static const int opCode = 0;

    static const bool shouldAddRemainingSegmentsFromSpan1 = true;
    static const bool shouldAddRemainingSegmentsFromSpan2 = true;
    static const bool shouldAddRemainingSpansFromShape1 = true;
    static const bool shouldAddRemainingSpansFromShape2 = true;
};

Region::Shape Region::Shape::unionShapes(const Shape& shape1, const Shape& shape2)
{
    return shapeOperation<UnionOperation>(shape1, shape2);
}

struct Region::Shape::IntersectOperation {
    static bool trySimpleOperation(const Shape&, const Shape&, Shape&)
    {
        return false;
    }
    
    static const int opCode = 3;
    
    static const bool shouldAddRemainingSegmentsFromSpan1 = false;
    static const bool shouldAddRemainingSegmentsFromSpan2 = false;
    static const bool shouldAddRemainingSpansFromShape1 = false;
    static const bool shouldAddRemainingSpansFromShape2 = false;
};

Region::Shape Region::Shape::intersectShapes(const Shape& shape1, const Shape& shape2)
{
    return shapeOperation<IntersectOperation>(shape1, shape2);
}

struct Region::Shape::SubtractOperation {
    static bool trySimpleOperation(const Shape&, const Shape&, Region::Shape&)
    {
        return false;
    }
    
    static const int opCode = 1;
    
    static const bool shouldAddRemainingSegmentsFromSpan1 = true;
    static const bool shouldAddRemainingSegmentsFromSpan2 = false;
    static const bool shouldAddRemainingSpansFromShape1 = true;
    static const bool shouldAddRemainingSpansFromShape2 = false;
};

Region::Shape Region::Shape::subtractShapes(const Shape& shape1, const Shape& shape2)
{
    return shapeOperation<SubtractOperation>(shape1, shape2);
}

#ifndef NDEBUG
void Region::dump() const
{
    printf("Bounds: (%d, %d, %d, %d)\n",
           m_bounds.x(), m_bounds.y(), m_bounds.width(), m_bounds.height());
    if (m_shape)
        m_shape->dump();
}
#endif

void Region::intersect(const Region& region)
{
    if (m_bounds.isEmpty())
        return;
    if (!m_bounds.intersects(region.m_bounds)) {
        m_shape = nullptr;
        m_bounds = IntRect();
        return;
    }
    if (!m_shape && !region.m_shape) {
        m_bounds = intersection(m_bounds, region.m_bounds);
        return;
    }

    setShape(Shape::intersectShapes(m_shape ? *m_shape : m_bounds, region.m_shape ? *region.m_shape : region.m_bounds));
}

void Region::unite(const Region& region)
{
    if (region.isEmpty())
        return;
    if (isEmpty()) {
        m_bounds = region.m_bounds;
        m_shape = region.copyShape();
        return;
    }
    if (region.isRect() && region.m_bounds.contains(m_bounds)) {
        m_bounds = region.m_bounds;
        m_shape = nullptr;
        return;
    }
    if (contains(region))
        return;

    setShape(Shape::unionShapes(m_shape ? *m_shape : m_bounds, region.m_shape ? *region.m_shape : region.m_bounds));
}

void Region::subtract(const Region& region)
{
    if (isEmpty())
        return;
    if (region.isEmpty())
        return;
    if (!m_bounds.intersects(region.m_bounds))
        return;

    setShape(Shape::subtractShapes(m_shape ? *m_shape : m_bounds, region.m_shape ? *region.m_shape : region.m_bounds));
}

void Region::translate(const IntSize& offset)
{
    m_bounds.move(offset);
    if (m_shape)
        m_shape->translate(offset);
}

void Region::setShape(Shape&& shape)
{
    m_bounds = shape.bounds();

    if (shape.isRect()) {
        m_shape = nullptr;
        return;
    }

    if (!m_shape)
        m_shape = makeUnique<Shape>(WTFMove(shape));
    else
        *m_shape = WTFMove(shape);
}

TextStream& operator<<(TextStream& ts, const Region& region)
{
    ts << "\n";
    {
        TextStream::IndentScope indentScope(ts);
        for (auto& rect : region.rects())
            ts << indent << "(rect " << rect << ")\n";
    }

    return ts;
}

} // namespace WebCore
