/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
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
#include "RenderStyleConstants.h"
#include "StyleImage.h"

namespace WebCore {

struct BlendingContext;

class ShapeValue : public RefCounted<ShapeValue> {
public:
    static Ref<ShapeValue> create(Ref<BasicShape>&& shape, CSSBoxType cssBox)
    {
        return adoptRef(*new ShapeValue(WTFMove(shape), cssBox));
    }

    static Ref<ShapeValue> create(CSSBoxType boxShape)
    {
        return adoptRef(*new ShapeValue(boxShape));
    }

    static Ref<ShapeValue> create(Ref<StyleImage>&& image)
    {
        return adoptRef(*new ShapeValue(WTFMove(image)));
    }

    enum class Type { Shape, Box, Image };
    Type type() const { return m_type; }
    BasicShape* shape() const { return m_shape.get(); }
    CSSBoxType cssBox() const { return m_cssBox; }
    CSSBoxType effectiveCSSBox() const;
    StyleImage* image() const { return m_image.get(); }
    RefPtr<StyleImage> protectedImage() const { return m_image; }
    bool isImageValid() const;

    void setImage(Ref<StyleImage>&& image)
    {
        ASSERT(m_type == Type::Image);
        m_image = WTFMove(image);
    }

    bool canBlend(const ShapeValue&) const;
    Ref<ShapeValue> blend(const ShapeValue&, const BlendingContext&) const;

    bool operator==(const ShapeValue&) const;

private:
    ShapeValue(Ref<BasicShape>&& shape, CSSBoxType cssBox)
        : m_type(Type::Shape)
        , m_shape(WTFMove(shape))
        , m_cssBox(cssBox)
    {
    }

    explicit ShapeValue(Ref<StyleImage>&& image)
        : m_type(Type::Image)
        , m_image(WTFMove(image))
    {
    }

    explicit ShapeValue(CSSBoxType cssBox)
        : m_type(Type::Box)
        , m_cssBox(cssBox)
    {
    }

    Type m_type;
    RefPtr<BasicShape> m_shape;
    RefPtr<StyleImage> m_image;
    CSSBoxType m_cssBox { CSSBoxType::BoxMissing };
};

}
