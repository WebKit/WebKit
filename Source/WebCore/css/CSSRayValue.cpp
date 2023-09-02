/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSRayValue.h"

#include "CSSPrimitiveValueMappings.h"

namespace WebCore {

String CSSRayValue::customCSSText() const
{
    bool isNonDefaultSize = m_size != CSSValueClosestSide;
    bool hasPosition = m_position;
    bool hasCustomCoordinateBox = m_coordinateBox != CSSBoxType::BoxMissing;

    return makeString(
        "ray("_s, m_angle->cssText(),
        isNonDefaultSize ? " "_s : ""_s,
        isNonDefaultSize ? nameLiteral(m_size) : ""_s,
        m_isContaining ? " contain"_s : ""_s,
        hasPosition ? " at "_s : ""_s,
        hasPosition ? m_position->cssText() : ""_s,
        ')',
        hasCustomCoordinateBox ? " "_s : ""_s,
        hasCustomCoordinateBox ? nameLiteral(toCSSValueID(m_coordinateBox)) : ""_s
    );
}

bool CSSRayValue::equals(const CSSRayValue& other) const
{
    return compareCSSValue(m_angle, other.m_angle)
        && m_size == other.m_size
        && m_isContaining == other.m_isContaining
        && compareCSSValuePtr(m_position, other.m_position)
        && m_coordinateBox == other.m_coordinateBox;
}

}
