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

#ifndef Region_h
#define Region_h

#include "IntRect.h"
#include <wtf/PointerComparison.h>
#include <wtf/Vector.h>

namespace WebCore {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(Region);
class Region {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Region);
public:
    WEBCORE_EXPORT Region();
    WEBCORE_EXPORT Region(const IntRect&);

    WEBCORE_EXPORT Region(const Region&);
    WEBCORE_EXPORT Region(Region&&);

    WEBCORE_EXPORT ~Region();

    WEBCORE_EXPORT Region& operator=(const Region&);
    WEBCORE_EXPORT Region& operator=(Region&&);

    IntRect bounds() const { return m_bounds; }
    bool isEmpty() const { return m_bounds.isEmpty(); }
    bool isRect() const { return !m_shape; }

    WEBCORE_EXPORT Vector<IntRect, 1> rects() const;

    WEBCORE_EXPORT void unite(const Region&);
    WEBCORE_EXPORT void intersect(const Region&);
    WEBCORE_EXPORT void subtract(const Region&);

    WEBCORE_EXPORT void translate(const IntSize&);

    // Returns true if the query region is a subset of this region.
    WEBCORE_EXPORT bool contains(const Region&) const;

    WEBCORE_EXPORT bool contains(const IntPoint&) const;

    // Returns true if the query region intersects any part of this region.
    WEBCORE_EXPORT bool intersects(const Region&) const;

    WEBCORE_EXPORT uint64_t totalArea() const;

    unsigned gridSize() const { return m_shape ? m_shape->gridSize() : 0; }

#ifndef NDEBUG
    void dump() const;
#endif

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<Region> decode(Decoder&);
    // FIXME: Remove legacy decode.
    template<class Decoder> static WARN_UNUSED_RETURN bool decode(Decoder&, Region&);

private:
    struct Span {
        int y { 0 };
        size_t segmentIndex { 0 };

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static std::optional<Span> decode(Decoder&);
    };

    class Shape {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        Shape() = default;
        Shape(const IntRect&);

        IntRect bounds() const;
        bool isEmpty() const { return m_spans.isEmpty(); }
        bool isRect() const { return m_spans.size() <= 2 && m_segments.size() <= 2; }
        unsigned gridSize() const { return m_spans.size() * m_segments.size(); }

        typedef const Span* SpanIterator;
        SpanIterator spans_begin() const;
        SpanIterator spans_end() const;
        
        typedef const int* SegmentIterator;
        SegmentIterator segments_begin(SpanIterator) const;
        SegmentIterator segments_end(SpanIterator) const;

        static Shape unionShapes(const Shape& shape1, const Shape& shape2);
        static Shape intersectShapes(const Shape& shape1, const Shape& shape2);
        static Shape subtractShapes(const Shape& shape1, const Shape& shape2);

        WEBCORE_EXPORT void translate(const IntSize&);

        struct CompareContainsOperation;
        struct CompareIntersectsOperation;

        template<typename CompareOperation>
        static bool compareShapes(const Shape& shape1, const Shape& shape2);

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static std::optional<Shape> decode(Decoder&);

#ifndef NDEBUG
        void dump() const;
#endif

    private:
        struct UnionOperation;
        struct IntersectOperation;
        struct SubtractOperation;
        
        template<typename Operation>
        static Shape shapeOperation(const Shape& shape1, const Shape& shape2);

        void appendSegment(int x);
        void appendSpan(int y);
        void appendSpan(int y, SegmentIterator begin, SegmentIterator end);
        void appendSpans(const Shape&, SpanIterator begin, SpanIterator end);

        bool canCoalesce(SegmentIterator begin, SegmentIterator end);

        Vector<int, 32> m_segments;
        Vector<Span, 16> m_spans;

        friend bool operator==(const Shape&, const Shape&);
    };

    std::unique_ptr<Shape> copyShape() const { return m_shape ? makeUnique<Shape>(*m_shape) : nullptr; }
    void setShape(Shape&&);

    IntRect m_bounds;
    std::unique_ptr<Shape> m_shape;

    friend bool operator==(const Region&, const Region&);
    friend bool operator==(const Shape&, const Shape&);
    friend bool operator==(const Span&, const Span&);
    friend bool operator!=(const Span&, const Span&);
};

static inline Region intersect(const Region& a, const Region& b)
{
    Region result(a);
    result.intersect(b);

    return result;
}

static inline Region subtract(const Region& a, const Region& b)
{
    Region result(a);
    result.subtract(b);

    return result;
}

static inline Region translate(const Region& region, const IntSize& offset)
{
    Region result(region);
    result.translate(offset);

    return result;
}

inline bool operator==(const Region& a, const Region& b)
{
    return a.m_bounds == b.m_bounds && arePointingToEqualData(a.m_shape, b.m_shape);
}
inline bool operator!=(const Region& a, const Region& b)
{
    return !(a == b);
}

inline bool operator==(const Region::Shape& a, const Region::Shape& b)
{
    return a.m_spans == b.m_spans && a.m_segments == b.m_segments;
}

inline bool operator==(const Region::Span& a, const Region::Span& b)
{
    return a.y == b.y && a.segmentIndex == b.segmentIndex;
}

inline bool operator!=(const Region::Span& a, const Region::Span& b)
{
    return !(a == b);
}

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const Region&);

template<class Encoder>
void Region::Span::encode(Encoder& encoder) const
{
    encoder << y << static_cast<uint64_t>(segmentIndex);
}

template<class Decoder>
std::optional<Region::Span> Region::Span::decode(Decoder& decoder)
{
    std::optional<int> y;
    decoder >> y;
    if (!y)
        return { };

    std::optional<uint64_t> segmentIndex;
    decoder >> segmentIndex;
    if (!segmentIndex)
        return { };

    return { { *y, static_cast<size_t>(*segmentIndex) } };
}

template<class Encoder>
void Region::Shape::encode(Encoder& encoder) const
{
    encoder << m_segments;
    encoder << m_spans;
}

template<class Decoder>
std::optional<Region::Shape> Region::Shape::decode(Decoder& decoder)
{
    std::optional<Vector<int>> segments;
    decoder >> segments;
    if (!segments)
        return std::nullopt;

    std::optional<Vector<Region::Span>> spans;
    decoder >> spans;
    if (!spans)
        return std::nullopt;

    Shape shape;
    shape.m_segments = WTFMove(*segments);
    shape.m_spans = WTFMove(*spans);

    return { shape };
}

template<class Encoder>
void Region::encode(Encoder& encoder) const
{
    encoder << m_bounds;
    bool hasShape = !!m_shape;
    encoder << hasShape;
    if (hasShape)
        encoder << *m_shape;
}

template<class Decoder>
std::optional<Region> Region::decode(Decoder& decoder)
{
    std::optional<IntRect> bounds;
    decoder >> bounds;
    if (!bounds)
        return std::nullopt;

    std::optional<bool> hasShape;
    decoder >> hasShape;
    if (!hasShape)
        return std::nullopt;

    Region region = { *bounds };

    if (*hasShape) {
        std::optional<Shape> shape;
        decoder >> shape;
        if (!shape)
            return std::nullopt;
        region.m_shape = makeUnique<Shape>(WTFMove(*shape));
    }

    return { region };
}

template<class Decoder>
bool Region::decode(Decoder& decoder, Region& region)
{
    std::optional<Region> decodedRegion;
    decoder >> decodedRegion;
    if (!decodedRegion)
        return false;
    region = WTFMove(*decodedRegion);
    return true;
}

} // namespace WebCore

#endif // Region_h
