/*
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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

#include "ColorConversion.h"
#include "ColorSpace.h"
#include "ColorUtilities.h"
#include "DestinationColorSpace.h"
#include <functional>
#include <wtf/Forward.h>
#include <wtf/HashFunctions.h>
#include <wtf/Hasher.h>
#include <wtf/OptionSet.h>
#include <wtf/Ref.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ThreadSafeRefCounted.h>

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
//    - 4x float color components + color space, stored in a reference counted sub-object

class Color {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Flags {
        Semantic                        = 1 << 0,
        UseColorFunctionSerialization   = 1 << 1,
    };

    Color() = default;

    Color(SRGBA<uint8_t>, OptionSet<Flags> = { });
    Color(std::optional<SRGBA<uint8_t>>, OptionSet<Flags> = { });
    
    template<typename ColorType, typename std::enable_if_t<IsColorTypeWithComponentType<ColorType, float>>* = nullptr>
    Color(const ColorType&, OptionSet<Flags> = { });
    
    template<typename ColorType, typename std::enable_if_t<IsColorTypeWithComponentType<ColorType, float>>* = nullptr>
    Color(const std::optional<ColorType>&, OptionSet<Flags> = { });

    explicit Color(WTF::HashTableEmptyValueType);
    explicit Color(WTF::HashTableDeletedValueType);
    bool isHashTableDeletedValue() const;

    WEBCORE_EXPORT Color(const Color&);
    WEBCORE_EXPORT Color(Color&&);

    WEBCORE_EXPORT Color& operator=(const Color&);
    WEBCORE_EXPORT Color& operator=(Color&&);

    ~Color();

    unsigned hash() const;

    bool isValid() const;
    bool isSemantic() const;
    bool usesColorFunctionSerialization() const;

    ColorSpace colorSpace() const;

    bool isOpaque() const { return isOutOfLine() ? asOutOfLine().alpha() == 1.0 : asInline().alpha == 255; }
    bool isVisible() const { return isOutOfLine() ? asOutOfLine().alpha() > 0.0 : asInline().alpha > 0; }
    uint8_t alphaByte() const { return isOutOfLine() ? convertFloatAlphaTo<uint8_t>(asOutOfLine().alpha()) : asInline().alpha; }
    float alphaAsFloat() const { return isOutOfLine() ? asOutOfLine().alpha() : convertByteAlphaTo<float>(asInline().alpha); }

    WEBCORE_EXPORT double luminance() const;
    WEBCORE_EXPORT double lightness() const; // FIXME: Replace remaining uses with luminance.

    template<typename Functor> decltype(auto) callOnUnderlyingType(Functor&&) const;

    // This will convert the underlying color into ColorType, potentially lossily if the gamut
    // or precision of ColorType is smaller than the current underlying type.
    template<typename ColorType> ColorType toColorTypeLossy() const;

    // This will convert the underlying color into sRGB, potentially lossily if the gamut
    // or precision of sRGB is smaller than the current underlying type. This is a convenience
    // wrapper around toColorTypeLossy<>().
    template<typename T> SRGBA<T> toSRGBALossy() const { return toColorTypeLossy<SRGBA<T>>(); }

    ColorComponents<float, 4> toColorComponentsInColorSpace(ColorSpace) const;
    ColorComponents<float, 4> toColorComponentsInColorSpace(const DestinationColorSpace&) const;

    WEBCORE_EXPORT std::pair<ColorSpace, ColorComponents<float, 4>> colorSpaceAndComponents() const;

    WEBCORE_EXPORT Color lightened() const;
    WEBCORE_EXPORT Color darkened() const;

    Color invertedColorWithAlpha(std::optional<float> alpha) const;
    Color invertedColorWithAlpha(float alpha) const;

    Color colorWithAlphaMultipliedBy(std::optional<float>) const;
    Color colorWithAlphaMultipliedBy(float) const;

    Color colorWithAlpha(std::optional<float>) const;
    WEBCORE_EXPORT Color colorWithAlpha(float) const;

    Color opaqueColor() const { return colorWithAlpha(1.0f); }

    Color semanticColor() const;

    // Returns the underlying color if its type is SRGBA<uint8_t>.
    std::optional<SRGBA<uint8_t>> tryGetAsSRGBABytes() const;

#if PLATFORM(GTK)
    Color(const GdkRGBA&);
    operator GdkRGBA() const;
#endif

#if USE(CG)
    WEBCORE_EXPORT static Color createAndPreserveColorSpace(CGColorRef, OptionSet<Flags> = { });
#endif

#if PLATFORM(WIN)
    WEBCORE_EXPORT Color(D2D1_COLOR_F);
    WEBCORE_EXPORT operator D2D1_COLOR_F() const;
    WEBCORE_EXPORT operator D2D1_VECTOR_4F() const;
#endif

    static constexpr auto transparentBlack = SRGBA<uint8_t> { };
    static constexpr auto black = SRGBA<uint8_t> { 0, 0, 0 };
    static constexpr auto white = SRGBA<uint8_t> { 255, 255, 255 };
    static constexpr auto darkGray = SRGBA<uint8_t> { 128, 128, 128 };
    static constexpr auto gray = SRGBA<uint8_t> { 160, 160, 160 };
    static constexpr auto lightGray = SRGBA<uint8_t> { 192, 192, 192 };
    static constexpr auto cyan = SRGBA<uint8_t> { 0, 255, 255 };
    static constexpr auto yellow = SRGBA<uint8_t> { 255, 255, 0 };
    static constexpr auto red = SRGBA<uint8_t> { 255, 0, 0 };
    static constexpr auto magenta = SRGBA<uint8_t> { 255, 0, 255 };
    static constexpr auto blue = SRGBA<uint8_t> { 0, 0, 255 };
    static constexpr auto green = SRGBA<uint8_t> { 0, 255, 0 };
    static constexpr auto darkGreen = SRGBA<uint8_t> { 0, 128, 0 };
    static constexpr auto orange = SRGBA<uint8_t> { 255, 128, 0 };

    static bool isBlackColor(const Color&);
    static bool isWhiteColor(const Color&);

    // Out of line and inline colors will always be non-equal.
    friend bool operator==(const Color& a, const Color& b);
    friend bool equalIgnoringSemanticColor(const Color& a, const Color& b);
    friend bool outOfLineComponentssEqual(const Color&, const Color&);
    friend bool outOfLineComponentssEqualIgnoringSemanticColor(const Color&, const Color&);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<Color> decode(Decoder&);

private:
    class OutOfLineComponents : public ThreadSafeRefCounted<OutOfLineComponents> {
    public:
        static Ref<OutOfLineComponents> create(ColorComponents<float, 4> components)
        {
            return adoptRef(*new OutOfLineComponents(components));
        }

        float alpha() const { return m_components[3]; }
        ColorComponents<float, 4> components() const { return m_components; }

    private:
        OutOfLineComponents(ColorComponents<float, 4> components)
            : m_components(components)
        {
        }

        ColorComponents<float, 4> m_components;
    };
    Color(Ref<OutOfLineComponents>&&, ColorSpace, OptionSet<Flags> = { });

#if USE(CG)
    WEBCORE_EXPORT static Color createAndLosslesslyConvertToSupportedColorSpace(CGColorRef, OptionSet<Flags> = { });
#endif

    enum class FlagsIncludingPrivate : uint8_t {
        Semantic                        = static_cast<uint8_t>(Flags::Semantic),
        UseColorFunctionSerialization   = static_cast<uint8_t>(Flags::UseColorFunctionSerialization),
        Valid                           = 1 << 2,
        OutOfLine                       = 1 << 3,
        HashTableEmptyValue             = 1 << 4,
        HashTableDeletedValue           = 1 << 5,
    };
    static OptionSet<FlagsIncludingPrivate> toFlagsIncludingPrivate(OptionSet<Flags> flags) { return OptionSet<FlagsIncludingPrivate>::fromRaw(flags.toRaw()); }

    OptionSet<FlagsIncludingPrivate> flags() const;
    bool isOutOfLine() const;
    bool isInline() const;

    void setColor(SRGBA<uint8_t>, OptionSet<FlagsIncludingPrivate> = { });
    void setOutOfLineComponents(Ref<OutOfLineComponents>&&, ColorSpace, OptionSet<FlagsIncludingPrivate> = { });

    SRGBA<uint8_t> asInline() const;
    PackedColor::RGBA asPackedInline() const;

    const OutOfLineComponents& asOutOfLine() const;
    Ref<OutOfLineComponents> asOutOfLineRef() const;

#if CPU(ADDRESS64)
    static constexpr unsigned maxNumberOfBitsInPointer = 48;
#else
    static constexpr unsigned maxNumberOfBitsInPointer = 32;
#endif
    static constexpr uint64_t colorValueMask = (1ULL << maxNumberOfBitsInPointer) - 1;
    static constexpr uint64_t flagsSize = sizeof(FlagsIncludingPrivate) * 8;
    static constexpr uint64_t flagsShift = maxNumberOfBitsInPointer;
    static constexpr uint64_t colorSpaceSize = sizeof(ColorSpace) * 8;
    static constexpr uint64_t colorSpaceShift = flagsShift + flagsSize;
    static_assert(flagsSize + colorSpaceSize + maxNumberOfBitsInPointer <= 64);

    static uint64_t encodedFlags(OptionSet<FlagsIncludingPrivate>);
    static uint64_t encodedColorSpace(ColorSpace);
    static uint64_t encodedInlineColor(SRGBA<uint8_t>);
    static uint64_t encodedPackedInlineColor(PackedColor::RGBA);
    static uint64_t encodedOutOfLineComponents(Ref<OutOfLineComponents>&&);

    static OptionSet<FlagsIncludingPrivate> decodedFlags(uint64_t);
    static ColorSpace decodedColorSpace(uint64_t);
    static SRGBA<uint8_t> decodedInlineColor(uint64_t);
    static PackedColor::RGBA decodedPackedInlineColor(uint64_t);
    static OutOfLineComponents& decodedOutOfLineComponents(uint64_t);

    static constexpr uint64_t invalidColorAndFlags = 0;
    uint64_t m_colorAndFlags { invalidColorAndFlags };
};

bool operator==(const Color&, const Color&);
bool operator!=(const Color&, const Color&);

// One or both must be out of line colors.
bool outOfLineComponentssEqual(const Color&, const Color&);
bool outOfLineComponentssEqualIgnoringSemanticColor(const Color&, const Color&);

#if USE(CG)
WEBCORE_EXPORT CGColorRef cachedCGColor(const Color&);
WEBCORE_EXPORT ColorComponents<float, 4> platformConvertColorComponents(ColorSpace, ColorComponents<float, 4>, const DestinationColorSpace&);
WEBCORE_EXPORT std::optional<SRGBA<uint8_t>> roundAndClampToSRGBALossy(CGColorRef);
#endif

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const Color&);

inline bool operator==(const Color& a, const Color& b)
{
    if (a.isOutOfLine() || b.isOutOfLine())
        return outOfLineComponentssEqual(a, b);
    return a.m_colorAndFlags == b.m_colorAndFlags;
}

inline bool operator!=(const Color& a, const Color& b)
{
    return !(a == b);
}

inline bool outOfLineComponentssEqual(const Color& a, const Color& b)
{
    if (a.isOutOfLine() && b.isOutOfLine())
        return a.asOutOfLine().components() == b.asOutOfLine().components() && a.colorSpace() == b.colorSpace() && a.flags() == b.flags();

    ASSERT(a.isOutOfLine() || b.isOutOfLine());
    return false;
}

inline bool outOfLineComponentssEqualIgnoringSemanticColor(const Color& a, const Color& b)
{
    if (a.isOutOfLine() && b.isOutOfLine()) {
        auto aFlags = a.flags() - Color::FlagsIncludingPrivate::Semantic;
        auto bFlags = b.flags() - Color::FlagsIncludingPrivate::Semantic;
        return a.asOutOfLine().components() == b.asOutOfLine().components() && a.colorSpace() == b.colorSpace() && aFlags == bFlags;
    }

    ASSERT(a.isOutOfLine() || b.isOutOfLine());
    return false;
}

inline bool equalIgnoringSemanticColor(const Color& a, const Color& b)
{
    if (a.isOutOfLine() || b.isOutOfLine())
        return outOfLineComponentssEqualIgnoringSemanticColor(a, b);

    auto aFlags = a.flags() - Color::FlagsIncludingPrivate::Semantic;
    auto bFlags = b.flags() - Color::FlagsIncludingPrivate::Semantic;
    return a.asPackedInline().value == b.asPackedInline().value && aFlags == bFlags;
}

inline Color::Color(SRGBA<uint8_t> color, OptionSet<Flags> flags)
{
    setColor(color, toFlagsIncludingPrivate(flags));
}

inline Color::Color(std::optional<SRGBA<uint8_t>> color, OptionSet<Flags> flags)
{
    if (color)
        setColor(*color, toFlagsIncludingPrivate(flags));
}

template<typename ColorType, typename std::enable_if_t<IsColorTypeWithComponentType<ColorType, float>>*>
inline Color::Color(const ColorType& color, OptionSet<Flags> flags)
{
    setOutOfLineComponents(OutOfLineComponents::create(asColorComponents(color)), ColorSpaceFor<ColorType>, toFlagsIncludingPrivate(flags));
}

template<typename ColorType, typename std::enable_if_t<IsColorTypeWithComponentType<ColorType, float>>*>
inline Color::Color(const std::optional<ColorType>& color, OptionSet<Flags> flags)
{
    if (color)
        setOutOfLineComponents(OutOfLineComponents::create(asColorComponents(*color)), ColorSpaceFor<ColorType>, toFlagsIncludingPrivate(flags));
}

inline Color::Color(Ref<OutOfLineComponents>&& outOfLineComponents, ColorSpace colorSpace, OptionSet<Flags> flags)
{
    setOutOfLineComponents(WTFMove(outOfLineComponents), colorSpace, toFlagsIncludingPrivate(flags));
}

inline Color::Color(WTF::HashTableEmptyValueType)
{
    m_colorAndFlags = encodedFlags({ FlagsIncludingPrivate::HashTableEmptyValue });
}

inline Color::Color(WTF::HashTableDeletedValueType)
{
    m_colorAndFlags = encodedFlags({ FlagsIncludingPrivate::HashTableDeletedValue });
}

inline bool Color::isHashTableDeletedValue() const
{
    return flags().contains(FlagsIncludingPrivate::HashTableDeletedValue);
}

inline Color::~Color()
{
    if (isOutOfLine())
        asOutOfLine().deref();
}

inline unsigned Color::hash() const
{
    if (isOutOfLine())
        return computeHash(asOutOfLine().components(), colorSpace(), flags().toRaw());
    return computeHash(asPackedInline().value, flags().toRaw());
}

inline bool Color::isValid() const
{
    return flags().contains(FlagsIncludingPrivate::Valid);
}

inline bool Color::isSemantic() const
{
    return flags().contains(FlagsIncludingPrivate::Semantic);
}

inline bool Color::usesColorFunctionSerialization() const
{
    return flags().contains(FlagsIncludingPrivate::UseColorFunctionSerialization);
}

inline ColorSpace Color::colorSpace() const
{
    return decodedColorSpace(m_colorAndFlags);
}

template<typename Functor> decltype(auto) Color::callOnUnderlyingType(Functor&& functor) const
{
    if (isOutOfLine())
        return callWithColorType(asOutOfLine().components(), colorSpace(), std::forward<Functor>(functor));
    return std::invoke(std::forward<Functor>(functor), asInline());
}

template<typename ColorType> ColorType Color::toColorTypeLossy() const
{
    return callOnUnderlyingType([] (const auto& underlyingColor) {
        return convertColor<ColorType>(underlyingColor);
    });
}

inline Color Color::invertedColorWithAlpha(std::optional<float> alpha) const
{
    return alpha ? invertedColorWithAlpha(alpha.value()) : *this;
}

inline Color Color::colorWithAlphaMultipliedBy(float amount) const
{
    return colorWithAlpha(amount * alphaAsFloat());
}

inline Color Color::colorWithAlphaMultipliedBy(std::optional<float> alpha) const
{
    return alpha ? colorWithAlphaMultipliedBy(alpha.value()) : *this;
}

inline Color Color::colorWithAlpha(std::optional<float> alpha) const
{
    return alpha ? colorWithAlpha(alpha.value()) : *this;
}

inline OptionSet<Color::FlagsIncludingPrivate> Color::flags() const
{
    return decodedFlags(m_colorAndFlags);
}

inline bool Color::isOutOfLine() const
{
    return flags().contains(FlagsIncludingPrivate::OutOfLine);
}

inline bool Color::isInline() const
{
    return !flags().contains(FlagsIncludingPrivate::OutOfLine);
}

inline const Color::OutOfLineComponents& Color::asOutOfLine() const
{
    ASSERT(isOutOfLine());
    return decodedOutOfLineComponents(m_colorAndFlags);
}

inline Ref<Color::OutOfLineComponents> Color::asOutOfLineRef() const
{
    ASSERT(isOutOfLine());
    return decodedOutOfLineComponents(m_colorAndFlags);
}

inline SRGBA<uint8_t> Color::asInline() const
{
    ASSERT(isInline());
    return asSRGBA(asPackedInline());
}

inline PackedColor::RGBA Color::asPackedInline() const
{
    ASSERT(isInline());
    return decodedPackedInlineColor(m_colorAndFlags);
}

inline std::optional<SRGBA<uint8_t>> Color::tryGetAsSRGBABytes() const
{
    if (isInline())
        return asInline();
    return std::nullopt;
}

inline uint64_t Color::encodedFlags(OptionSet<FlagsIncludingPrivate> flags)
{
    return static_cast<uint64_t>(flags.toRaw()) << flagsShift;
}

inline uint64_t Color::encodedColorSpace(ColorSpace colorSpace)
{
    return static_cast<uint64_t>(colorSpace) << colorSpaceShift;
}

inline uint64_t Color::encodedInlineColor(SRGBA<uint8_t> color)
{
    return encodedPackedInlineColor(PackedColor::RGBA { color });
}

inline uint64_t Color::encodedPackedInlineColor(PackedColor::RGBA color)
{
    return color.value;
}

inline uint64_t Color::encodedOutOfLineComponents(Ref<OutOfLineComponents>&& outOfLineComponents)
{
#if CPU(ADDRESS64)
    return bitwise_cast<uint64_t>(&outOfLineComponents.leakRef());
#else
    return bitwise_cast<uint32_t>(&outOfLineComponents.leakRef());
#endif
}

inline OptionSet<Color::FlagsIncludingPrivate> Color::decodedFlags(uint64_t value)
{
    return OptionSet<Color::FlagsIncludingPrivate>::fromRaw(static_cast<uint8_t>(value >> flagsShift));
}

inline ColorSpace Color::decodedColorSpace(uint64_t value)
{
    return static_cast<ColorSpace>(static_cast<uint8_t>(value >> colorSpaceShift));
}

inline SRGBA<uint8_t> Color::decodedInlineColor(uint64_t value)
{
    return asSRGBA(decodedPackedInlineColor(value));
}

inline PackedColor::RGBA Color::decodedPackedInlineColor(uint64_t value)
{
    return PackedColor::RGBA { static_cast<uint32_t>(value & colorValueMask) };
}

inline Color::OutOfLineComponents& Color::decodedOutOfLineComponents(uint64_t value)
{
#if CPU(ADDRESS64)
    return *bitwise_cast<OutOfLineComponents*>(value & colorValueMask);
#else
    return *bitwise_cast<OutOfLineComponents*>(static_cast<uint32_t>(value & colorValueMask));
#endif
}

inline void Color::setColor(SRGBA<uint8_t> color, OptionSet<FlagsIncludingPrivate> flags)
{
    flags.add({ FlagsIncludingPrivate::Valid });
    m_colorAndFlags = encodedInlineColor(color) | encodedColorSpace(ColorSpace::SRGB) | encodedFlags(flags);
    ASSERT(isInline());
}

inline void Color::setOutOfLineComponents(Ref<OutOfLineComponents>&& color, ColorSpace colorSpace, OptionSet<FlagsIncludingPrivate> flags)
{
    flags.add({ FlagsIncludingPrivate::Valid, FlagsIncludingPrivate::OutOfLine });
    m_colorAndFlags = encodedOutOfLineComponents(WTFMove(color)) | encodedColorSpace(colorSpace) | encodedFlags(flags);
    ASSERT(isOutOfLine());
}

template<class Encoder> void Color::encode(Encoder& encoder) const
{
    if (!isValid()) {
        encoder << false;
        return;
    }
    encoder << true;

    encoder << flags().contains(FlagsIncludingPrivate::Semantic);
    encoder << flags().contains(FlagsIncludingPrivate::UseColorFunctionSerialization);
    
    if (isOutOfLine()) {
        encoder << true;
        encoder << colorSpace();

        auto& outOfLineComponents = asOutOfLine();
        auto [c1, c2, c3, alpha] = outOfLineComponents.components();
        encoder << c1;
        encoder << c2;
        encoder << c3;
        encoder << alpha;
        return;
    }
    encoder << false;

    encoder << asPackedInline().value;
}

template<class Decoder> std::optional<Color> Color::decode(Decoder& decoder)
{
    bool isValid;
    if (!decoder.decode(isValid))
        return std::nullopt;

    if (!isValid)
        return Color { };

    OptionSet<Flags> flags;

    bool isSemantic;
    if (!decoder.decode(isSemantic))
        return std::nullopt;

    if (isSemantic)
        flags.add(Flags::Semantic);

    bool usesColorFunctionSerialization;
    if (!decoder.decode(usesColorFunctionSerialization))
        return std::nullopt;

    if (usesColorFunctionSerialization)
        flags.add(Flags::UseColorFunctionSerialization);

    bool isOutOfLine;
    if (!decoder.decode(isOutOfLine))
        return std::nullopt;

    if (isOutOfLine) {
        ColorSpace colorSpace;
        if (!decoder.decode(colorSpace))
            return std::nullopt;
        float c1;
        if (!decoder.decode(c1))
            return std::nullopt;
        float c2;
        if (!decoder.decode(c2))
            return std::nullopt;
        float c3;
        if (!decoder.decode(c3))
            return std::nullopt;
        float alpha;
        if (!decoder.decode(alpha))
            return std::nullopt;
        return Color { OutOfLineComponents::create({ c1, c2, c3, alpha }), colorSpace, flags };
    }

    uint32_t value;
    if (!decoder.decode(value))
        return std::nullopt;

    return Color { asSRGBA(PackedColor::RGBA { value }), flags };
}

} // namespace WebCore

namespace WTF {
template<> struct DefaultHash<WebCore::Color>;
template<> struct HashTraits<WebCore::Color>;
}
