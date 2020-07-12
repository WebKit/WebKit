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
#include <wtf/Hasher.h>
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

// Able to represent:
//    - Special "invalid color" state, treated as transparent black but distinguishable
//    - 4x 8-bit (0-255) sRGBA, stored inline, no allocation
//    - 4x float (0-1) sRGBA, stored in a reference counted sub-object
//    - 4x float (0-1) Linear sRGBA, stored in a reference counted sub-object
//    - 4x float (0-1) DisplayP3, stored in a reference counted sub-object
//
// Additionally, the inline 8-bit sRGBA can have an optional "semantic" bit set on it,
// which indicates the color originated from a CSS semantic color name.
// FIXME: If we keep the "semantic" bit on Color, we should extend support to all colors,
// not just inline ones, by using the unused pointer bits.

class Color {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Color() = default;

    Color(SRGBA<uint8_t>);
    Color(Optional<SRGBA<uint8_t>>);

    enum SemanticTag { Semantic };
    Color(SRGBA<uint8_t>, SemanticTag);
    Color(Optional<SRGBA<uint8_t>>, SemanticTag);

    Color(ColorComponents<float>, ColorSpace);
    Color(const SRGBA<float>&);
    Color(const LinearSRGBA<float>&);
    Color(const DisplayP3<float>&);

    explicit Color(WTF::HashTableEmptyValueType);
    explicit Color(WTF::HashTableDeletedValueType);
    bool isHashTableDeletedValue() const { return m_colorData.inlineColorAndFlags == deletedHashValue; }

    WEBCORE_EXPORT Color(const Color&);
    WEBCORE_EXPORT Color(Color&&);

    WEBCORE_EXPORT Color& operator=(const Color&);
    WEBCORE_EXPORT Color& operator=(Color&&);

    ~Color();

    unsigned hash() const;

    bool isValid() const { return isExtended() || (m_colorData.inlineColorAndFlags & validInlineColorBit); }
    bool isSemantic() const { return isInline() && (m_colorData.inlineColorAndFlags & isSemanticInlineColorBit); }

    bool isOpaque() const { return isExtended() ? asExtended().alpha() == 1.0 : asInline().alpha == 255; }
    bool isVisible() const { return isExtended() ? asExtended().alpha() > 0.0 : asInline().alpha > 0; }
    uint8_t alpha() const { return isExtended() ? convertToComponentByte(asExtended().alpha()) : asInline().alpha; }
    float alphaAsFloat() const { return isExtended() ? asExtended().alpha() : convertToComponentFloat(asInline().alpha); }

    WEBCORE_EXPORT float luminance() const;
    WEBCORE_EXPORT float lightness() const; // FIXME: Replace remaining uses with luminance.

    template<typename Functor> decltype(auto) callOnUnderlyingType(Functor&&) const;

    // This will convert non-sRGB colorspace colors into sRGB.
    template<typename T> SRGBA<T> toSRGBALossy() const;

    WEBCORE_EXPORT std::pair<ColorSpace, ColorComponents<float>> colorSpaceAndComponents() const;

    WEBCORE_EXPORT Color lightened() const;
    WEBCORE_EXPORT Color darkened() const;

    Color invertedColorWithAlpha(Optional<float> alpha) const;
    Color invertedColorWithAlpha(float alpha) const;

    Color colorWithAlphaMultipliedBy(Optional<float>) const;
    Color colorWithAlphaMultipliedBy(float) const;

    Color colorWithAlpha(Optional<float>) const;
    WEBCORE_EXPORT Color colorWithAlpha(float) const;

    Color opaqueColor() const { return colorWithAlpha(1.0f); }

    Color semanticColor() const;

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

    static constexpr auto black = makeSimpleColor(0, 0, 0);
    static constexpr auto white = makeSimpleColor(255, 255, 255);
    static constexpr auto darkGray = makeSimpleColor(128, 128, 128);
    static constexpr auto gray = makeSimpleColor(160, 160, 160);
    static constexpr auto lightGray = makeSimpleColor(192, 192, 192);
    static constexpr auto transparent = makeSimpleColor(0, 0, 0, 0);
    static constexpr auto cyan = makeSimpleColor(0, 255, 255);
    static constexpr auto yellow = makeSimpleColor(255, 255, 0);

