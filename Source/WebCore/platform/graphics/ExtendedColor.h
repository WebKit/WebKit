/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "ColorComponents.h"
#include "ColorSpace.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Color;

class ExtendedColor : public RefCounted<ExtendedColor> {
public:
    WEBCORE_EXPORT static Ref<ExtendedColor> create(float c1, float c2, float c3, float alpha, ColorSpace);

    float alpha() const { return m_components[3]; }

    const ColorComponents<float>& components() const { return m_components; }
    ColorSpace colorSpace() const { return m_colorSpace; }

    WEBCORE_EXPORT unsigned hash() const;

    WEBCORE_EXPORT String cssText() const;

    Ref<ExtendedColor> colorWithAlpha(float) const;
    Ref<ExtendedColor> invertedColorWithAlpha(float) const;

    ColorComponents<float> toSRGBAComponentsLossy() const;

    bool isWhite() const;
    bool isBlack() const;

private:
    ExtendedColor(float c1, float c2, float c3, float alpha, ColorSpace colorSpace)
        : m_components(c1, c2, c3, alpha)
        , m_colorSpace(colorSpace)
    {
    }

    ColorComponents<float> m_components;
    ColorSpace m_colorSpace;
};

inline bool operator==(const ExtendedColor& a, const ExtendedColor& b)
{
    return a.colorSpace() == b.colorSpace() && a.components() == b.components();
}

inline bool operator!=(const ExtendedColor& a, const ExtendedColor& b)
{
    return !(a == b);
}

// FIXME: If the ColorSpace is sRGB and the values can all be
// converted exactly to integers, we should make a SimpleColor.
WEBCORE_EXPORT Color makeExtendedColor(float c1, float c2, float c3, float alpha, ColorSpace);

}
