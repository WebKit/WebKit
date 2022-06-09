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
#include "Path.h"
#include "RenderStyleConstants.h"
#include <wtf/RefCounted.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

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

protected:
    explicit PathOperation(OperationType type)
        : m_type(type)
    {
    }

    OperationType m_type;
};

class ReferencePathOperation final : public PathOperation {
public:
    static Ref<ReferencePathOperation> create(const String& url, const String& fragment)
    {
        return adoptRef(*new ReferencePathOperation(url, fragment));
    }

    const String& url() const { return m_url; }
    const String& fragment() const { return m_fragment; }

private:
    bool operator==(const PathOperation& other) const override
    {
        if (!isSameType(other))
            return false;
        auto& referenceClip = downcast<ReferencePathOperation>(other);
        return m_url == referenceClip.m_url;
    }

    ReferencePathOperation(const String& url, const String& fragment)
        : PathOperation(Reference)
        , m_url(url)
        , m_fragment(fragment)
    {
    }

    String m_url;
    String m_fragment;
};

class ShapePathOperation final : public PathOperation {
public:
    static Ref<ShapePathOperation> create(Ref<BasicShape>&& shape)
    {
        return adoptRef(*new ShapePathOperation(WTFMove(shape)));
    }

    const BasicShape& basicShape() const { return m_shape; }
    WindRule windRule() const { return m_shape.get().windRule(); }
    const Path& pathForReferenceRect(const FloatRect& boundingRect) const { return m_shape.get().path(boundingRect); }

    void setReferenceBox(CSSBoxType referenceBox) { m_referenceBox = referenceBox; }
    CSSBoxType referenceBox() const { return m_referenceBox; }

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

    Ref<BasicShape> m_shape;
    CSSBoxType m_referenceBox;
};

class BoxPathOperation final : public PathOperation {
public:
    static Ref<BoxPathOperation> create(CSSBoxType referenceBox)
    {
        return adoptRef(*new BoxPathOperation(referenceBox));
    }

    const Path pathForReferenceRect(const FloatRoundedRect& boundingRect) const
    {
        Path path;
        path.addRoundedRect(boundingRect);
        return path;
    }
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

    CSSBoxType m_referenceBox;
};


class RayPathOperation final : public PathOperation {
public:
    enum class Size {
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

    float m_angle;
    Size m_size;
    bool m_isContaining;
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
