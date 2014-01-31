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

#ifndef ClipPathOperation_h
#define ClipPathOperation_h

#include "BasicShapes.h"
#include "Path.h"
#include "RenderStyleConstants.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ClipPathOperation : public RefCounted<ClipPathOperation> {
public:
    enum OperationType {
        Reference,
        Shape,
        Box
    };

    virtual ~ClipPathOperation() { }

    virtual bool operator==(const ClipPathOperation&) const = 0;
    bool operator!=(const ClipPathOperation& o) const { return !(*this == o); }

    virtual OperationType type() const { return m_type; }
    virtual bool isSameType(const ClipPathOperation& o) const { return o.type() == m_type; }

protected:
    explicit ClipPathOperation(OperationType type)
        : m_type(type)
    {
    }

    OperationType m_type;
};

class ReferenceClipPathOperation : public ClipPathOperation {
public:
    static PassRefPtr<ReferenceClipPathOperation> create(const String& url, const String& fragment)
    {
        return adoptRef(new ReferenceClipPathOperation(url, fragment));
    }

    const String& url() const { return m_url; }
    const String& fragment() const { return m_fragment; }

private:
    virtual bool operator==(const ClipPathOperation& o) const override
    {
        if (!isSameType(o))
            return false;
        const ReferenceClipPathOperation* other = static_cast<const ReferenceClipPathOperation*>(&o);
        return m_url == other->m_url;
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

class ShapeClipPathOperation : public ClipPathOperation {
public:
    static PassRefPtr<ShapeClipPathOperation> create(PassRefPtr<BasicShape> shape)
    {
        return adoptRef(new ShapeClipPathOperation(shape));
    }

    const BasicShape* basicShape() const { return m_shape.get(); }
    WindRule windRule() const { return m_shape->windRule(); }
    const Path pathForReferenceRect(const FloatRect& boundingRect) const
    {
        ASSERT(m_shape);
        Path path;
        m_shape->path(path, boundingRect);
        return path;
    }

    void setReferenceBox(LayoutBox referenceBox) { m_referenceBox = referenceBox; }
    LayoutBox referenceBox() const { return m_referenceBox; }

private:
    virtual bool operator==(const ClipPathOperation& o) const override
    {
        if (!isSameType(o))
            return false;
        const ShapeClipPathOperation* other = static_cast<const ShapeClipPathOperation*>(&o);
        return m_shape == other->m_shape;
    }

    explicit ShapeClipPathOperation(PassRefPtr<BasicShape> shape)
        : ClipPathOperation(Shape)
        , m_shape(shape)
        , m_referenceBox(BoxMissing)
    {
    }

    RefPtr<BasicShape> m_shape;
    LayoutBox m_referenceBox;
};

class BoxClipPathOperation : public ClipPathOperation {
public:
    static PassRefPtr<BoxClipPathOperation> create(LayoutBox referenceBox)
    {
        return adoptRef(new BoxClipPathOperation(referenceBox));
    }

    const Path pathForReferenceRect(const RoundedRect& boundingRect) const
    {
        Path path;
        path.addRoundedRect(boundingRect);
        return path;
    }
    LayoutBox referenceBox() const { return m_referenceBox; }

private:
    virtual bool operator==(const ClipPathOperation& o) const override
    {
        if (!isSameType(o))
            return false;
        const BoxClipPathOperation* other = static_cast<const BoxClipPathOperation*>(&o);
        return m_referenceBox == other->m_referenceBox;
    }

    explicit BoxClipPathOperation(LayoutBox referenceBox)
        : ClipPathOperation(Box)
        , m_referenceBox(referenceBox)
    {
    }

    LayoutBox m_referenceBox;
};

#define CLIP_PATH_OPERATION_CASTS(ToValueTypeName, predicate) \
    TYPE_CASTS_BASE(ToValueTypeName, ClipPathOperation, operation, operation->type() == ClipPathOperation::predicate, operation.type() == ClipPathOperation::predicate)

CLIP_PATH_OPERATION_CASTS(ReferenceClipPathOperation, Reference)
CLIP_PATH_OPERATION_CASTS(ShapeClipPathOperation, Shape)
CLIP_PATH_OPERATION_CASTS(BoxClipPathOperation, Box)

} // namespace WebCore

#endif // ClipPathOperation_h
