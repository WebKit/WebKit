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

class ClipPathOperation : public RefCounted<ClipPathOperation> {
public:
    enum OperationType {
        Reference,
        Shape,
        Box
    };

    virtual ~ClipPathOperation() = default;

    virtual bool operator==(const ClipPathOperation&) const = 0;
    bool operator!=(const ClipPathOperation& o) const { return !(*this == o); }

    OperationType type() const { return m_type; }
    bool isSameType(const ClipPathOperation& o) const { return o.type() == m_type; }

protected:
    explicit ClipPathOperation(OperationType type)
        : m_type(type)
    {
    }

    OperationType m_type;
};

class ReferenceClipPathOperation final : public ClipPathOperation {
public:
    static Ref<ReferenceClipPathOperation> create(const String& url, const String& fragment)
    {
        return adoptRef(*new ReferenceClipPathOperation(url, fragment));
    }

    const String& url() const { return m_url; }
    const String& fragment() const { return m_fragment; }

private:
    bool operator==(const ClipPathOperation& other) const override
    {
        if (!isSameType(other))
            return false;
        auto& referenceClip = downcast<ReferenceClipPathOperation>(other);
        return m_url == referenceClip.m_url;
    }

    ReferenceClipPathOperation(const String& url, const String& fragment)
        : ClipPathOperation(Reference)
        , m_url(url)
        , m_fragment(fragment)
    {
    }

    String m_url;
    String m_fragment;
};

class ShapeClipPathOperation final : public ClipPathOperation {
public:
    static Ref<ShapeClipPathOperation> create(Ref<BasicShape>&& shape)
    {
        return adoptRef(*new ShapeClipPathOperation(WTFMove(shape)));
    }

    const BasicShape& basicShape() const { return m_shape; }
    WindRule windRule() const { return m_shape.get().windRule(); }
    const Path& pathForReferenceRect(const FloatRect& boundingRect) { return m_shape.get().path(boundingRect); }

    void setReferenceBox(CSSBoxType referenceBox) { m_referenceBox = referenceBox; }
    CSSBoxType referenceBox() const { return m_referenceBox; }

private:
    bool operator==(const ClipPathOperation& other) const override
    {
        if (!isSameType(other))
            return false;
        auto& shapeClip = downcast<ShapeClipPathOperation>(other);
        return m_referenceBox == shapeClip.referenceBox()
            && (m_shape.ptr() == shapeClip.m_shape.ptr() || m_shape.get() == shapeClip.m_shape.get());
    }

    explicit ShapeClipPathOperation(Ref<BasicShape>&& shape)
        : ClipPathOperation(Shape)
        , m_shape(WTFMove(shape))
        , m_referenceBox(CSSBoxType::BoxMissing)
    {
    }

    Ref<BasicShape> m_shape;
    CSSBoxType m_referenceBox;
};

class BoxClipPathOperation final : public ClipPathOperation {
public:
    static Ref<BoxClipPathOperation> create(CSSBoxType referenceBox)
    {
        return adoptRef(*new BoxClipPathOperation(referenceBox));
    }

    const Path pathForReferenceRect(const FloatRoundedRect& boundingRect) const
    {
        Path path;
        path.addRoundedRect(boundingRect);
        return path;
    }
    CSSBoxType referenceBox() const { return m_referenceBox; }

private:
    bool operator==(const ClipPathOperation& other) const override
    {
        if (!isSameType(other))
            return false;
        auto& boxClip = downcast<BoxClipPathOperation>(other);
        return m_referenceBox == boxClip.m_referenceBox;
    }

    explicit BoxClipPathOperation(CSSBoxType referenceBox)
        : ClipPathOperation(Box)
        , m_referenceBox(referenceBox)
    {
    }

    CSSBoxType m_referenceBox;
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_CLIP_PATH_OPERATION(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::ClipPathOperation& operation) { return operation.type() == WebCore::predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_CLIP_PATH_OPERATION(ReferenceClipPathOperation, ClipPathOperation::Reference)
SPECIALIZE_TYPE_TRAITS_CLIP_PATH_OPERATION(ShapeClipPathOperation, ClipPathOperation::Shape)
SPECIALIZE_TYPE_TRAITS_CLIP_PATH_OPERATION(BoxClipPathOperation, ClipPathOperation::Box)
