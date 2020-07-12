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
#include "ColorConversion.h"
#include "ColorSpace.h"
#include "ColorTypes.h"
#include <functional>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class ExtendedColor : public RefCounted<ExtendedColor> {
public:
    template<typename ColorType> static Ref<ExtendedColor> create(const ColorType&);
    static Ref<ExtendedColor> create(ColorComponents<float>, ColorSpace);
    
    float alpha() const { return m_components[3]; }

    const ColorComponents<float>& components() const { return m_components; }
    ColorSpace colorSpace() const { return m_colorSpace; }

    template<typename Functor> decltype(auto) callOnUnderlyingType(Functor&&) const;

private:
    ExtendedColor(ColorComponents<float>, ColorSpace);

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

template<typename ColorType> inline Ref<ExtendedColor> ExtendedColor::create(const ColorType& color)
{
    return adoptRef(*new ExtendedColor(asColorComponents(color), color.colorSpace));
}

inline Ref<ExtendedColor> ExtendedColor::create(ColorComponents<float> components, ColorSpace colorSpace)
{
    return adoptRef(*new ExtendedColor(components, colorSpace));
}

inline ExtendedColor::ExtendedColor(ColorComponents<float> components, ColorSpace colorSpace)
    : m_components(components)
    , m_colorSpace(colorSpace)
{
}

template<typename Functor> decltype(auto) ExtendedColor::callOnUnderlyingType(Functor&& functor) const
{
    switch (m_colorSpace) {
    case ColorSpace::SRGB:
        return std::invoke(std::forward<Functor>(functor), asSRGBA(m_components));
    case ColorSpace::LinearRGB:
        return std::invoke(std::forward<Functor>(functor), asLinearSRGBA(m_components));
    case ColorSpace::DisplayP3:
        return std::invoke(std::forward<Functor>(functor), asDisplayP3(m_components));
    }

    ASSERT_NOT_REACHED();
    return std::invoke(std::forward<Functor>(functor), asSRGBA(m_components));
}

}
