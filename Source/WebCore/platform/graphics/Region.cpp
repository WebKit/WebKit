/*
 * Copyright (C) 2010-2024 Apple Inc. All rights reserved.
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
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/TextStream.h>

// A region class based on the paper "Scanline Coherent Shape Algebra"
// by Jonathan E. Steinhart from the book "Graphics Gems II".
//
// This implementation uses two vectors instead of linked list, and
// also compresses regions when possible.

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(Region);

WTF_MAKE_TZONE_ALLOCATED_IMPL(Region::Shape);

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
            rects.append(m_bounds);
        return rects;
    }

    for (auto spans = m_shape->spans(); spans.size() > 1; skip(spans, 1)) {
        int y = spans[0].y;
        int height = spans[1].y - y; // Ok since isValidShape ensures increasing Span::y.

        for (auto segments = m_shape->segments(spans); segments.size() > 1; skip(segments, 2)) {
            int x = segments[0];
            int width = segments[1] - x; // Ok since isValidShape ensures increasing segments.

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

    for (auto spans = m_shape->spans(); spans.size() > 1; skip(spans, 1)) {
        int y = spans[0].y;
        int maxY = spans[1].y;

        if (y > point.y())
            break;
        if (maxY <= point.y())
            continue;

        for (auto segments = m_shape->segments(spans); segments.size() > 1; skip(segments, 2)) {
            int x = segments[0];
            int maxX = segments[1];

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

    auto aSpans = aShape.spans();
    auto bSpans = bShape.spans();

    bool aHadSegmentInPreviousSpan = false;
    bool bHadSegmentInPreviousSpan = false;
    while (aSpans.size() > 1 && bSpans.size() > 1) {
        int aY = aSpans[0].y;
        int aMaxY = aSpans[1].y;
        int bY = bSpans[0].y;
        int bMaxY = bSpans[1].y;

        auto aSegments = aShape.segments(aSpans);
        auto bSegments = bShape.segments(bSpans);

        // Look for a non-overlapping part of the spans. If B had a segment in its previous span, then we already tested A against B within that span.
        bool aHasSegmentInSpan = !aSegments.empty();
        bool bHasSegmentInSpan = !bSegments.empty();
        if (aY < bY && !bHadSegmentInPreviousSpan && aHasSegmentInSpan && CompareOperation::aOutsideB(result))
            return result;
        if (bY < aY && !aHadSegmentInPreviousSpan && bHasSegmentInSpan && CompareOperation::bOutsideA(result))
            return result;

        aHadSegmentInPreviousSpan = aHasSegmentInSpan;
        bHadSegmentInPreviousSpan = bHasSegmentInSpan;

        bool spansOverlap = bMaxY > aY && bY < aMaxY;
        if (spansOverlap) {
            while (!aSegments.empty() && !bSegments.empty()) {
                int aX = aSegments[0];
                int aMaxX = aSegments[1];
                int bX = bSegments[0];
                int bMaxX = bSegments[1];

                bool segmentsOverlap = bMaxX > aX && bX < aMaxX;
                if (segmentsOverlap && CompareOperation::aOverlapsB(result))
                    return result;
                if (aX < bX && CompareOperation::aOutsideB(result))
                    return result;
                if (bX < aX && CompareOperation::bOutsideA(result))
                    return result;

                if (aMaxX < bMaxX)
                    skip(aSegments, 2);
                else if (bMaxX < aMaxX)
                    skip(bSegments, 2);
                else {
                    skip(aSegments, 2);
                    skip(bSegments, 2);
                }
            }

            if (!aSegments.empty() && CompareOperation::aOutsideB(result))
                return result;
            if (!bSegments.empty() && CompareOperation::bOutsideA(result))
                return result;
        }

        if (aMaxY < bMaxY)
            skip(aSpans, 1);
        else if (bMaxY < aMaxY)
            skip(bSpans, 1);
        else {
            skip(aSpans, 1);
            skip(bSpans, 1);
        }
    }

    if (aSpans.size() > 1 && CompareOperation::aOutsideB(result))
        return result;
    if (bSpans.size() > 1 && CompareOperation::bOutsideA(result))
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
{
    if (rect.isEmpty())
        return;
    m_segments.append(rect.x());
    m_segments.append(rect.maxX());
    m_spans.append({ rect.y(), 0 });
    m_spans.append({ rect.maxY(), 2 });
}

Region::Shape::Shape(Vector<int, 32>&& segments, Vector<Span, 16>&& spans)
    : m_segments(WTFMove(segments))
    , m_spans(WTFMove(spans))
{
    ASSERT(isValidShape(m_segments.span(), m_spans.span()));
}

void Region::Shape::appendSpan(int y)
{
    m_spans.append({ y, m_segments.size() });
}

bool Region::Shape::canCoalesce(std::span<const int> segments)
{
    if (m_spans.isEmpty())
        return false;

    return equalSpans(segments, m_segments.subspan(m_spans.last().segmentIndex));
}

void Region::Shape::appendSpan(int y, std::span<const int> segments)
{
    if (canCoalesce(segments))
        return;
  
    appendSpan(y);
    m_segments.append(segments);
}

void Region::Shape::appendSpans(const Shape& shape, std::span<const Span> spans)
{
    for (; !spans.empty(); skip(spans, 1))
        appendSpan(spans[0].y, shape.segments(spans));
}

std::span<const int> Region::Shape::segments(std::span<const Span> span) const
{
    ASSERT(span.data() >= std::to_address(m_spans.begin()));
    ASSERT(span.data() < std::to_address(m_spans.end()));
    ASSERT(std::to_address(span.end()) <= std::to_address(m_spans.end()));

    // Check if this span has any segments.
    if (span[0].segmentIndex == m_segments.size())
        return { };

    return m_segments.subspan(span[0].segmentIndex, span[1].segmentIndex - span[0].segmentIndex);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const Region::Shape& value)
{
    ts << '\n';
    TextStream::IndentScope indentScope(ts);
    ts << indent;
    for (auto spans = value.spans(); !spans.empty(); skip(spans, 1)) {
        ts << "y: "_s << spans[0].y << " spans: ("_s;
        int comma = 0;
        for (auto& segment : value.segments(spans))
            ts << (comma++ > 0 ? ", "_s : ""_s) << segment;
        ts << ")\n"_s;
    }
    ts << "spans: ("_s;
    for (size_t i = 0; i < value.m_spans.size(); ++i)
        ts << (i > 0 ? ", "_s : ""_s) << "y: "_s << value.m_spans[i].y << " si: "_s << value.m_spans[i].segmentIndex;
    ts << ")\n"_s << "segments: ("_s;
    for (size_t i = 0; i < value.m_segments.size(); ++i)
        ts << (i > 0 ? ", "_s : ""_s) << value.m_segments[i];
    ts << ")\n"_s;
    return ts;
}

IntRect Region::Shape::bounds() const
{
    if (isEmpty())
        return IntRect();

    auto spans = this->spans();
    int minY = spans.front().y;
    int maxY = spans.back().y;

    int minX = std::numeric_limits<int>::max();
    int maxX = std::numeric_limits<int>::min();

    while (spans.size() > 1) {
        auto segments = this->segments(spans);
        if (!segments.empty()) {
            auto& firstSegment = segments.front();
            auto& lastSegment = segments.back();
            ASSERT(&firstSegment != &lastSegment);

            if (firstSegment < minX)
                minX = firstSegment;

            if (lastSegment > maxX)
                maxX = lastSegment;
        }

        skip(spans, 1);
    }

    ASSERT(minX <= maxX);
    ASSERT(minY <= maxY);

    CheckedInt32 width = checkedDifference<int32_t>(maxX, minX);
    CheckedInt32 height = checkedDifference<int32_t>(maxY, minY);
    return IntRect(minX, minY, width.hasOverflowed() ? std::numeric_limits<int32_t>::max() : width.value(), height.hasOverflowed() ? std::numeric_limits<int32_t>::max() : height.value());
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
    static_assert(!(!Operation::shouldAddRemainingSegmentsFromSpan1 && Operation::shouldAddRemainingSegmentsFromSpan2), "invalid segment combination");
    static_assert(!(!Operation::shouldAddRemainingSpansFromShape1 && Operation::shouldAddRemainingSpansFromShape2), "invalid span combination");

    Shape result;
    if (Operation::trySimpleOperation(shape1, shape2, result))
        return result;

    auto spans1 = shape1.spans();
    auto spans2 = shape2.spans();

    std::span<const int> segments1;
    std::span<const int> segments2;

    // Iterate over all spans.
    while (!spans1.empty() && !spans2.empty()) {
        int y = 0;
        auto test = spans1[0].y <=> spans2[0].y;

        if (test <= 0) {
            y = spans1[0].y;

            segments1 = shape1.segments(spans1);
            skip(spans1, 1);
        }
        if (test >= 0) {
            y = spans2[0].y;

            segments2 = shape2.segments(spans2);
            skip(spans2, 1);
        }

        int flag = 0;
        int oldFlag = 0;

        auto s1 = segments1;
        auto s2 = segments2;

        Vector<int, 32> segments;

        // Now iterate over the segments in each span and construct a new vector of segments.
        while (!s1.empty() && !s2.empty()) {
            auto test = s1[0] <=> s2[0];
            int x;

            if (test <= 0) {
                x = s1[0];
                flag = flag ^ 1;
                skip(s1, 1);
            }
            if (test >= 0) {
                x = s2[0];
                flag = flag ^ 2;
                skip(s2, 1);
            }

            if (flag == Operation::opCode || oldFlag == Operation::opCode)
                segments.append(x);
            
            oldFlag = flag;
        }

        // Add any remaining segments.
        if (Operation::shouldAddRemainingSegmentsFromSpan1 && !s1.empty())
            segments.append(s1);
        else if (Operation::shouldAddRemainingSegmentsFromSpan2 && !s2.empty())
            segments.append(s2);

        // Add the span.
        if (!segments.isEmpty() || !result.isEmpty())
            result.appendSpan(y, segments.span());
    }

    // Add any remaining spans.
    if (Operation::shouldAddRemainingSpansFromShape1 && !spans1.empty())
        result.appendSpans(shape1, spans1);
    else if (Operation::shouldAddRemainingSpansFromShape2 && !spans2.empty())
        result.appendSpans(shape2, spans2);

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

static std::span<const int> segmentsForSpanSegmentIndices(std::span<const int> segments, size_t start, size_t end)
{
    if (segments.size() <= end)
        return { };
    return segments.subspan(start, end - start);
}

bool Region::Shape::isValidShape(std::span<const int> segments, std::span<const Span> spans)
{
    const size_t spansSize = spans.size();
    const size_t segmentsSize = segments.size();
    if (!spansSize)
        return !segmentsSize;
    if (!segmentsSize)
        return !spansSize;
    if (spansSize == 1) [[unlikely]]
        return false;
    if (segmentsSize % 2) [[unlikely]]
        return false;
    for (size_t i = 0; i < spansSize; ++i) {
        auto& span = spans[i];
        if (span.segmentIndex > segmentsSize) [[unlikely]]
            return false;
        if (span.segmentIndex % 2) [[unlikely]]
            return false;

        if (i < spansSize - 1) {
            auto& nextSpan = spans[i + 1];

            if (span.y >= nextSpan.y) [[unlikely]]
                return false;
            if (span.segmentIndex > nextSpan.segmentIndex) [[unlikely]]
                return false;

            std::span spanSegments = segmentsForSpanSegmentIndices(segments, span.segmentIndex, nextSpan.segmentIndex);
            int lastX = std::numeric_limits<int>::min();
            for (int segment : spanSegments) {
                if (lastX > segment) [[unlikely]]
                    return false;
                lastX = segment;
            }
        } else if (span.segmentIndex != segments.size()) [[unlikely]]
            return false;
    }
    return true;
}

TextStream& operator<<(TextStream& ts, const Region& region)
{
    ts << '\n';
    {
        TextStream::IndentScope indentScope(ts);
        for (auto& rect : region.rects())
            ts << indent << "(rect "_s << rect << ")\n"_s;
    }

    return ts;
}

} // namespace WebCore
