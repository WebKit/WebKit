/*
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
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

#include "FloatRect.h"
#include "InlinePathData.h"
#include "WindRule.h"
#include <wtf/EnumTraits.h>
#include <wtf/FastMalloc.h>
#include <wtf/Function.h>
#include <wtf/Forward.h>

#if USE(CG)

#include <wtf/RetainPtr.h>
#include <CoreGraphics/CGPath.h>
typedef struct CGPath PlatformPath;

#elif USE(CAIRO)
#include "RefPtrCairo.h"

#else

typedef void PlatformPath;

#endif

#if !USE(CAIRO)
typedef PlatformPath* PlatformPathPtr;

#if USE(CG)
using PlatformPathStorageType = RetainPtr<CGMutablePathRef>;
#else
using PlatformPathStorageType = PlatformPathPtr;
#endif
#endif

namespace WTF {
class TextStream;
}

namespace WebCore {

class AffineTransform;
class FloatPoint;
class FloatRoundedRect;
class FloatSize;
class GraphicsContext;
class PathTraversalState;
class RoundedRect;

// The points in the structure are the same as those that would be used with the
// add... method. For example, a line returns the endpoint, while a cubic returns
// two tangent points and the endpoint.
struct PathElement {
    enum class Type : uint8_t {
        MoveToPoint, // The points member will contain 1 value.
        AddLineToPoint, // The points member will contain 1 value.
        AddQuadCurveToPoint, // The points member will contain 2 values.
        AddCurveToPoint, // The points member will contain 3 values.
        CloseSubpath // The points member will contain no values.
    };

    FloatPoint points[3];
    Type type;
};

using PathApplierFunction = Function<void(const PathElement&)>;

class Path {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT Path();
#if USE(CG)
    Path(RetainPtr<CGMutablePathRef>&&);
#endif
#if USE(CAIRO)
    explicit Path(RefPtr<cairo_t>&&);
#endif
    WEBCORE_EXPORT ~Path();

    WEBCORE_EXPORT Path(const Path&);
    WEBCORE_EXPORT Path(Path&&);
    WEBCORE_EXPORT Path& operator=(const Path&);
    WEBCORE_EXPORT Path& operator=(Path&&);

#if ENABLE(INLINE_PATH_DATA)
    static Path from(const InlinePathData& inlineData)
    {
        Path path;
        path.m_inlineData = inlineData;
        return path;
    }
#endif

    static Path polygonPathFromPoints(const Vector<FloatPoint>&);

    bool contains(const FloatPoint&, WindRule = WindRule::NonZero) const;
    bool strokeContains(const FloatPoint&, const Function<void(GraphicsContext&)>& strokeStyleApplier) const;

    // fastBoundingRect() should equal or contain boundingRect(); boundingRect()
    // should perfectly bound the points within the path.
    FloatRect boundingRect() const;
    WEBCORE_EXPORT FloatRect fastBoundingRect() const;
    FloatRect strokeBoundingRect(const Function<void(GraphicsContext&)>& strokeStyleApplier = { }) const;

    WEBCORE_EXPORT size_t elementCount() const;
    float length() const;
    PathTraversalState traversalStateAtLength(float length) const;
    FloatPoint pointAtLength(float length) const;

    WEBCORE_EXPORT void clear();
    WEBCORE_EXPORT bool isNull() const;
    bool isEmpty() const;
    // Gets the current point of the current path, which is conceptually the final point reached by the path so far.
    // Note the Path can be empty (isEmpty() == true) and still have a current point.
    // FIXME: The above comment might need to be updated; on all supported platforms, the result of hasCurrentPoint() is identical
    // to !isEmpty().
    bool hasCurrentPoint() const;
    FloatPoint currentPoint() const;

    bool isClosed() const;

    WEBCORE_EXPORT void moveTo(const FloatPoint&);
    WEBCORE_EXPORT void addLineTo(const FloatPoint&);
    WEBCORE_EXPORT void addQuadCurveTo(const FloatPoint& controlPoint, const FloatPoint& endPoint);
    WEBCORE_EXPORT void addBezierCurveTo(const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, const FloatPoint& endPoint);
    void addArcTo(const FloatPoint&, const FloatPoint&, float radius);
    WEBCORE_EXPORT void closeSubpath();

    void addArc(const FloatPoint&, float radius, float startAngle, float endAngle, bool anticlockwise);
    WEBCORE_EXPORT void addRect(const FloatRect&);
    void addEllipse(FloatPoint, float radiusX, float radiusY, float rotation, float startAngle, float endAngle, bool anticlockwise);
    void addEllipse(const FloatRect&);

    enum class RoundedRectStrategy : uint8_t {
        PreferNative,
        PreferBezier
    };

    WEBCORE_EXPORT void addRoundedRect(const FloatRect&, const FloatSize& roundingRadii, RoundedRectStrategy = RoundedRectStrategy::PreferNative);
    WEBCORE_EXPORT void addRoundedRect(const FloatRoundedRect&, RoundedRectStrategy = RoundedRectStrategy::PreferNative);
    void addRoundedRect(const RoundedRect&);

    void addPath(const Path&, const AffineTransform&);

    void translate(const FloatSize&);

    // To keep Path() cheap, it does not allocate a PlatformPath immediately
    // meaning Path::platformPath() can return null.
#if USE(CG)
    WEBCORE_EXPORT PlatformPathPtr platformPath() const;
#elif USE(CAIRO)
    cairo_t* cairoPath() const { return m_path.get(); }
#else
    PlatformPathPtr platformPath() const { return m_path; }
#endif

#if !USE(CAIRO)
    // ensurePlatformPath() will allocate a PlatformPath if it has not yet been and will never return null.
    WEBCORE_EXPORT PlatformPathPtr ensurePlatformPath();
#endif

    WEBCORE_EXPORT void apply(const PathApplierFunction&) const;
    void transform(const AffineTransform&);

    static float circleControlPoint()
    {
        // Approximation of control point positions on a bezier to simulate a quarter of a circle.
        // This is 1-kappa, where kappa = 4 * (sqrt(2) - 1) / 3
        return 0.447715;
    }

    void addBeziersForRoundedRect(const FloatRect&, const FloatSize& topLeftRadius, const FloatSize& topRightRadius, const FloatSize& bottomLeftRadius, const FloatSize& bottomRightRadius);

#if USE(CG)
    void platformAddPathForRoundedRect(const FloatRect&, const FloatSize& topLeftRadius, const FloatSize& topRightRadius, const FloatSize& bottomLeftRadius, const FloatSize& bottomRightRadius);
#endif

#ifndef NDEBUG
    void dump() const;
#endif

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<Path> decode(Decoder&);

#if ENABLE(INLINE_PATH_DATA)
    template<typename DataType> const DataType& inlineData() const;
    InlinePathData inlineData() const { return m_inlineData; }
    template<typename DataType> bool hasInlineData() const;
    bool hasInlineData() const;
#endif

private:
    class EncodedPathElementType {
    public:
        EncodedPathElementType()
            : m_type(endOfPath)
        {
        }

        EncodedPathElementType(PathElement::Type type)
            : m_type(enumToUnderlyingType(type))
        {
            ASSERT(isValidPathElementType());
        }

        bool isEndOfPath() const { return m_type == endOfPath; }
        bool isValidPathElementType() const { return isValidEnum<PathElement::Type>(m_type); }
        bool isValid() const { return isValidPathElementType() || isEndOfPath(); }

        PathElement::Type type() const
        {
            ASSERT(isValidPathElementType());
            return static_cast<PathElement::Type>(m_type);
        }

        template <typename Encoder>
        void encode(Encoder& encoder)
        {
            encoder << m_type;
        }

        template <typename Decoder>
        static std::optional<EncodedPathElementType> decode(Decoder& decoder)
        {
            EncodedPathElementType type;
            if (!decoder.decode(type.m_type))
                return std::nullopt;

            if (!type.isValid())
                return std::nullopt;

            return type;
        }

    private:
        using UnderlyingType = std::underlying_type_t<PathElement::Type>;

        static inline constexpr auto endOfPath = std::numeric_limits<UnderlyingType>::max();

        UnderlyingType m_type;
    };

#if ENABLE(INLINE_PATH_DATA)
    template<typename DataType> DataType& inlineData();
    std::optional<FloatRect> fastBoundingRectFromInlineData() const;
    std::optional<FloatRect> boundingRectFromInlineData() const;
#endif

    void moveToSlowCase(const FloatPoint&);
    void addLineToSlowCase(const FloatPoint&);
    void addArcSlowCase(const FloatPoint&, float radius, float startAngle, float endAngle, bool anticlockwise);
    void addQuadCurveToSlowCase(const FloatPoint& controlPoint, const FloatPoint& endPoint);
    void addBezierCurveToSlowCase(const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, const FloatPoint& endPoint);

    FloatRect boundingRectSlowCase() const;
    FloatRect fastBoundingRectSlowCase() const;
    bool isEmptySlowCase() const;
    FloatPoint currentPointSlowCase() const;
    size_t elementCountSlowCase() const;
    void applySlowCase(const PathApplierFunction&) const;

#if USE(CG)
    void createCGPath() const;
    void swap(Path&);
#endif

#if USE(CAIRO)
    cairo_t* ensureCairoPath();
    void appendElement(PathElement::Type, Vector<FloatPoint, 3>&&);
#endif

#if USE(CAIRO)
    RefPtr<cairo_t> m_path;
#else
    mutable PlatformPathStorageType m_path;
#endif

#if ENABLE(INLINE_PATH_DATA)
    InlinePathData m_inlineData;
#endif
#if USE(CG)
    mutable bool m_copyPathBeforeMutation { false };
#endif
#if USE(CAIRO)
    std::optional<Vector<PathElement>> m_elements;
#endif
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const Path&);

template<class Encoder> void Path::encode(Encoder& encoder) const
{
#if ENABLE(INLINE_PATH_DATA)
    bool hasInlineData = this->hasInlineData();
    encoder << hasInlineData;
    if (hasInlineData) {
        encoder << m_inlineData;
        return;
    }
#endif

    apply([&](auto& element) {
        encoder << EncodedPathElementType(element.type);

        switch (element.type) {
        case PathElement::Type::MoveToPoint:
            encoder << element.points[0];
            break;
        case PathElement::Type::AddLineToPoint:
            encoder << element.points[0];
            break;
        case PathElement::Type::AddQuadCurveToPoint:
            encoder << element.points[0];
            encoder << element.points[1];
            break;
        case PathElement::Type::AddCurveToPoint:
            encoder << element.points[0];
            encoder << element.points[1];
            encoder << element.points[2];
            break;
        case PathElement::Type::CloseSubpath:
            break;
        }
    });

    encoder << EncodedPathElementType();
}

template<class Decoder> std::optional<Path> Path::decode(Decoder& decoder)
{
    Path path;

#if ENABLE(INLINE_PATH_DATA)
    bool hasInlineData;
    if (!decoder.decode(hasInlineData))
        return std::nullopt;

    if (hasInlineData) {
        if (!decoder.decode(path.m_inlineData))
            return std::nullopt;

        return path;
    }
#endif

    path.clear();

    while (true) {
        EncodedPathElementType elementType;
        if (!decoder.decode(elementType))
            return std::nullopt;

        if (elementType.isEndOfPath())
            break;

        switch (elementType.type()) {
        case PathElement::Type::MoveToPoint: {
            FloatPoint point;
            if (!decoder.decode(point))
                return std::nullopt;
            path.moveTo(point);
            break;
        }
        case PathElement::Type::AddLineToPoint: {
            FloatPoint point;
            if (!decoder.decode(point))
                return std::nullopt;
            path.addLineTo(point);
            break;
        }
        case PathElement::Type::AddQuadCurveToPoint: {
            FloatPoint controlPoint;
            if (!decoder.decode(controlPoint))
                return std::nullopt;

            FloatPoint endPoint;
            if (!decoder.decode(endPoint))
                return std::nullopt;

            path.addQuadCurveTo(controlPoint, endPoint);
            break;
        }
        case PathElement::Type::AddCurveToPoint: {
            FloatPoint controlPoint1;
            if (!decoder.decode(controlPoint1))
                return std::nullopt;

            FloatPoint controlPoint2;
            if (!decoder.decode(controlPoint2))
                return std::nullopt;

            FloatPoint endPoint;
            if (!decoder.decode(endPoint))
                return std::nullopt;

            path.addBezierCurveTo(controlPoint1, controlPoint2, endPoint);
            break;
        }
        case PathElement::Type::CloseSubpath:
            path.closeSubpath();
            break;
        }
    }

    return path;
}

#if ENABLE(INLINE_PATH_DATA)

template <typename DataType> inline bool Path::hasInlineData() const
{
    return std::holds_alternative<DataType>(m_inlineData);
}

template<typename DataType> inline const DataType& Path::inlineData() const
{
    return std::get<DataType>(m_inlineData);
}

template<typename DataType> inline DataType& Path::inlineData()
{
    return std::get<DataType>(m_inlineData);
}

inline bool Path::hasInlineData() const
{
    return !hasInlineData<std::monostate>();
}

#endif

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::PathElement::Type> {
    using values = EnumValues<
        WebCore::PathElement::Type,
        WebCore::PathElement::Type::MoveToPoint,
        WebCore::PathElement::Type::AddLineToPoint,
        WebCore::PathElement::Type::AddQuadCurveToPoint,
        WebCore::PathElement::Type::AddCurveToPoint,
        WebCore::PathElement::Type::CloseSubpath
    >;
};

} // namespace WTF
