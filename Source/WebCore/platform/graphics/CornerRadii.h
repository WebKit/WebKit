/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/FloatSize.h>
#include <WebCore/LayoutRoundedRect.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class CornerRadii {
    WTF_MAKE_TZONE_ALLOCATED(CornerRadii);
public:
    CornerRadii() = default;
    CornerRadii(const FloatSize& topLeft, const FloatSize& topRight, const FloatSize& bottomLeft, const FloatSize& bottomRight)
        : m_topLeft(topLeft)
        , m_topRight(topRight)
        , m_bottomLeft(bottomLeft)
        , m_bottomRight(bottomRight)
    {
    }

    CornerRadii(float topLeft, float topRight, float bottomLeft, float bottomRight)
        : m_topLeft(FloatSize { topLeft, topLeft })
        , m_topRight(FloatSize { topRight, topRight })
        , m_bottomLeft(FloatSize { bottomLeft, bottomLeft })
        , m_bottomRight(FloatSize { bottomRight, bottomRight })
    {
    }

    CornerRadii(const LayoutRoundedRect::Radii& intRadii)
        : m_topLeft(intRadii.topLeft())
        , m_topRight(intRadii.topRight())
        , m_bottomLeft(intRadii.bottomLeft())
        , m_bottomRight(intRadii.bottomRight())
    {
    }

    explicit CornerRadii(float uniformRadius)
        : m_topLeft(uniformRadius, uniformRadius)
        , m_topRight(uniformRadius, uniformRadius)
        , m_bottomLeft(uniformRadius, uniformRadius)
        , m_bottomRight(uniformRadius, uniformRadius)
    {
    }

    explicit CornerRadii(float uniformRadiusWidth, float uniformRadiusHeight)
        : m_topLeft(uniformRadiusWidth, uniformRadiusHeight)
        , m_topRight(uniformRadiusWidth, uniformRadiusHeight)
        , m_bottomLeft(uniformRadiusWidth, uniformRadiusHeight)
        , m_bottomRight(uniformRadiusWidth, uniformRadiusHeight)
    {
    }

    void setTopLeft(const FloatSize& size) { m_topLeft = size; }
    void setTopRight(const FloatSize& size) { m_topRight = size; }
    void setBottomLeft(const FloatSize& size) { m_bottomLeft = size; }
    void setBottomRight(const FloatSize& size) { m_bottomRight = size; }
    const FloatSize& topLeft() const { return m_topLeft; }
    const FloatSize& topRight() const { return m_topRight; }
    const FloatSize& bottomLeft() const { return m_bottomLeft; }
    const FloatSize& bottomRight() const { return m_bottomRight; }

    bool isZero() const { return m_topLeft.isZero() && m_topRight.isZero() && m_bottomLeft.isZero() && m_bottomRight.isZero(); }
    bool hasEvenCorners() const;
    bool isUniformCornerRadius() const; // Including no radius.

    void scale(float factor);
    void scale(float horizontalFactor, float verticalFactor);
    void expandEvenIfZero(float size);
    void expand(float topWidth, float bottomWidth, float leftWidth, float rightWidth);
    void expand(float size) { expand(size, size, size, size); }
    void shrink(float topWidth, float bottomWidth, float leftWidth, float rightWidth) { expand(-topWidth, -bottomWidth, -leftWidth, -rightWidth); }
    void shrink(float size) { shrink(size, size, size, size); }

    friend bool operator==(const CornerRadii&, const CornerRadii&) = default;

private:
    FloatSize m_topLeft;
    FloatSize m_topRight;
    FloatSize m_bottomLeft;
    FloatSize m_bottomRight;
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const CornerRadii&);

} // namespace WebCore
