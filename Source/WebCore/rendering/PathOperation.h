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
#include "LengthPoint.h"
#include "MotionPath.h"
#include "OffsetRotation.h"
#include "Path.h"
#include "RenderStyleConstants.h"
#include "TransformOperationData.h"
#include <wtf/RefCounted.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct BlendingContext;
class SVGElement;

class PathOperation : public RefCounted<PathOperation> {
public:
    enum class Type : uint8_t {
        Reference,
        Shape,
        Box,
        Ray
    };

    virtual ~PathOperation() = default;

    virtual Ref<PathOperation> clone() const = 0;

    virtual bool operator==(const PathOperation&) const = 0;

    virtual bool canBlend(const PathOperation&) const { return false; }
    virtual RefPtr<PathOperation> blend(const PathOperation*, const BlendingContext&) const { return nullptr; }
    virtual std::optional<Path> getPath(const TransformOperationData&) const = 0;

    Type type() const { return m_type; }

    void setReferenceBox(CSSBoxType type) { m_referenceBox = type; }
    CSSBoxType referenceBox() const { return m_referenceBox; }

    bool isSameType(const PathOperation& o) const { return o.type() == m_type; }

protected:
    explicit PathOperation(Type type)
        : m_type(type)
        , m_referenceBox(CSSBoxType::BoxMissing)
    {
    }
    explicit PathOperation(Type type, CSSBoxType referenceBox)
        : m_type(type)
        , m_referenceBox(referenceBox)
    {
    }
    const Type m_type;
    CSSBoxType m_referenceBox;
};

class ReferencePathOperation final : public PathOperation {
public:
    static Ref<ReferencePathOperation> create(const String& url, const AtomString& fragment, const RefPtr<SVGElement>);
    WEBCORE_EXPORT static Ref<ReferencePathOperation> create(std::optional<Path>&&);

    Ref<PathOperation> clone() const final;

    const String& url() const { return m_url; }
    const AtomString& fragment() const { return m_fragment; }

    std::optional<Path> getPath(const TransformOperationData&) const final { return m_path; }
    std::optional<Path> path() const { return m_path; }

private:
    bool operator==(const PathOperation& other) const override
    {
        if (!isSameType(other))
            return false;
        auto& referenceClip = uncheckedDowncast<ReferencePathOperation>(other);
        return m_url == referenceClip.m_url;
    }

    ReferencePathOperation(const String& url, const AtomString& fragment, const RefPtr<SVGElement>);
    ReferencePathOperation(std::optional<Path>&&);

    String m_url;
    AtomString m_fragment;
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

    Ref<PathOperation> clone() const final
    {
        return adoptRef(*new ShapePathOperation(m_shape->clone(), referenceBox()));
    }

    bool canBlend(const PathOperation& to) const final
    {
        auto* operation = dynamicDowncast<ShapePathOperation>(to);
        return operation && m_shape->canBlend(operation->basicShape());
    }

    RefPtr<PathOperation> blend(const PathOperation* to, const BlendingContext& context) const final
    {
        return ShapePathOperation::create(downcast<ShapePathOperation>(*to).basicShape().blend(m_shape, context));
    }

    const BasicShape& basicShape() const { return m_shape; }
    const Ref<BasicShape>& shape() const { return m_shape; }
    WindRule windRule() const { return m_shape.get().windRule(); }
    Path pathForReferenceRect(const FloatRect& boundingRect) const { return m_shape.get().path(boundingRect); }

    std::optional<Path> getPath(const TransformOperationData& data) const final
    {
        return MotionPath::computePathForShape(*this, data);
    }

private:
    bool operator==(const PathOperation& other) const override
    {
        if (!isSameType(other))
            return false;

        auto& shapeClip = uncheckedDowncast<ShapePathOperation>(other);
        return referenceBox() == shapeClip.referenceBox()
            && (m_shape.ptr() == shapeClip.m_shape.ptr() || m_shape.get() == shapeClip.m_shape.get());
    }

