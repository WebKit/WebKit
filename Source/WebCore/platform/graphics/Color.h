/*
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
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
#include "ExtendedColor.h"
#include "SimpleColor.h"
#include <wtf/Forward.h>
#include <wtf/HashFunctions.h>
#include <wtf/Optional.h>

#if USE(CG)
typedef struct CGColor* CGColorRef;
#endif

#if PLATFORM(WIN)
struct _D3DCOLORVALUE;
typedef _D3DCOLORVALUE D3DCOLORVALUE;
typedef D3DCOLORVALUE D2D_COLOR_F;
typedef D2D_COLOR_F D2D1_COLOR_F;
struct D2D_VECTOR_4F;
typedef D2D_VECTOR_4F D2D1_VECTOR_4F;
#endif

#if PLATFORM(GTK)
typedef struct _GdkRGBA GdkRGBA;
#endif

namespace WebCore {

class Color {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Color() { }

    Color(SimpleColor color)
    {
        setSimpleColor(color);
    }

    enum SemanticTag { Semantic };
    Color(SimpleColor color, SemanticTag)
        : Color(color)
    {
        tagAsSemantic();
    }

    Color(Ref<ExtendedColor>&& extendedColor)
    {
        // Zero the union, just in case a 32-bit system only assigns the
        // top 32 bits when copying the ExtendedColor pointer below.
        m_colorData.simpleColorAndFlags = 0;
        m_colorData.extendedColor = &extendedColor.leakRef();
        ASSERT(isExtended());
    }

    explicit Color(WTF::HashTableDeletedValueType)
    {
        static_assert(deletedHashValue & invalidSimpleColor, "Color's deleted hash value must not look like an ExtendedColor");
        static_assert(!(deletedHashValue & validSimpleColorBit), "Color's deleted hash value must not look like a valid SimpleColor");
        static_assert(deletedHashValue & (1 << 4), "Color's deleted hash value must have some bits set that an SimpleColor wouldn't have");
        m_colorData.simpleColorAndFlags = deletedHashValue;
        ASSERT(!isExtended());
    }

    bool isHashTableDeletedValue() const
    {
        return m_colorData.simpleColorAndFlags == deletedHashValue;
    }

    explicit Color(WTF::HashTableEmptyValueType)
    {
        static_assert(emptyHashValue & invalidSimpleColor, "Color's empty hash value must not look like an ExtendedColor");
        static_assert(emptyHashValue & (1 << 4), "Color's deleted hash value must have some bits set that an SimpleColor wouldn't have");
        m_colorData.simpleColorAndFlags = emptyHashValue;
        ASSERT(!isExtended());
    }

    WEBCORE_EXPORT Color(const Color&);
    WEBCORE_EXPORT Color(Color&&);

    ~Color()
    {
        if (isExtended())
            m_colorData.extendedColor->deref();
    }

    bool isValid() const { return isExtended() || (m_colorData.simpleColorAndFlags & validSimpleColorBit); }

    bool isOpaque() const { return isExtended() ? asExtended().alpha() == 1.0 : asSimple().isOpaque(); }
    bool isVisible() const { return isExtended() ? asExtended().alpha() > 0.0 : asSimple().isVisible(); }

    uint8_t alpha() const { return isExtended() ? convertToComponentByte(asExtended().alpha()) : asSimple().alphaComponent(); }
    float alphaAsFloat() const { return isExtended() ? asExtended().alpha() : asSimple().alphaComponentAsFloat(); }

    unsigned hash() const;

    WEBCORE_EXPORT std::pair<ColorSpace, ColorComponents<float>> colorSpaceAndComponents() const;

    // This will convert non-sRGB colorspace colors into sRGB.
    WEBCORE_EXPORT SimpleColor toSRGBASimpleColorLossy() const;

    // This will convert non-sRGB colorspace colors into sRGB.
    WEBCORE_EXPORT SRGBA<float> toSRGBALossy() const;

    WEBCORE_EXPORT Color lightened() const;
    WEBCORE_EXPORT Color darkened() const;

    WEBCORE_EXPORT float luminance() const;

    // FIXME: Replace remaining uses with luminance.
    WEBCORE_EXPORT float lightness() const;

    Color invertedColorWithAlpha(Optional<float> alpha) const;
    Color invertedColorWithAlpha(float alpha) const;

    Color colorWithAlphaMultipliedBy(Optional<float>) const;
    Color colorWithAlphaMultipliedBy(float) const;

    Color colorWithAlpha(Optional<float>) const;
    WEBCORE_EXPORT Color colorWithAlpha(float) const;

    Color opaqueColor() const { return colorWithAlpha(1.0f); }

    Color semanticColor() const;

    // True if the color originated from a CSS semantic color name.
    bool isSemantic() const { return isSimple() && (m_colorData.simpleColorAndFlags & isSemanticSimpleColorBit); }

#if PLATFORM(GTK)
    Color(const GdkRGBA&);
    operator GdkRGBA() const;
#endif

#if USE(CG)
    WEBCORE_EXPORT Color(CGColorRef);
    WEBCORE_EXPORT Color(CGColorRef, SemanticTag);
#endif

#if PLATFORM(WIN)
    WEBCORE_EXPORT Color(D2D1_COLOR_F);
    WEBCORE_EXPORT operator D2D1_COLOR_F() const;
    WEBCORE_EXPORT operator D2D1_VECTOR_4F() const;
#endif

    static constexpr SimpleColor black { 0xFF000000 };
    static constexpr SimpleColor white { 0xFFFFFFFF };
    static constexpr SimpleColor darkGray { 0xFF808080 };
    static constexpr SimpleColor gray { 0xFFA0A0A0 };
    static constexpr SimpleColor lightGray { 0xFFC0C0C0 };
    static constexpr SimpleColor transparent { 0x00000000 };
    static constexpr SimpleColor cyan { 0xFF00FFFF };
    static constexpr SimpleColor yellow { 0xFFFFFF00 };

    bool isExtended() const { return !(m_colorData.simpleColorAndFlags & invalidSimpleColor); }
    bool isSimple() const { return !isExtended(); }

    const ExtendedColor& asExtended() const;
    const SimpleColor asSimple() const;

    WEBCORE_EXPORT Color& operator=(const Color&);
    WEBCORE_EXPORT Color& operator=(Color&&);

    // Extended and non-extended colors will always be non-equal.
    friend bool operator==(const Color& a, const Color& b);
    friend bool equalIgnoringSemanticColor(const Color& a, const Color& b);
    friend bool extendedColorsEqual(const Color&, const Color&);

    static bool isBlackColor(const Color&);
    static bool isWhiteColor(const Color&);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Color> decode(Decoder&);

private:
    void setSimpleColor(SimpleColor);

    void tagAsSemantic() { m_colorData.simpleColorAndFlags |= isSemanticSimpleColorBit; }
    void tagAsValid() { m_colorData.simpleColorAndFlags |= validSimpleColor; }

    // 0x_______00 is an ExtendedColor pointer.
    // 0x_______01 is an invalid SimpleColor.
    // 0x_______11 is a valid SimpleColor.
    static const uint64_t extendedColor = 0x0;
    static const uint64_t invalidSimpleColor = 0x1;
    static const uint64_t validSimpleColorBit = 0x2;
    static const uint64_t validSimpleColor = 0x3;
    static const uint64_t isSemanticSimpleColorBit = 0x4;

    static const uint64_t deletedHashValue = 0xFFFFFFFFFFFFFFFD;
    static const uint64_t emptyHashValue = 0xFFFFFFFFFFFFFFFB;

    union {
        uint64_t simpleColorAndFlags { invalidSimpleColor };
        ExtendedColor* extendedColor;
    } m_colorData;
};

bool operator==(const Color&, const Color&);
bool operator!=(const Color&, const Color&);

// One or both must be extended colors.
bool extendedColorsEqual(const Color&, const Color&);

#if USE(CG)
WEBCORE_EXPORT CGColorRef cachedCGColor(const Color&);
#endif

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const Color&);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, ColorSpace);

inline bool operator==(const Color& a, const Color& b)
{
    if (a.isExtended() || b.isExtended())
        return extendedColorsEqual(a, b);

    return a.m_colorData.simpleColorAndFlags == b.m_colorData.simpleColorAndFlags;
}

inline bool operator!=(const Color& a, const Color& b)
{
    return !(a == b);
}

inline bool extendedColorsEqual(const Color& a, const Color& b)
{
    if (a.isExtended() && b.isExtended())
        return a.asExtended() == b.asExtended();

    ASSERT(a.isExtended() || b.isExtended());
    return false;
}

inline bool equalIgnoringSemanticColor(const Color& a, const Color& b)
{
    if (a.isExtended() || b.isExtended())
        return extendedColorsEqual(a, b);
    return (a.m_colorData.simpleColorAndFlags & ~Color::isSemanticSimpleColorBit) == (b.m_colorData.simpleColorAndFlags & ~Color::isSemanticSimpleColorBit);
}

inline unsigned Color::hash() const
{
    if (isExtended())
        return asExtended().hash();
    return WTF::intHash(m_colorData.simpleColorAndFlags);
}

inline Color Color::invertedColorWithAlpha(Optional<float> alpha) const
{
    return alpha ? invertedColorWithAlpha(alpha.value()) : *this;
}

inline Color Color::colorWithAlphaMultipliedBy(float amount) const
{
    return colorWithAlpha(amount * alphaAsFloat());
}

inline Color Color::colorWithAlphaMultipliedBy(Optional<float> alpha) const
{
    return alpha ? colorWithAlphaMultipliedBy(alpha.value()) : *this;
}

inline Color Color::colorWithAlpha(Optional<float> alpha) const
{
    return alpha ? colorWithAlpha(alpha.value()) : *this;
}

inline const ExtendedColor& Color::asExtended() const
{
    ASSERT(isExtended());
    return *m_colorData.extendedColor;
}

inline const SimpleColor Color::asSimple() const
{
    ASSERT(isSimple());
    return { static_cast<uint32_t>(m_colorData.simpleColorAndFlags >> 32) };
}

inline void Color::setSimpleColor(SimpleColor simpleColor)
{
    m_colorData.simpleColorAndFlags = static_cast<uint64_t>(simpleColor.value()) << 32;
    tagAsValid();
}

inline bool Color::isBlackColor(const Color& color)
{
    if (color.isExtended())
        return color.asExtended().isBlack();
    return color.asSimple() == Color::black;
}

inline bool Color::isWhiteColor(const Color& color)
{
    if (color.isExtended())
        return color.asExtended().isWhite();
    return color.asSimple() == Color::white;
}

template<class Encoder>
void Color::encode(Encoder& encoder) const
{
    if (isExtended()) {
        encoder << true;

        auto& extendedColor = asExtended();
        auto [c1, c2, c3, alpha] = extendedColor.components();
        encoder << c1;
        encoder << c2;
        encoder << c3;
        encoder << alpha;
        encoder << extendedColor.colorSpace();
        return;
    }

    encoder << false;

    if (!isValid()) {
        encoder << false;
        return;
    }

    // FIXME: This should encode whether the color is semantic.

    encoder << true;
    encoder << asSimple().value();
}

template<class Decoder>
Optional<Color> Color::decode(Decoder& decoder)
{
    bool isExtended;
    if (!decoder.decode(isExtended))
        return WTF::nullopt;

    if (isExtended) {
        float c1;
        float c2;
        float c3;
        float alpha;
        ColorSpace colorSpace;
        if (!decoder.decode(c1))
            return WTF::nullopt;
        if (!decoder.decode(c2))
            return WTF::nullopt;
        if (!decoder.decode(c3))
            return WTF::nullopt;
        if (!decoder.decode(alpha))
            return WTF::nullopt;
        if (!decoder.decode(colorSpace))
            return WTF::nullopt;
        return Color { ExtendedColor::create(c1, c2, c3, alpha, colorSpace) };
    }

    bool isValid;
    if (!decoder.decode(isValid))
        return WTF::nullopt;

    if (!isValid)
        return Color { };

    uint32_t value;
    if (!decoder.decode(value))
        return WTF::nullopt;

    return Color { SimpleColor { value } };
}

} // namespace WebCore

namespace WTF {
template<> struct DefaultHash<WebCore::Color>;
template<> struct HashTraits<WebCore::Color>;
}