    bool isExtended() const { return !(m_colorData.inlineColorAndFlags & invalidInlineColor); }
    bool isInline() const { return !isExtended(); }

    const ExtendedColor& asExtended() const;
    SRGBA<uint8_t> asInline() const;

    // Extended and non-extended colors will always be non-equal.
    friend bool operator==(const Color& a, const Color& b);
    friend bool equalIgnoringSemanticColor(const Color& a, const Color& b);
    friend bool extendedColorsEqual(const Color&, const Color&);

    static bool isBlackColor(const Color&);
    static bool isWhiteColor(const Color&);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Color> decode(Decoder&);

private:
    Color(Ref<ExtendedColor>&&);

    void setColor(SRGBA<uint8_t>);
    void setColor(const SRGBA<float>&);
    void setColor(const LinearSRGBA<float>&);
    void setColor(const DisplayP3<float>&);
    void setExtendedColor(Ref<ExtendedColor>&&);

    void tagAsSemantic() { m_colorData.inlineColorAndFlags |= isSemanticInlineColorBit; }
    void tagAsValid() { m_colorData.inlineColorAndFlags |= validInlineColor; }

    // 0x_______00 is an ExtendedColor pointer.
    // 0x_______01 is an invalid inline color.
    // 0x_______11 is a valid inline color.
    static const uint64_t extendedColor = 0x0;
    static const uint64_t invalidInlineColor = 0x1;
    static const uint64_t validInlineColorBit = 0x2;
    static const uint64_t validInlineColor = 0x3;
    static const uint64_t isSemanticInlineColorBit = 0x4;

    static const uint64_t deletedHashValue = 0xFFFFFFFFFFFFFFFD;
    static const uint64_t emptyHashValue = 0xFFFFFFFFFFFFFFFB;

    union {
        uint64_t inlineColorAndFlags { invalidInlineColor };
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

