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

#include "ColorSpace.h"
#include "ColorUtilities.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ExtendedColor : public RefCounted<ExtendedColor> {
public:
    static Ref<ExtendedColor> create(float, float, float, float alpha, ColorSpace = ColorSpace::SRGB);

    float alpha() const { return m_channels.components[3]; }

    const FloatComponents& channels() const { return m_channels; }
    ColorSpace colorSpace() const { return m_colorSpace; }

    WEBCORE_EXPORT unsigned hash() const;

    WEBCORE_EXPORT String cssText() const;

    Ref<ExtendedColor> colorWithAlpha(float) const;
    Ref<ExtendedColor> invertedColorWithAlpha(float) const;

    bool isWhite() const;
    bool isBlack() const;

private:
    ExtendedColor(float c1, float c2, float c3, float alpha, ColorSpace colorSpace)
        : m_channels(c1, c2, c3, alpha)
        , m_colorSpace(colorSpace)
    { }

    FloatComponents m_channels;
    ColorSpace m_colorSpace { ColorSpace::SRGB };
};

inline bool operator==(const ExtendedColor& a, const ExtendedColor& b)
{
    return a.colorSpace() == b.colorSpace() && a.channels() == b.channels();
}

inline bool operator!=(const ExtendedColor& a, const ExtendedColor& b)
{
    return !(a == b);
}

}
