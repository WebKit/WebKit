/*
 * Copyright (C) 2012, 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include "BasicShapes.h"
#include "OffsetRotation.h"
#include "Path.h"
#include "RenderStyleConstants.h"
#include <wtf/RefCounted.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class SVGElement;

class PathOperation : public RefCounted<PathOperation> {
public:
    enum OperationType {
        Reference,
        Shape,
        Box,
        Ray
    };

    virtual ~PathOperation() = default;

    virtual bool operator==(const PathOperation&) const = 0;
    bool operator!=(const PathOperation& o) const { return !(*this == o); }

    OperationType type() const { return m_type; }
    bool isSameType(const PathOperation& o) const { return o.type() == m_type; }
    virtual const std::optional<Path> getPath(const FloatRect& reference = { }, FloatPoint anchor = { }, OffsetRotation rotation = { }) const = 0;
protected:
    explicit PathOperation(OperationType type)
        : m_type(type)
    {
    }
    OperationType m_type;
};

class ReferencePathOperation final : public PathOperation {
public:
    static Ref<ReferencePathOperation> create(const String& url, const AtomString& fragment, const RefPtr<SVGElement>);
    WEBCORE_EXPORT static Ref<ReferencePathOperation> create(std::optional<Path>&&);
    const String& url() const { return m_url; }
    const AtomString& fragment() const { return m_fragment; }
    const SVGElement* element() const;
    const std::optional<Path> getPath(const FloatRect&, FloatPoint, OffsetRotation) const final { return m_path; }
    const std::optional<Path> path() const { return m_path; }
private:
    bool operator==(const PathOperation& other) const override
    {
        if (!isSameType(other))
            return false;
        auto& referenceClip = downcast<ReferencePathOperation>(other);
        return m_url == referenceClip.m_url;
    }

    ReferencePathOperation(const String& url, const AtomString& fragment, const RefPtr<SVGElement>);
    ReferencePathOperation(std::optional<Path>&&);

    String m_url;
    AtomString m_fragment;
    RefPtr<SVGElement> m_element;
    std::optional<Path> m_path;
};

class ShapePathOperation final : public PathOperation {
public:
    static Ref<ShapePathOperation> create(Ref<BasicShape>&& shape)
    {
        return adoptRef(*new ShapePathOperation(WTFMove(shape)));
    }

    static Ref<ShapePathOperation> create(Ref<BasicShape>&& shape, CSSBoxType referenceBox)
    {
        return adoptRef(*new ShapePathOperation(WTFMove(shape), referenceBox));
    }

    const BasicShape& basicShape() const { return m_shape; }
    const Ref<BasicShape>& shape() const { return m_shape; }
    WindRule windRule() const { return m_shape.get().windRule(); }
    const Path& pathForReferenceRect(const FloatRect& boundingRect) const { return m_shape.get().path(boundingRect); }

    void setReferenceBox(CSSBoxType referenceBox) { m_referenceBox = referenceBox; }
    CSSBoxType referenceBox() const { return m_referenceBox; }
    const std::optional<Path> getPath(const FloatRect& reference, FloatPoint, OffsetRotation) const final { return pathForReferenceRect(reference); }

private:
    bool operator==(const PathOperation& other) const override
    {
        if (!isSameType(other))
            return false;
        auto& shapeClip = downcast<ShapePathOperation>(other);
        return m_referenceBox == shapeClip.referenceBox()
            && (m_shape.ptr() == shapeClip.m_shape.ptr() || m_shape.get() == shapeClip.m_shape.get());
    }

    explicit ShapePathOperation(Ref<BasicShape>&& shape)
        : PathOperation(Shape)
        , m_shape(WTFMove(shape))
        , m_referenceBox(CSSBoxType::BoxMissing)
    {
    }

    ShapePathOperation(Ref<BasicShape>&& shape, CSSBoxType referenceBox)
        : PathOperation(Shape)
        , m_shape(WTFMove(shape))
        , m_referenceBox(referenceBox)
    {
    }

    Ref<BasicShape> m_shape;
    CSSBoxType m_referenceBox;
};

class BoxPathOperation final : public PathOperation {
public:
    static Ref<BoxPathOperation> create(CSSBoxType referenceBox)
    {
        return adoptRef(*new BoxPathOperation(referenceBox));
    }

    static Ref<BoxPathOperation> create(Path&& path, CSSBoxType referenceBox)
    {
        return adoptRef(*new BoxPathOperation(WTFMove(path), referenceBox));
    }

    const Path pathForReferenceRect(const FloatRoundedRect& boundingRect) const
    {
        Path path;
        path.addRoundedRect(boundingRect);
        return path;
    }
    
    void setPathForReferenceRect(const FloatRoundedRect& boundingRect)
    {
        m_path.clear();
        m_path.addRoundedRect(boundingRect);
    }
    
    const std::optional<Path> getPath(const FloatRect&, FloatPoint, OffsetRotation) const final { return m_path; }
    const Path& path() const { return m_path; }
    CSSBoxType referenceBox() const { return m_referenceBox; }

private:
    bool operator==(const PathOperation& other) const override
    {
        if (!isSameType(other))
            return false;
        auto& boxClip = downcast<BoxPathOperation>(other);
        return m_referenceBox == boxClip.m_referenceBox;
    }

    explicit BoxPathOperation(CSSBoxType referenceBox)
        : PathOperation(Box)
        , m_referenceBox(referenceBox)
    {
    }

    BoxPathOperation(Path&& path, CSSBoxType referenceBox)
        : PathOperation(Box)
        , m_path(WTFMove(path))
        , m_referenceBox(referenceBox)
    {
    }

    Path m_path;
    CSSBoxType m_referenceBox;
};


class RayPathOperation final : public PathOperation {
public:
    enum class Size : uint8_t {
        ClosestSide,
        ClosestCorner,
        FarthestSide,
        FarthestCorner,
        Sides
    };

    static Ref<RayPathOperation> create(float angle, Size size, bool isContaining)
    {
        return adoptRef(*new RayPathOperation(angle, size, isContaining));
    }

    WEBCORE_EXPORT static Ref<RayPathOperation> create(float angle, Size, bool isContaining, FloatRect&& containingBlockBoundingRect, FloatPoint&& position);

    float angle() const { return m_angle; }
    Size size() const { return m_size; }
    bool isContaining() const { return m_isContaining; }

    bool canBlend(const RayPathOperation& other) const
    {
        // Two rays can only be blended if they have the same size and are both containing.
        return m_size == other.m_size && m_isContaining == other.m_isContaining;
    }

    Ref<RayPathOperation> blend(const RayPathOperation& to, const BlendingContext& context) const
    {
        return RayPathOperation::create(WebCore::blend(m_angle, to.m_angle, context), m_size, m_isContaining);
    }

    double lengthForPath() const;
    double lengthForContainPath(const FloatRect& elementRect, double computedPathLength, const FloatPoint& anchor, const OffsetRotation rotation) const;
    
    void setContainingBlockReferenceRect(const FloatRect& boundingRect)
    {
        m_containingBlockBoundingRect = boundingRect;
    }
    void setStartingPosition(const FloatPoint& position)
    {
        m_position = position;
    }
    const std::optional<Path> getPath(const FloatRect& referenceRect = { }, FloatPoint anchor = { }, OffsetRotation rotation = { }) const final;

    const FloatRect& containingBlockBoundingRect() const { return m_containingBlockBoundingRect; }
    const FloatPoint& position() const { return m_position; }

private:
    bool operator==(const PathOperation& other) const override
    {
        if (!isSameType(other))
            return false;

        auto& otherCasted = downcast<RayPathOperation>(other);
        return m_angle == otherCasted.m_angle
            && m_size == otherCasted.m_size
            && m_isContaining == otherCasted.m_isContaining;
    }

    RayPathOperation(float angle, Size size, bool isContaining)
        : PathOperation(Ray)
        , m_angle(angle)
        , m_size(size)
        , m_isContaining(isContaining)
    {
    }

    RayPathOperation(float angle, Size size, bool isContaining, FloatRect&& containingBlockBoundingRect, FloatPoint&& position)
        : PathOperation(Ray)
        , m_angle(angle)
        , m_size(size)
        , m_isContaining(isContaining)
        , m_containingBlockBoundingRect(WTFMove(containingBlockBoundingRect))
        , m_position(WTFMove(position))
    {
    }

    float m_angle { 0 };
    Size m_size;
    bool m_isContaining { false };
    FloatRect m_containingBlockBoundingRect;
    FloatPoint m_position;
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_CLIP_PATH_OPERATION(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::PathOperation& operation) { return operation.type() == WebCore::predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_CLIP_PATH_OPERATION(ReferencePathOperation, PathOperation::Reference)
SPECIALIZE_TYPE_TRAITS_CLIP_PATH_OPERATION(ShapePathOperation, PathOperation::Shape)
SPECIALIZE_TYPE_TRAITS_CLIP_PATH_OPERATION(BoxPathOperation, PathOperation::Box)
SPECIALIZE_TYPE_TRAITS_CLIP_PATH_OPERATION(RayPathOperation, PathOperation::Ray)