    return a.m_colorData.inlineColorAndFlags == b.m_colorData.inlineColorAndFlags;
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
    return (a.m_colorData.inlineColorAndFlags & ~Color::isSemanticInlineColorBit) == (b.m_colorData.inlineColorAndFlags & ~Color::isSemanticInlineColorBit);
}

inline Color::Color(SRGBA<uint8_t> color)
{
    setColor(color);
}

inline Color::Color(Optional<SRGBA<uint8_t>> color)
{
    if (color)
        setColor(*color);
}

inline Color::Color(SRGBA<uint8_t> color, SemanticTag)
{
    setColor(color);
    tagAsSemantic();
}

inline Color::Color(Optional<SRGBA<uint8_t>> color, SemanticTag)
{
    if (color) {
        setColor(*color);
        tagAsSemantic();
    }
}

inline Color::Color(ColorComponents<float> components, ColorSpace colorSpace)
{
    switch (colorSpace) {
    case ColorSpace::SRGB:
        setColor(asSRGBA(components));
        return;
    case ColorSpace::LinearRGB:
        setColor(asLinearSRGBA(components));
        return;
    case ColorSpace::DisplayP3:
        setColor(asDisplayP3(components));
        return;
    }
}

inline Color::Color(const SRGBA<float>& color)
{
    setColor(color);
}

inline Color::Color(const LinearSRGBA<float>& color)
{
    setColor(color);
}

inline Color::Color(const DisplayP3<float>& color)
{
    setColor(color);
}

inline Color::Color(Ref<ExtendedColor>&& extendedColor)
{
    setExtendedColor(WTFMove(extendedColor));
}

inline Color::Color(WTF::HashTableDeletedValueType)
{
    static_assert(deletedHashValue & invalidInlineColor, "Color's deleted hash value must not look like an ExtendedColor");
    static_assert(!(deletedHashValue & validInlineColorBit), "Color's deleted hash value must not look like a valid InlineColor");
    static_assert(deletedHashValue & (1 << 4), "Color's deleted hash value must have some bits set that an InlineColor wouldn't have");
    m_colorData.inlineColorAndFlags = deletedHashValue;
    ASSERT(!isExtended());
}

inline Color::Color(WTF::HashTableEmptyValueType)
{
    static_assert(emptyHashValue & invalidInlineColor, "Color's empty hash value must not look like an ExtendedColor");
    static_assert(emptyHashValue & (1 << 4), "Color's deleted hash value must have some bits set that an InlineColor wouldn't have");
    m_colorData.inlineColorAndFlags = emptyHashValue;
    ASSERT(!isExtended());
}

inline Color::~Color()
{
    if (isExtended())
        m_colorData.extendedColor->deref();
}

inline unsigned Color::hash() const
{
    if (isExtended())
        return computeHash(asExtended().components(), asExtended().colorSpace());
    return WTF::intHash(m_colorData.inlineColorAndFlags);
}

template<typename Functor> decltype(auto) Color::callOnUnderlyingType(Functor&& functor) const
{
    if (isExtended())
        return asExtended().callOnUnderlyingType(std::forward<Functor>(functor));
    return std::invoke(std::forward<Functor>(functor), asInline());
}

template<typename T> SRGBA<T> Color::toSRGBALossy() const
{
    return callOnUnderlyingType(WTF::makeVisitor(
        [] (const SRGBA<uint8_t>& color) {
            if constexpr (std::is_same_v<T, uint8_t>)
                return color;
            if constexpr (std::is_same_v<T, float>)
                return convertToComponentFloats(color);
        },
        [] (const auto& color) {
            if constexpr (std::is_same_v<T, uint8_t>)
                return convertToComponentBytes(toSRGBA(color));
            if constexpr (std::is_same_v<T, float>)
                return toSRGBA(color);
        }
    ));
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

inline SRGBA<uint8_t> Color::asInline() const
{
    ASSERT(isInline());
    return asSRGBA(Packed::RGBA { static_cast<uint32_t>(m_colorData.inlineColorAndFlags >> 32) });
}

inline void Color::setColor(SRGBA<uint8_t> color)
{
    m_colorData.inlineColorAndFlags = static_cast<uint64_t>(Packed::RGBA { color }.value) << 32;
    tagAsValid();
}

inline void Color::setColor(const SRGBA<float>& color)
{
    setExtendedColor(ExtendedColor::create(color));
}

inline void Color::setColor(const LinearSRGBA<float>& color)
{
    setExtendedColor(ExtendedColor::create(color));
}

inline void Color::setColor(const DisplayP3<float>& color)
{
    setExtendedColor(ExtendedColor::create(color));
}

inline void Color::setExtendedColor(Ref<ExtendedColor>&& extendedColor)
{
    // Zero the union, just in case a 32-bit system only assigns the
    // top 32 bits when copying the ExtendedColor pointer below.
    m_colorData.inlineColorAndFlags = 0;
    m_colorData.extendedColor = &extendedColor.leakRef();
    ASSERT(isExtended());
}

inline bool Color::isBlackColor(const Color& color)
{
    return color.callOnUnderlyingType([] (const auto& color) {
        return WebCore::isBlack(color);
    });
}

inline bool Color::isWhiteColor(const Color& color)
{
    return color.callOnUnderlyingType([] (const auto& color) {
        return WebCore::isWhite(color);
    });
}

template<class Encoder> void Color::encode(Encoder& encoder) const
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
    encoder << Packed::RGBA { asInline() }.value;
}

template<class Decoder> Optional<Color> Color::decode(Decoder& decoder)
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
        return Color { ExtendedColor::create({ c1, c2, c3, alpha }, colorSpace) };
    }

    bool isValid;
    if (!decoder.decode(isValid))
        return WTF::nullopt;

    if (!isValid)
        return Color { };

    uint32_t value;
    if (!decoder.decode(value))
        return WTF::nullopt;

    return Color { asSRGBA(Packed::RGBA { value }) };
}

} // namespace WebCore

namespace WTF {
template<> struct DefaultHash<WebCore::Color>;
template<> struct HashTraits<WebCore::Color>;
}
