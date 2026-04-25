/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CornerRadii.h"

#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CornerRadii);

bool CornerRadii::hasEvenCorners() const
{
    return areEssentiallyEqual(m_topLeft, m_topRight)
        && areEssentiallyEqual(m_topLeft, m_bottomLeft)
        && areEssentiallyEqual(m_topLeft, m_bottomRight);
}

bool CornerRadii::isUniformCornerRadius() const
{
    return WTF::areEssentiallyEqual(m_topLeft.width(), m_topLeft.height()) && hasEvenCorners();
}

void CornerRadii::scale(float factor)
{
    scale(factor, factor);
}

void CornerRadii::scale(float horizontalFactor, float verticalFactor)
{
    if (horizontalFactor == 1 && verticalFactor == 1)
        return;

    // If either radius on a corner becomes zero, reset both radii on that corner.
    m_topLeft.scale(horizontalFactor, verticalFactor);
    if (!m_topLeft.width() || !m_topLeft.height())
        m_topLeft = FloatSize();
    m_topRight.scale(horizontalFactor, verticalFactor);
    if (!m_topRight.width() || !m_topRight.height())
        m_topRight = FloatSize();
    m_bottomLeft.scale(horizontalFactor, verticalFactor);
    if (!m_bottomLeft.width() || !m_bottomLeft.height())
        m_bottomLeft = FloatSize();
    m_bottomRight.scale(horizontalFactor, verticalFactor);
    if (!m_bottomRight.width() || !m_bottomRight.height())
        m_bottomRight = FloatSize();
}

void CornerRadii::expand(float topWidth, float bottomWidth, float leftWidth, float rightWidth)
{
    if (m_topLeft.width() > 0 && m_topLeft.height() > 0) {
        m_topLeft.setWidth(std::max<float>(0, m_topLeft.width() + leftWidth));
        m_topLeft.setHeight(std::max<float>(0, m_topLeft.height() + topWidth));
    }
    if (m_topRight.width() > 0 && m_topRight.height() > 0) {
        m_topRight.setWidth(std::max<float>(0, m_topRight.width() + rightWidth));
        m_topRight.setHeight(std::max<float>(0, m_topRight.height() + topWidth));
    }
    if (m_bottomLeft.width() > 0 && m_bottomLeft.height() > 0) {
        m_bottomLeft.setWidth(std::max<float>(0, m_bottomLeft.width() + leftWidth));
        m_bottomLeft.setHeight(std::max<float>(0, m_bottomLeft.height() + bottomWidth));
    }
    if (m_bottomRight.width() > 0 && m_bottomRight.height() > 0) {
        m_bottomRight.setWidth(std::max<float>(0, m_bottomRight.width() + rightWidth));
        m_bottomRight.setHeight(std::max<float>(0, m_bottomRight.height() + bottomWidth));
    }
}

void CornerRadii::expandEvenIfZero(float size)
{
    auto expand = [&](auto& corner) {
        corner.setWidth(std::max(0.f, corner.width() + size));
        corner.setHeight(std::max(0.f, corner.height() + size));
    };

    expand(m_topLeft);
    expand(m_topRight);
    expand(m_bottomLeft);
    expand(m_bottomRight);
}

TextStream& operator<<(TextStream& ts, const CornerRadii& cornerRadii)
{
    ts.dumpProperty("top-left"_s, cornerRadii.topLeft());
    ts.dumpProperty("top-right"_s, cornerRadii.topRight());
    ts.dumpProperty("bottom-left"_s, cornerRadii.bottomLeft());
    ts.dumpProperty("bottom-right"_s, cornerRadii.bottomRight());
    return ts;
}

} // namespace WebCore