    explicit ShapePathOperation(Ref<BasicShape>&& shape)
        : PathOperation(Type::Shape)
        , m_shape(WTFMove(shape))
    {
    }

    ShapePathOperation(Ref<BasicShape>&& shape, CSSBoxType referenceBox)
        : PathOperation(Type::Shape, referenceBox)
        , m_shape(WTFMove(shape))
    {
    }

    Ref<BasicShape> m_shape;
};

class BoxPathOperation final : public PathOperation {
public:
    static Ref<BoxPathOperation> create(CSSBoxType referenceBox)
    {
        return adoptRef(*new BoxPathOperation(referenceBox));
    }

    Ref<PathOperation> clone() const final
    {
        return adoptRef(*new BoxPathOperation(referenceBox()));
    }

    Path pathForReferenceRect(const FloatRoundedRect& boundingRect) const
    {
        Path path;
        path.addRoundedRect(boundingRect);
        return path;
    }
    
    std::optional<Path> getPath(const TransformOperationData& data) const final
    {
        return MotionPath::computePathForBox(*this, data);
    }

private:
    bool operator==(const PathOperation& other) const override
    {
        if (!isSameType(other))
            return false;
        auto& boxClip = uncheckedDowncast<BoxPathOperation>(other);
        return referenceBox() == boxClip.referenceBox();
    }

    explicit BoxPathOperation(CSSBoxType referenceBox)
        : PathOperation(Type::Box, referenceBox)
    {
    }
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

    WEBCORE_EXPORT static Ref<RayPathOperation> create(float angle, Size, bool isContaining, LengthPoint&& position, CSSBoxType = CSSBoxType::BoxMissing);

    Ref<PathOperation> clone() const final;

    float angle() const { return m_angle; }
    Size size() const { return m_size; }
    bool isContaining() const { return m_isContaining; }
    const LengthPoint& position() const { return m_position; }

    WEBCORE_EXPORT bool canBlend(const PathOperation&) const final;
    RefPtr<PathOperation> blend(const PathOperation*, const BlendingContext&) const final;

    double lengthForPath() const;
    double lengthForContainPath(const FloatRect& elementRect, double computedPathLength) const;
    std::optional<Path> getPath(const TransformOperationData&) const final;

private:
    bool operator==(const PathOperation& other) const override
    {
        if (!isSameType(other))
            return false;

        auto& otherCasted = uncheckedDowncast<RayPathOperation>(other);
        return m_angle == otherCasted.m_angle
            && m_size == otherCasted.m_size
            && m_isContaining == otherCasted.m_isContaining
            && m_position == otherCasted.m_position;
    }

    RayPathOperation(float angle, Size size, bool isContaining)
        : PathOperation(Type::Ray)
        , m_angle(angle)
        , m_size(size)
        , m_isContaining(isContaining)
    {
    }

    RayPathOperation(float angle, Size size, bool isContaining, LengthPoint&& position, CSSBoxType referenceBox)
        : PathOperation(Type::Ray, referenceBox)
        , m_angle(angle)
        , m_size(size)
        , m_isContaining(isContaining)
        , m_position(WTFMove(position))
    {
    }

    float m_angle { 0 };
    Size m_size;
    bool m_isContaining { false };
    LengthPoint m_position { Length(LengthType::Auto), Length(LengthType::Auto) };
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_CLIP_PATH_OPERATION(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::PathOperation& operation) { return operation.type() == WebCore::predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_CLIP_PATH_OPERATION(ReferencePathOperation, PathOperation::Type::Reference)
SPECIALIZE_TYPE_TRAITS_CLIP_PATH_OPERATION(ShapePathOperation, PathOperation::Type::Shape)
SPECIALIZE_TYPE_TRAITS_CLIP_PATH_OPERATION(BoxPathOperation, PathOperation::Type::Box)
SPECIALIZE_TYPE_TRAITS_CLIP_PATH_OPERATION(RayPathOperation, PathOperation::Type::Ray)
