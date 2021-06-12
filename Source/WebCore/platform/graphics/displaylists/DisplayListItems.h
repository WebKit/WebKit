/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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

#include "AlphaPremultiplication.h"
#include "DisplayListFlushIdentifier.h"
#include "DisplayListItemBufferIdentifier.h"
#include "DisplayListItemType.h"
#include "FloatRoundedRect.h"
#include "Font.h"
#include "GlyphBuffer.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "MediaPlayerIdentifier.h"
#include "Pattern.h"
#include "PixelBuffer.h"
#include "RenderingResourceIdentifier.h"
#include "SharedBuffer.h"
#include <wtf/EnumTraits.h>
#include <wtf/TypeCasts.h>
#include <wtf/Variant.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class MediaPlayer;
struct ImagePaintingOptions;

namespace DisplayList {

struct ItemHandle;

/* isInlineItem indicates whether the object needs to be passed through IPC::Encoder in order to serialize,
 * or whether we can just use placement new and be done.
 * It needs to match (1) RemoteImageBufferProxy::encodeItem(), (2) RemoteRenderingBackend::decodeItem(),
 * and (3) isInlineItem() in DisplayListItemType.cpp.
 *
 * isDrawingItem indicates if this command can affect dirty rects.
 * We can do things like skip drawing items when replaying them if their extents don't intersect with the current clip.
 * It needs to match isDrawingItem() in DisplayListItemType.cpp. */

class Save {
public:
    static constexpr ItemType itemType = ItemType::Save;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    void apply(GraphicsContext&) const;
};

class Restore {
public:
    static constexpr ItemType itemType = ItemType::Restore;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    void apply(GraphicsContext&) const;
};

class Translate {
public:
    static constexpr ItemType itemType = ItemType::Translate;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    Translate(float x, float y)
        : m_x(x)
        , m_y(y)
    {
    }

    float x() const { return m_x; }
    float y() const { return m_y; }

    void apply(GraphicsContext&) const;

private:
    float m_x { 0 };
    float m_y { 0 };
};

class Rotate {
public:
    static constexpr ItemType itemType = ItemType::Rotate;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    Rotate(float angle)
        : m_angle(angle)
    {
    }

    float angle() const { return m_angle; }

    void apply(GraphicsContext&) const;

private:
    float m_angle { 0 }; // In radians.
};

class Scale {
public:
    static constexpr ItemType itemType = ItemType::Scale;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    Scale(const FloatSize& size)
        : m_size(size)
    {
    }

    const FloatSize& amount() const { return m_size; }

    void apply(GraphicsContext&) const;

private:
    FloatSize m_size;
};

class SetCTM {
public:
    static constexpr ItemType itemType = ItemType::SetCTM;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    SetCTM(const AffineTransform& transform)
        : m_transform(transform)
    {
    }

    const AffineTransform& transform() const { return m_transform; }

    void apply(GraphicsContext&) const;

private:
    AffineTransform m_transform;
};

class ConcatenateCTM {
public:
    static constexpr ItemType itemType = ItemType::ConcatenateCTM;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    ConcatenateCTM(const AffineTransform& transform)
        : m_transform(transform)
    {
    }

    const AffineTransform& transform() const { return m_transform; }

    void apply(GraphicsContext&) const;

private:
    AffineTransform m_transform;
};

class SetInlineFillGradient {
public:
    static constexpr ItemType itemType = ItemType::SetInlineFillGradient;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;
    static constexpr uint8_t maxColorStopCount = 4;

    SetInlineFillGradient(const Gradient&, const AffineTransform& gradientSpaceTransform);
    WEBCORE_EXPORT SetInlineFillGradient(float offsets[maxColorStopCount], SRGBA<uint8_t> colors[maxColorStopCount], const Gradient::Data&, const AffineTransform& gradientSpaceTransform, GradientSpreadMethod, uint8_t colorStopCount);
    SetInlineFillGradient(const SetInlineFillGradient&);
    bool isValid() const { return m_isValid; }

    static bool isInline(const Gradient&);
    Ref<Gradient> gradient() const;

    void apply(GraphicsContext&) const;

private:
    float m_offsets[maxColorStopCount];
    SRGBA<uint8_t> m_colors[maxColorStopCount];
    Gradient::Data m_data;
    AffineTransform m_gradientSpaceTransform;
    GradientSpreadMethod m_spreadMethod { GradientSpreadMethod::Pad };
    uint8_t m_colorStopCount { 0 };
    bool m_isValid { true };
};

class SetInlineFillColor {
public:
    static constexpr ItemType itemType = ItemType::SetInlineFillColor;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    SetInlineFillColor(SRGBA<uint8_t> colorData)
        : m_colorData(colorData)
    {
    }

    Color color() const { return { m_colorData }; }
    void apply(GraphicsContext&) const;

private:
    SRGBA<uint8_t> m_colorData;
};

class SetInlineStrokeColor {
public:
    static constexpr ItemType itemType = ItemType::SetInlineStrokeColor;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    SetInlineStrokeColor(SRGBA<uint8_t> colorData)
        : m_colorData(colorData)
    {
    }

    Color color() const { return { m_colorData }; }

    void apply(GraphicsContext&) const;

private:
    SRGBA<uint8_t> m_colorData;
};

class SetStrokeThickness {
public:
    static constexpr ItemType itemType = ItemType::SetStrokeThickness;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    SetStrokeThickness(float thickness)
        : m_thickness(thickness)
    {
    }

    float thickness() const { return m_thickness; }

    void apply(GraphicsContext&) const;

private:
    float m_thickness { 0 };
};

class SetState {
public:
    static constexpr ItemType itemType = ItemType::SetState;
    static constexpr bool isInlineItem = false;
    static constexpr bool isDrawingItem = false;

    WEBCORE_EXPORT SetState(const GraphicsContextState&, GraphicsContextState::StateChangeFlags);

    const GraphicsContextStateChange& stateChange() const { return m_stateChange; }
    const Pattern::Parameters& strokePatternParameters() const { return m_strokePattern.parameters; }
    const Pattern::Parameters& fillPatternParameters() const { return m_fillPattern.parameters; }
    RenderingResourceIdentifier strokePatternImageIdentifier() const { return m_strokePattern.tileImageIdentifier; }
    RenderingResourceIdentifier fillPatternImageIdentifier() const { return m_fillPattern.tileImageIdentifier; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<SetState> decode(Decoder&);

    void apply(GraphicsContext&, NativeImage* strokePatternImage, NativeImage* fillPatternImage);

private:
    struct PatternData {
        RenderingResourceIdentifier tileImageIdentifier;
        Pattern::Parameters parameters;
    };

    WEBCORE_EXPORT SetState(const GraphicsContextStateChange&, const PatternData& strokePattern, const PatternData& fillPattern);

    GraphicsContextStateChange m_stateChange;
    PatternData m_strokePattern;
    PatternData m_fillPattern;
};

template<class Encoder>
void SetState::encode(Encoder& encoder) const
{
    auto changeFlags = m_stateChange.m_changeFlags;
    encoder << changeFlags;

    auto& state = m_stateChange.m_state;

    if (changeFlags.contains(GraphicsContextState::StrokeGradientChange)) {
        ASSERT(state.strokeGradient);
        encoder << *state.strokeGradient;
    }

    if (changeFlags.contains(GraphicsContextState::StrokePatternChange)) {
        ASSERT(state.strokePattern);
        encoder << state.strokePattern->tileImage().renderingResourceIdentifier();
        encoder << state.strokePattern->parameters();
    }

    if (changeFlags.contains(GraphicsContextState::FillGradientChange)) {
        ASSERT(state.fillGradient);
        encoder << *state.fillGradient;
    }

    if (changeFlags.contains(GraphicsContextState::FillPatternChange)) {
        ASSERT(state.fillPattern);
        encoder << state.fillPattern->tileImage().renderingResourceIdentifier();
        encoder << state.fillPattern->parameters();
    }

    if (changeFlags.contains(GraphicsContextState::ShadowChange)) {
        encoder << state.shadowOffset;
        encoder << state.shadowBlur;
        encoder << state.shadowColor;
        encoder << state.shadowRadiusMode;
    }

    if (changeFlags.contains(GraphicsContextState::StrokeThicknessChange))
        encoder << state.strokeThickness;

    if (changeFlags.contains(GraphicsContextState::TextDrawingModeChange))
        encoder << state.textDrawingMode;

    if (changeFlags.contains(GraphicsContextState::StrokeColorChange))
        encoder << state.strokeColor;

    if (changeFlags.contains(GraphicsContextState::FillColorChange))
        encoder << state.fillColor;

    if (changeFlags.contains(GraphicsContextState::StrokeStyleChange))
        encoder << state.strokeStyle;

    if (changeFlags.contains(GraphicsContextState::FillRuleChange))
        encoder << state.fillRule;

    if (changeFlags.contains(GraphicsContextState::CompositeOperationChange))
        encoder << state.compositeOperator;

    if (changeFlags.contains(GraphicsContextState::BlendModeChange))
        encoder << state.blendMode;

    if (changeFlags.contains(GraphicsContextState::ImageInterpolationQualityChange))
        encoder << state.imageInterpolationQuality;

    if (changeFlags.contains(GraphicsContextState::AlphaChange))
        encoder << state.alpha;

    if (changeFlags.contains(GraphicsContextState::ShouldAntialiasChange))
        encoder << state.shouldAntialias;

    if (changeFlags.contains(GraphicsContextState::ShouldSmoothFontsChange))
        encoder << state.shouldSmoothFonts;

    if (changeFlags.contains(GraphicsContextState::ShouldSubpixelQuantizeFontsChange))
        encoder << state.shouldSubpixelQuantizeFonts;

    if (changeFlags.contains(GraphicsContextState::ShadowsIgnoreTransformsChange))
        encoder << state.shadowsIgnoreTransforms;
}

template<class Decoder>
std::optional<SetState> SetState::decode(Decoder& decoder)
{
    std::optional<GraphicsContextState::StateChangeFlags> changeFlags;
    decoder >> changeFlags;
    if (!changeFlags)
        return std::nullopt;

    GraphicsContextStateChange stateChange;
    stateChange.m_changeFlags = *changeFlags;

    PatternData strokePattern;
    PatternData fillPattern;

    if (stateChange.m_changeFlags.contains(GraphicsContextState::StrokeGradientChange)) {
        auto strokeGradient = Gradient::decode(decoder);
        if (!strokeGradient)
            return std::nullopt;

        stateChange.m_state.strokeGradient = WTFMove(*strokeGradient);
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::StrokePatternChange)) {
        std::optional<RenderingResourceIdentifier> renderingResourceIdentifier;
        decoder >> renderingResourceIdentifier;
        if (!renderingResourceIdentifier)
            return std::nullopt;

        std::optional<Pattern::Parameters> parameters;
        decoder >> parameters;
        if (!parameters)
            return std::nullopt;

        strokePattern = { *renderingResourceIdentifier, *parameters };
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::FillGradientChange)) {
        auto fillGradient = Gradient::decode(decoder);
        if (!fillGradient)
            return std::nullopt;

        stateChange.m_state.fillGradient = WTFMove(*fillGradient);
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::FillPatternChange)) {
        std::optional<RenderingResourceIdentifier> renderingResourceIdentifier;
        decoder >> renderingResourceIdentifier;
        if (!renderingResourceIdentifier)
            return std::nullopt;

        std::optional<Pattern::Parameters> parameters;
        decoder >> parameters;
        if (!parameters)
            return std::nullopt;

        fillPattern = { *renderingResourceIdentifier, *parameters };
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::ShadowChange)) {
        std::optional<FloatSize> shadowOffset;
        decoder >> shadowOffset;
        if (!shadowOffset)
            return std::nullopt;

        stateChange.m_state.shadowOffset = *shadowOffset;

        std::optional<float> shadowBlur;
        decoder >> shadowBlur;
        if (!shadowBlur)
            return std::nullopt;

        stateChange.m_state.shadowBlur = *shadowBlur;

        std::optional<Color> shadowColor;
        decoder >> shadowColor;
        if (!shadowColor)
            return std::nullopt;

        stateChange.m_state.shadowColor = *shadowColor;

        std::optional<ShadowRadiusMode> shadowRadiusMode;
        decoder >> shadowRadiusMode;
        if (!shadowRadiusMode)
            return std::nullopt;

        stateChange.m_state.shadowRadiusMode = WTFMove(*shadowRadiusMode);
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::StrokeThicknessChange)) {
        std::optional<float> strokeThickness;
        decoder >> strokeThickness;
        if (!strokeThickness)
            return std::nullopt;

        stateChange.m_state.strokeThickness = *strokeThickness;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::TextDrawingModeChange)) {
        std::optional<TextDrawingModeFlags> textDrawingMode;
        decoder >> textDrawingMode;
        if (!textDrawingMode)
            return std::nullopt;

        stateChange.m_state.textDrawingMode = WTFMove(*textDrawingMode);
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::StrokeColorChange)) {
        std::optional<Color> strokeColor;
        decoder >> strokeColor;
        if (!strokeColor)
            return std::nullopt;

        stateChange.m_state.strokeColor = *strokeColor;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::FillColorChange)) {
        std::optional<Color> fillColor;
        decoder >> fillColor;
        if (!fillColor)
            return std::nullopt;

        stateChange.m_state.fillColor = *fillColor;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::StrokeStyleChange)) {
        StrokeStyle strokeStyle;
        if (!decoder.decode(strokeStyle))
            return std::nullopt;

        stateChange.m_state.strokeStyle = strokeStyle;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::FillRuleChange)) {
        std::optional<WindRule> fillRule;
        decoder >> fillRule;
        if (!fillRule)
            return std::nullopt;

        stateChange.m_state.fillRule = *fillRule;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::CompositeOperationChange)) {
        std::optional<CompositeOperator> compositeOperator;
        decoder >> compositeOperator;
        if (!compositeOperator)
            return std::nullopt;

        stateChange.m_state.compositeOperator = *compositeOperator;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::BlendModeChange)) {
        std::optional<BlendMode> blendMode;
        decoder >> blendMode;
        if (!blendMode)
            return std::nullopt;

        stateChange.m_state.blendMode = *blendMode;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::ImageInterpolationQualityChange)) {
        std::optional<InterpolationQuality> imageInterpolationQuality;
        decoder >> imageInterpolationQuality;
        if (!imageInterpolationQuality)
            return std::nullopt;

        stateChange.m_state.imageInterpolationQuality = *imageInterpolationQuality;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::AlphaChange)) {
        std::optional<float> alpha;
        decoder >> alpha;
        if (!alpha)
            return std::nullopt;

        stateChange.m_state.alpha = *alpha;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::ShouldAntialiasChange)) {
        std::optional<bool> shouldAntialias;
        decoder >> shouldAntialias;
        if (!shouldAntialias)
            return std::nullopt;

        stateChange.m_state.shouldAntialias = *shouldAntialias;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::ShouldSmoothFontsChange)) {
        std::optional<bool> shouldSmoothFonts;
        decoder >> shouldSmoothFonts;
        if (!shouldSmoothFonts)
            return std::nullopt;

        stateChange.m_state.shouldSmoothFonts = *shouldSmoothFonts;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::ShouldSubpixelQuantizeFontsChange)) {
        std::optional<bool> shouldSubpixelQuantizeFonts;
        decoder >> shouldSubpixelQuantizeFonts;
        if (!shouldSubpixelQuantizeFonts)
            return std::nullopt;

        stateChange.m_state.shouldSubpixelQuantizeFonts = *shouldSubpixelQuantizeFonts;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::ShadowsIgnoreTransformsChange)) {
        std::optional<bool> shadowsIgnoreTransforms;
        decoder >> shadowsIgnoreTransforms;
        if (!shadowsIgnoreTransforms)
            return std::nullopt;

        stateChange.m_state.shadowsIgnoreTransforms = *shadowsIgnoreTransforms;
    }

    return {{ stateChange, strokePattern, fillPattern }};
}

class SetLineCap {
public:
    static constexpr ItemType itemType = ItemType::SetLineCap;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    SetLineCap(LineCap lineCap)
        : m_lineCap(lineCap)
    {
    }

    LineCap lineCap() const { return m_lineCap; }

    void apply(GraphicsContext&) const;

private:
    LineCap m_lineCap;
};

class SetLineDash {
public:
    static constexpr ItemType itemType = ItemType::SetLineDash;
    static constexpr bool isInlineItem = false;
    static constexpr bool isDrawingItem = false;

    SetLineDash(const DashArray& dashArray, float dashOffset)
        : m_dashArray(dashArray)
        , m_dashOffset(dashOffset)
    {
    }

    const DashArray& dashArray() const { return m_dashArray; }
    float dashOffset() const { return m_dashOffset; }

    void apply(GraphicsContext&) const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<SetLineDash> decode(Decoder&);

private:
    DashArray m_dashArray;
    float m_dashOffset;
};

template<class Encoder>
void SetLineDash::encode(Encoder& encoder) const
{
    encoder << m_dashArray;
    encoder << m_dashOffset;
}

template<class Decoder>
std::optional<SetLineDash> SetLineDash::decode(Decoder& decoder)
{
    std::optional<DashArray> dashArray;
    decoder >> dashArray;
    if (!dashArray)
        return std::nullopt;

    std::optional<float> dashOffset;
    decoder >> dashOffset;
    if (!dashOffset)
        return std::nullopt;

    return {{ *dashArray, *dashOffset }};
}

class SetLineJoin {
public:
    static constexpr ItemType itemType = ItemType::SetLineJoin;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    SetLineJoin(LineJoin lineJoin)
        : m_lineJoin(lineJoin)
    {
    }

    LineJoin lineJoin() const { return m_lineJoin; }

    void apply(GraphicsContext&) const;

private:
    LineJoin m_lineJoin;
};

class SetMiterLimit {
public:
    static constexpr ItemType itemType = ItemType::SetMiterLimit;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    SetMiterLimit(float miterLimit)
        : m_miterLimit(miterLimit)
    {
    }

    float miterLimit() const { return m_miterLimit; }

    void apply(GraphicsContext&) const;

private:
    float m_miterLimit;
};

class ClearShadow {
public:
    static constexpr ItemType itemType = ItemType::ClearShadow;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    void apply(GraphicsContext&) const;
};

// FIXME: treat as drawing item?
class Clip {
public:
    static constexpr ItemType itemType = ItemType::Clip;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    Clip(const FloatRect& rect)
        : m_rect(rect)
    {
    }

    const FloatRect& rect() const { return m_rect; }

    void apply(GraphicsContext&) const;

private:
    FloatRect m_rect;
};

class ClipOut {
public:
    static constexpr ItemType itemType = ItemType::ClipOut;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    ClipOut(const FloatRect& rect)
        : m_rect(rect)
    {
    }

    const FloatRect& rect() const { return m_rect; }

    void apply(GraphicsContext&) const;

private:
    FloatRect m_rect;
};

class ClipToImageBuffer {
public:
    static constexpr ItemType itemType = ItemType::ClipToImageBuffer;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    ClipToImageBuffer(RenderingResourceIdentifier imageBufferIdentifier, const FloatRect& destinationRect)
        : m_imageBufferIdentifier(imageBufferIdentifier)
        , m_destinationRect(destinationRect)
    {
    }
    
    RenderingResourceIdentifier imageBufferIdentifier() const { return m_imageBufferIdentifier; }
    FloatRect destinationRect() const { return m_destinationRect; }
    bool isValid() const { return m_imageBufferIdentifier.isValid(); }

    void apply(GraphicsContext&, WebCore::ImageBuffer&) const;

    NO_RETURN_DUE_TO_ASSERT void apply(GraphicsContext&) const;

private:
    RenderingResourceIdentifier m_imageBufferIdentifier;
    FloatRect m_destinationRect;
};

class ClipOutToPath {
public:
    static constexpr ItemType itemType = ItemType::ClipOutToPath;
    static constexpr bool isInlineItem = false;
    static constexpr bool isDrawingItem = false;

    ClipOutToPath(Path&& path)
        : m_path(WTFMove(path))
    {
    }

    ClipOutToPath(const Path& path)
        : m_path(path)
    {
    }

    const Path& path() const { return m_path; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ClipOutToPath> decode(Decoder&);

    void apply(GraphicsContext&) const;

private:
    Path m_path;
};

template<class Encoder>
void ClipOutToPath::encode(Encoder& encoder) const
{
    encoder << m_path;
}

template<class Decoder>
std::optional<ClipOutToPath> ClipOutToPath::decode(Decoder& decoder)
{
    std::optional<Path> path;
    decoder >> path;
    if (!path)
        return std::nullopt;

    return {{ WTFMove(*path) }};
}

class ClipPath {
public:
    static constexpr ItemType itemType = ItemType::ClipPath;
    static constexpr bool isInlineItem = false;
    static constexpr bool isDrawingItem = false;

    ClipPath(Path&& path, WindRule windRule)
        : m_path(WTFMove(path))
        , m_windRule(windRule)
    {
    }

    ClipPath(const Path& path, WindRule windRule)
        : m_path(path)
        , m_windRule(windRule)
    {
    }

    const Path& path() const { return m_path; }
    WindRule windRule() const { return m_windRule; }

    void apply(GraphicsContext&) const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ClipPath> decode(Decoder&);

private:
    Path m_path;
    WindRule m_windRule;
};

template<class Encoder>
void ClipPath::encode(Encoder& encoder) const
{
    encoder << m_path;
    encoder << m_windRule;
}

template<class Decoder>
std::optional<ClipPath> ClipPath::decode(Decoder& decoder)
{
    std::optional<Path> path;
    decoder >> path;
    if (!path)
        return std::nullopt;

    std::optional<WindRule> windRule;
    decoder >> windRule;
    if (!windRule)
        return std::nullopt;

    return {{ WTFMove(*path), *windRule }};
}

class BeginClipToDrawingCommands {
public:
    static constexpr ItemType itemType = ItemType::BeginClipToDrawingCommands;
    static constexpr bool isInlineItem = false;
    static constexpr bool isDrawingItem = false;

    BeginClipToDrawingCommands(const FloatRect& destination, DestinationColorSpace colorSpace)
        : m_destination(destination)
        , m_colorSpace(WTFMove(colorSpace))
    {
    }

    // Explicit destructor added to force non-trivial destructor on all platforms
    // as the encoding logic currently hardcodes which display list item types need
    // out of line treatment rather than using the isInlineItem constant.
    ~BeginClipToDrawingCommands() { }

    const FloatRect& destination() const { return m_destination; }
    const DestinationColorSpace& colorSpace() const { return m_colorSpace; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<BeginClipToDrawingCommands> decode(Decoder&);

private:
    FloatRect m_destination;
    DestinationColorSpace m_colorSpace;
};

template<class Encoder>
void BeginClipToDrawingCommands::encode(Encoder& encoder) const
{
    encoder << m_destination;
    encoder << m_colorSpace;
}

template<class Decoder>
std::optional<BeginClipToDrawingCommands> BeginClipToDrawingCommands::decode(Decoder& decoder)
{
    std::optional<FloatRect> destination;
    decoder >> destination;
    if (!destination)
        return std::nullopt;

    std::optional<DestinationColorSpace> colorSpace;
    decoder >> colorSpace;
    if (!colorSpace)
        return std::nullopt;

    return {{ *destination, WTFMove(*colorSpace) }};
}

class EndClipToDrawingCommands {
public:
    static constexpr ItemType itemType = ItemType::EndClipToDrawingCommands;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    EndClipToDrawingCommands(const FloatRect& destination)
        : m_destination(destination)
    {
    }

    const FloatRect& destination() const { return m_destination; }

private:
    FloatRect m_destination;
};

class DrawGlyphs {
public:
    static constexpr ItemType itemType = ItemType::DrawGlyphs;
    static constexpr bool isInlineItem = false;
    static constexpr bool isDrawingItem = true;

    RenderingResourceIdentifier fontIdentifier() { return m_fontIdentifier; }
    const FloatPoint& localAnchor() const { return m_localAnchor; }
    FloatPoint anchorPoint() const { return m_localAnchor; }
    const Vector<GlyphBufferGlyph, 16>& glyphs() const { return m_glyphs; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<DrawGlyphs> decode(Decoder&);

    DrawGlyphs(const Font&, const GlyphBufferGlyph*, const GlyphBufferAdvance*, unsigned count, const FloatPoint& localAnchor, FontSmoothingMode);
    WEBCORE_EXPORT DrawGlyphs(RenderingResourceIdentifier, Vector<GlyphBufferGlyph, 128>&&, Vector<GlyphBufferAdvance, 128>&&, const FloatRect&, const FloatPoint& localAnchor, FontSmoothingMode);

    void apply(GraphicsContext&, const Font&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return m_bounds; }

private:
    void computeBounds(const Font&);

    RenderingResourceIdentifier m_fontIdentifier;
    Vector<GlyphBufferGlyph, 16> m_glyphs;
    Vector<GlyphBufferAdvance, 16> m_advances;
    FloatRect m_bounds;
    FloatPoint m_localAnchor;
    FontSmoothingMode m_smoothingMode;
};

template<class Encoder>
void DrawGlyphs::encode(Encoder& encoder) const
{
    encoder << m_fontIdentifier;
    encoder << m_glyphs;
    encoder << m_advances;
    encoder << m_bounds;
    encoder << m_localAnchor;
    encoder << m_smoothingMode;
}

template<class Decoder>
std::optional<DrawGlyphs> DrawGlyphs::decode(Decoder& decoder)
{
    std::optional<RenderingResourceIdentifier> fontIdentifier;
    decoder >> fontIdentifier;
    if (!fontIdentifier)
        return std::nullopt;

    std::optional<Vector<GlyphBufferGlyph, 128>> glyphs;
    decoder >> glyphs;
    if (!glyphs)
        return std::nullopt;

    std::optional<Vector<GlyphBufferAdvance, 128>> advances;
    decoder >> advances;
    if (!advances)
        return std::nullopt;

    if (glyphs->size() != advances->size())
        return std::nullopt;

    std::optional<FloatRect> bounds;
    decoder >> bounds;
    if (!bounds)
        return std::nullopt;

    std::optional<FloatPoint> localAnchor;
    decoder >> localAnchor;
    if (!localAnchor)
        return std::nullopt;

    std::optional<FontSmoothingMode> smoothingMode;
    decoder >> smoothingMode;
    if (!smoothingMode)
        return std::nullopt;

    return {{ *fontIdentifier, WTFMove(*glyphs), WTFMove(*advances), *bounds, *localAnchor, *smoothingMode }};
}

class DrawImageBuffer {
public:
    static constexpr ItemType itemType = ItemType::DrawImageBuffer;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    DrawImageBuffer(RenderingResourceIdentifier imageBufferIdentifier, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
        : m_imageBufferIdentifier(imageBufferIdentifier)
        , m_destinationRect(destRect)
        , m_srcRect(srcRect)
        , m_options(options)
    {
    }

    RenderingResourceIdentifier imageBufferIdentifier() const { return m_imageBufferIdentifier; }
    FloatRect source() const { return m_srcRect; }
    FloatRect destinationRect() const { return m_destinationRect; }
    ImagePaintingOptions options() const { return m_options; }
    // FIXME: We might want to validate ImagePaintingOptions.
    bool isValid() const { return m_imageBufferIdentifier.isValid(); }

    void apply(GraphicsContext&, WebCore::ImageBuffer&) const;

    NO_RETURN_DUE_TO_ASSERT void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return m_destinationRect; }

private:
    RenderingResourceIdentifier m_imageBufferIdentifier;
    FloatRect m_destinationRect;
    FloatRect m_srcRect;
    ImagePaintingOptions m_options;
};

class DrawNativeImage {
public:
    static constexpr ItemType itemType = ItemType::DrawNativeImage;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    DrawNativeImage(RenderingResourceIdentifier imageIdentifier, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
        : m_imageIdentifier(imageIdentifier)
        , m_imageSize(imageSize)
        , m_destinationRect(destRect)
        , m_srcRect(srcRect)
        , m_options(options)
    {
    }

    RenderingResourceIdentifier imageIdentifier() const { return m_imageIdentifier; }
    const FloatRect& source() const { return m_srcRect; }
    const FloatRect& destinationRect() const { return m_destinationRect; }
    // FIXME: We might want to validate ImagePaintingOptions.
    bool isValid() const { return m_imageIdentifier.isValid(); }

    NO_RETURN_DUE_TO_ASSERT void apply(GraphicsContext&) const;
    void apply(GraphicsContext&, NativeImage&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return m_destinationRect; }

private:
    RenderingResourceIdentifier m_imageIdentifier;
    FloatSize m_imageSize;
    FloatRect m_destinationRect;
    FloatRect m_srcRect;
    ImagePaintingOptions m_options;
};

class DrawPattern {
public:
    static constexpr ItemType itemType = ItemType::DrawPattern;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    WEBCORE_EXPORT DrawPattern(RenderingResourceIdentifier, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& = { });

    RenderingResourceIdentifier imageIdentifier() const { return m_imageIdentifier; }
    FloatSize imageSize() const { return m_imageSize; }
    FloatRect destRect() const { return m_destination; }
    FloatRect tileRect() const { return m_tileRect; }
    const AffineTransform& patternTransform() const { return m_patternTransform; }
    FloatPoint phase() const { return m_phase; }
    FloatSize spacing() const { return m_spacing; }
    // FIXME: We might want to validate ImagePaintingOptions.
    bool isValid() const { return m_imageIdentifier.isValid(); }

    NO_RETURN_DUE_TO_ASSERT void apply(GraphicsContext&) const;
    void apply(GraphicsContext&, NativeImage&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return m_destination; }

private:
    RenderingResourceIdentifier m_imageIdentifier;
    FloatSize m_imageSize;
    FloatRect m_destination;
    FloatRect m_tileRect;
    AffineTransform m_patternTransform;
    FloatPoint m_phase;
    FloatSize m_spacing;
    ImagePaintingOptions m_options;
};

class BeginTransparencyLayer {
public:
    static constexpr ItemType itemType = ItemType::BeginTransparencyLayer;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true; // Is drawing item because the size of the transparency layer is implicitly the clip bounds.

    BeginTransparencyLayer(float opacity)
        : m_opacity(opacity)
    {
    }

    float opacity() const { return m_opacity; }

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return std::nullopt; }
    std::optional<FloatRect> globalBounds() const { return std::nullopt; }

private:
    float m_opacity;
};

class EndTransparencyLayer {
public:
    static constexpr ItemType itemType = ItemType::EndTransparencyLayer;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return std::nullopt; }
    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
};

class DrawRect {
public:
    static constexpr ItemType itemType = ItemType::DrawRect;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    DrawRect(FloatRect rect, float borderThickness)
        : m_rect(rect)
        , m_borderThickness(borderThickness)
    {
    }

    FloatRect rect() const { return m_rect; }
    float borderThickness() const { return m_borderThickness; }

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return m_rect; }

private:
    FloatRect m_rect;
    float m_borderThickness;
};

class DrawLine {
public:
    static constexpr ItemType itemType = ItemType::DrawLine;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    DrawLine(FloatPoint point1, FloatPoint point2)
        : m_point1(point1)
        , m_point2(point2)
    {
    }

    FloatPoint point1() const { return m_point1; }
    FloatPoint point2() const { return m_point2; }

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const;

private:
    FloatPoint m_point1;
    FloatPoint m_point2;
};

class DrawLinesForText {
public:
    static constexpr ItemType itemType = ItemType::DrawLinesForText;
    static constexpr bool isInlineItem = false;
    static constexpr bool isDrawingItem = true;

    WEBCORE_EXPORT DrawLinesForText(const FloatPoint& blockLocation, const FloatSize& localAnchor, float thickness, const DashArray& widths, bool printing, bool doubleLines);

    void setBlockLocation(const FloatPoint& blockLocation) { m_blockLocation = blockLocation; }
    const FloatPoint& blockLocation() const { return m_blockLocation; }
    const FloatSize& localAnchor() const { return m_localAnchor; }
    FloatPoint point() const { return m_blockLocation + m_localAnchor; }
    float thickness() const { return m_thickness; }
    const DashArray& widths() const { return m_widths; }
    bool isPrinting() const { return m_printing; }
    bool doubleLines() const { return m_doubleLines; }

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<DrawLinesForText> decode(Decoder&);

private:
    FloatPoint m_blockLocation;
    FloatSize m_localAnchor;
    DashArray m_widths;
    float m_thickness;
    bool m_printing;
    bool m_doubleLines;
};

template<class Encoder>
void DrawLinesForText::encode(Encoder& encoder) const
{
    encoder << m_blockLocation;
    encoder << m_localAnchor;
    encoder << m_widths;
    encoder << m_thickness;
    encoder << m_printing;
    encoder << m_doubleLines;
}

template<class Decoder>
std::optional<DrawLinesForText> DrawLinesForText::decode(Decoder& decoder)
{
    std::optional<FloatPoint> blockLocation;
    decoder >> blockLocation;
    if (!blockLocation)
        return std::nullopt;

    std::optional<FloatSize> localAnchor;
    decoder >> localAnchor;
    if (!localAnchor)
        return std::nullopt;

    std::optional<DashArray> widths;
    decoder >> widths;
    if (!widths)
        return std::nullopt;

    std::optional<float> thickness;
    decoder >> thickness;
    if (!thickness)
        return std::nullopt;

    std::optional<bool> printing;
    decoder >> printing;
    if (!printing)
        return std::nullopt;

    std::optional<bool> doubleLines;
    decoder >> doubleLines;
    if (!doubleLines)
        return std::nullopt;

    return {{ *blockLocation, *localAnchor, *thickness, *widths, *printing, *doubleLines }};
}

class DrawDotsForDocumentMarker {
public:
    static constexpr ItemType itemType = ItemType::DrawDotsForDocumentMarker;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    using UnderlyingDocumentMarkerLineStyleType = std::underlying_type<DocumentMarkerLineStyle::Mode>::type;

    DrawDotsForDocumentMarker(const FloatRect& rect, const DocumentMarkerLineStyle& style)
        : m_rect(rect)
        , m_styleMode(static_cast<UnderlyingDocumentMarkerLineStyleType>(style.mode))
        , m_styleShouldUseDarkAppearance(style.shouldUseDarkAppearance)
    {
    }

    DrawDotsForDocumentMarker(const FloatRect& rect, UnderlyingDocumentMarkerLineStyleType styleMode, bool styleShouldUseDarkAppearance)
        : m_rect(rect)
        , m_styleMode(styleMode)
        , m_styleShouldUseDarkAppearance(styleShouldUseDarkAppearance)
    {
    }

    bool isValid() const { return isValidEnum<DocumentMarkerLineStyle::Mode>(m_styleMode); }

    FloatRect rect() const { return m_rect; }

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const;

private:
    FloatRect m_rect;
    UnderlyingDocumentMarkerLineStyleType m_styleMode { 0 };
    bool m_styleShouldUseDarkAppearance { false };
};

class DrawEllipse {
public:
    static constexpr ItemType itemType = ItemType::DrawEllipse;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    DrawEllipse(const FloatRect& rect)
        : m_rect(rect)
    {
    }

    FloatRect rect() const { return m_rect; }

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return m_rect; }

private:
    FloatRect m_rect;
};

class DrawPath {
public:
    static constexpr ItemType itemType = ItemType::DrawPath;
    static constexpr bool isInlineItem = false;
    static constexpr bool isDrawingItem = true;

    DrawPath(const Path& path)
        : m_path(path)
    {
    }

    DrawPath(Path&& path)
        : m_path(WTFMove(path))
    {
    }

    const Path& path() const { return m_path; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<DrawPath> decode(Decoder&);

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return m_path.fastBoundingRect(); }

private:
    Path m_path;
};

template<class Encoder>
void DrawPath::encode(Encoder& encoder) const
{
    encoder << m_path;
}

template<class Decoder>
std::optional<DrawPath> DrawPath::decode(Decoder& decoder)
{
    std::optional<Path> path;
    decoder >> path;
    if (!path)
        return std::nullopt;

    return {{ WTFMove(*path) }};
}

class DrawFocusRingPath {
public:
    static constexpr ItemType itemType = ItemType::DrawFocusRingPath;
    static constexpr bool isInlineItem = false;
    static constexpr bool isDrawingItem = true;

    DrawFocusRingPath(Path&& path, float width, float offset, Color color)
        : m_path(WTFMove(path))
        , m_width(width)
        , m_offset(offset)
        , m_color(color)
    {
    }

    DrawFocusRingPath(const Path& path, float width, float offset, Color color)
        : m_path(path)
        , m_width(width)
        , m_offset(offset)
        , m_color(color)
    {
    }

    const Path& path() const { return m_path; }
    float width() const { return m_width; }
    float offset() const { return m_offset; }
    const Color& color() const { return m_color; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<DrawFocusRingPath> decode(Decoder&);

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const;

private:
    Path m_path;
    float m_width;
    float m_offset;
    Color m_color;
};

template<class Encoder>
void DrawFocusRingPath::encode(Encoder& encoder) const
{
    encoder << m_path;
    encoder << m_width;
    encoder << m_offset;
    encoder << m_color;
}

template<class Decoder>
std::optional<DrawFocusRingPath> DrawFocusRingPath::decode(Decoder& decoder)
{
    std::optional<Path> path;
    decoder >> path;
    if (!path)
        return std::nullopt;

    std::optional<float> width;
    decoder >> width;
    if (!width)
        return std::nullopt;

    std::optional<float> offset;
    decoder >> offset;
    if (!offset)
        return std::nullopt;

    std::optional<Color> color;
    decoder >> color;
    if (!color)
        return std::nullopt;

    return {{ WTFMove(*path), *width, *offset, *color }};
}

class DrawFocusRingRects {
public:
    static constexpr ItemType itemType = ItemType::DrawFocusRingRects;
    static constexpr bool isInlineItem = false;
    static constexpr bool isDrawingItem = true;

    WEBCORE_EXPORT DrawFocusRingRects(const Vector<FloatRect>&, float width, float offset, const Color&);

    const Vector<FloatRect> rects() const { return m_rects; }
    float width() const { return m_width; }
    float offset() const { return m_offset; }
    const Color& color() const { return m_color; }

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<DrawFocusRingRects> decode(Decoder&);

private:
    Vector<FloatRect> m_rects;
    float m_width;
    float m_offset;
    Color m_color;
};

template<class Encoder>
void DrawFocusRingRects::encode(Encoder& encoder) const
{
    encoder << m_rects;
    encoder << m_width;
    encoder << m_offset;
    encoder << m_color;
}

template<class Decoder>
std::optional<DrawFocusRingRects> DrawFocusRingRects::decode(Decoder& decoder)
{
    std::optional<Vector<FloatRect>> rects;
    decoder >> rects;
    if (!rects)
        return std::nullopt;

    std::optional<float> width;
    decoder >> width;
    if (!width)
        return std::nullopt;

    std::optional<float> offset;
    decoder >> offset;
    if (!offset)
        return std::nullopt;

    std::optional<Color> color;
    decoder >> color;
    if (!color)
        return std::nullopt;

    return {{ *rects, *width, *offset, *color }};
}

class FillRect {
public:
    static constexpr ItemType itemType = ItemType::FillRect;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    FillRect(const FloatRect& rect)
        : m_rect(rect)
    {
    }

    const FloatRect& rect() const { return m_rect; }

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return m_rect; }

private:
    FloatRect m_rect;
};

class FillRectWithColor {
public:
    static constexpr ItemType itemType = ItemType::FillRectWithColor;
    static constexpr bool isInlineItem = false;
    static constexpr bool isDrawingItem = true;

    FillRectWithColor(const FloatRect& rect, const Color& color)
        : m_rect(rect)
        , m_color(color)
    {
    }

    FloatRect rect() const { return m_rect; }
    const Color& color() const { return m_color; }

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return m_rect; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<FillRectWithColor> decode(Decoder&);

private:
    FloatRect m_rect;
    Color m_color;
};

template<class Encoder>
void FillRectWithColor::encode(Encoder& encoder) const
{
    encoder << m_rect;
    encoder << m_color;
}

template<class Decoder>
std::optional<FillRectWithColor> FillRectWithColor::decode(Decoder& decoder)
{
    std::optional<FloatRect> rect;
    decoder >> rect;
    if (!rect)
        return std::nullopt;

    std::optional<Color> color;
    decoder >> color;
    if (!color)
        return std::nullopt;

    return {{ *rect, *color }};
}

class FillRectWithGradient {
public:
    static constexpr ItemType itemType = ItemType::FillRectWithGradient;
    static constexpr bool isInlineItem = false;
    static constexpr bool isDrawingItem = true;

    WEBCORE_EXPORT FillRectWithGradient(const FloatRect&, Gradient&);

    const FloatRect& rect() const { return m_rect; }
    const Gradient& gradient() const { return m_gradient.get(); }

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return m_rect; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<FillRectWithGradient> decode(Decoder&);

private:
    FloatRect m_rect;
    mutable Ref<Gradient> m_gradient; // FIXME: Make this not mutable
};

template<class Encoder>
void FillRectWithGradient::encode(Encoder& encoder) const
{
    encoder << m_rect;
    encoder << m_gradient.get();
}

template<class Decoder>
std::optional<FillRectWithGradient> FillRectWithGradient::decode(Decoder& decoder)
{
    std::optional<FloatRect> rect;
    decoder >> rect;
    if (!rect)
        return std::nullopt;

    auto gradient = Gradient::decode(decoder);
    if (!gradient)
        return std::nullopt;

    return {{ *rect, gradient->get() }};
}

class FillCompositedRect {
public:
    static constexpr ItemType itemType = ItemType::FillCompositedRect;
    static constexpr bool isInlineItem = false;
    static constexpr bool isDrawingItem = true;

    FillCompositedRect(const FloatRect& rect, const Color& color, CompositeOperator op, BlendMode blendMode)
        : m_rect(rect)
        , m_color(color)
        , m_op(op)
        , m_blendMode(blendMode)
    {
    }

    FloatRect rect() const { return m_rect; }
    const Color& color() const { return m_color; }
    CompositeOperator compositeOperator() const { return m_op; }
    BlendMode blendMode() const { return m_blendMode; }

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return m_rect; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<FillCompositedRect> decode(Decoder&);

private:
    FloatRect m_rect;
    Color m_color;
    CompositeOperator m_op;
    BlendMode m_blendMode;
};

template<class Encoder>
void FillCompositedRect::encode(Encoder& encoder) const
{
    encoder << m_rect;
    encoder << m_color;
    encoder << m_op;
    encoder << m_blendMode;
}

template<class Decoder>
std::optional<FillCompositedRect> FillCompositedRect::decode(Decoder& decoder)
{
    std::optional<FloatRect> rect;
    decoder >> rect;
    if (!rect)
        return std::nullopt;

    std::optional<Color> color;
    decoder >> color;
    if (!color)
        return std::nullopt;

    std::optional<CompositeOperator> op;
    decoder >> op;
    if (!op)
        return std::nullopt;

    std::optional<BlendMode> blendMode;
    decoder >> blendMode;
    if (!blendMode)
        return std::nullopt;

    return {{ *rect, *color, *op, *blendMode }};
}

class FillRoundedRect {
public:
    static constexpr ItemType itemType = ItemType::FillRoundedRect;
    static constexpr bool isInlineItem = false;
    static constexpr bool isDrawingItem = true;

    FillRoundedRect(const FloatRoundedRect& rect, const Color& color, BlendMode blendMode)
        : m_rect(rect)
        , m_color(color)
        , m_blendMode(blendMode)
    {
    }

    const FloatRoundedRect& roundedRect() const { return m_rect; }
    const Color& color() const { return m_color; }
    BlendMode blendMode() const { return m_blendMode; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<FillRoundedRect> decode(Decoder&);

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return m_rect.rect(); }

private:
    FloatRoundedRect m_rect;
    Color m_color;
    BlendMode m_blendMode;
};

template<class Encoder>
void FillRoundedRect::encode(Encoder& encoder) const
{
    encoder << m_rect;
    encoder << m_color;
    encoder << m_blendMode;
}

template<class Decoder>
std::optional<FillRoundedRect> FillRoundedRect::decode(Decoder& decoder)
{
    std::optional<FloatRoundedRect> rect;
    decoder >> rect;
    if (!rect)
        return std::nullopt;

    std::optional<Color> color;
    decoder >> color;
    if (!color)
        return std::nullopt;

    std::optional<BlendMode> blendMode;
    decoder >> blendMode;
    if (!blendMode)
        return std::nullopt;

    return {{ *rect, *color, *blendMode }};
}

class FillRectWithRoundedHole {
public:
    static constexpr ItemType itemType = ItemType::FillRectWithRoundedHole;
    static constexpr bool isInlineItem = false;
    static constexpr bool isDrawingItem = true;

    FillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
        : m_rect(rect)
        , m_roundedHoleRect(roundedHoleRect)
        , m_color(color)
    {
    }

    const FloatRect& rect() const { return m_rect; }
    const FloatRoundedRect& roundedHoleRect() const { return m_roundedHoleRect; }
    const Color& color() const { return m_color; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<FillRectWithRoundedHole> decode(Decoder&);

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return m_rect; }

private:
    FloatRect m_rect;
    FloatRoundedRect m_roundedHoleRect;
    Color m_color;
};

template<class Encoder>
void FillRectWithRoundedHole::encode(Encoder& encoder) const
{
    encoder << m_rect;
    encoder << m_roundedHoleRect;
    encoder << m_color;
}

template<class Decoder>
std::optional<FillRectWithRoundedHole> FillRectWithRoundedHole::decode(Decoder& decoder)
{
    std::optional<FloatRect> rect;
    decoder >> rect;
    if (!rect)
        return std::nullopt;

    std::optional<FloatRoundedRect> roundedHoleRect;
    decoder >> roundedHoleRect;
    if (!roundedHoleRect)
        return std::nullopt;

    std::optional<Color> color;
    decoder >> color;
    if (!color)
        return std::nullopt;

    return {{ *rect, *roundedHoleRect, *color }};
}

#if ENABLE(INLINE_PATH_DATA)

class FillLine {
public:
    static constexpr ItemType itemType = ItemType::FillLine;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    FillLine(const LineData& lineData)
        : m_lineData(lineData)
    {
    }

    Path path() const { return Path::from({m_lineData}); }
    void apply(GraphicsContext&) const;
    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return path().fastBoundingRect(); }

private:
    LineData m_lineData;
};

class FillArc {
public:
    static constexpr ItemType itemType = ItemType::FillArc;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    FillArc(const ArcData& arcData)
        : m_arcData(arcData)
    {
    }

    Path path() const { return Path::from({m_arcData}); }
    void apply(GraphicsContext&) const;
    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return path().fastBoundingRect(); }

private:
    ArcData m_arcData;
};

class FillQuadCurve {
public:
    static constexpr ItemType itemType = ItemType::FillQuadCurve;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    FillQuadCurve(const QuadCurveData& quadCurveData)
        : m_quadCurveData(quadCurveData)
    {
    }

    Path path() const { return Path::from({m_quadCurveData}); }
    void apply(GraphicsContext&) const;
    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return path().fastBoundingRect(); }

private:
    QuadCurveData m_quadCurveData;
};

class FillBezierCurve {
public:
    static constexpr ItemType itemType = ItemType::FillBezierCurve;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    FillBezierCurve(const BezierCurveData& bezierCurveData)
        : m_bezierCurveData(bezierCurveData)
    {
    }

    Path path() const { return Path::from({m_bezierCurveData}); }
    void apply(GraphicsContext&) const;
    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return path().fastBoundingRect(); }

private:
    BezierCurveData m_bezierCurveData;
};

#endif // ENABLE(INLINE_PATH_DATA)

class FillPath {
public:
    static constexpr ItemType itemType = ItemType::FillPath;
    static constexpr bool isInlineItem = false;
    static constexpr bool isDrawingItem = true;

    FillPath(Path&& path)
        : m_path(WTFMove(path))
    {
    }

    FillPath(const Path& path)
        : m_path(path)
    {
    }

    const Path& path() const { return m_path; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<FillPath> decode(Decoder&);

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return m_path.fastBoundingRect(); }

private:
    Path m_path;
};

template<class Encoder>
void FillPath::encode(Encoder& encoder) const
{
    encoder << m_path;
}

template<class Decoder>
std::optional<FillPath> FillPath::decode(Decoder& decoder)
{
    std::optional<Path> path;
    decoder >> path;
    if (!path)
        return std::nullopt;

    return {{ WTFMove(*path) }};
}

class FillEllipse {
public:
    static constexpr ItemType itemType = ItemType::FillEllipse;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    FillEllipse(const FloatRect& rect)
        : m_rect(rect)
    {
    }

    FloatRect rect() const { return m_rect; }

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return m_rect; }

private:
    FloatRect m_rect;
};

class GetPixelBuffer {
public:
    static constexpr ItemType itemType = ItemType::GetPixelBuffer;
    static constexpr bool isInlineItem = false;
    static constexpr bool isDrawingItem = false;

    GetPixelBuffer(PixelBufferFormat outputFormat, const IntRect& srcRect)
        : m_srcRect(srcRect)
        , m_outputFormat(WTFMove(outputFormat))
    {
    }

    // Explicit destructor added to force non-trivial destructor on all platforms
    // as the encoding logic currently hardcodes which display list item types need
    // out of line treatment rather than using the isInlineItem constant.
    ~GetPixelBuffer() { }

    const PixelBufferFormat& outputFormat() const { return m_outputFormat; }
    IntRect srcRect() const { return m_srcRect; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<GetPixelBuffer> decode(Decoder&);

private:
    IntRect m_srcRect;
    PixelBufferFormat m_outputFormat;
};

template<class Encoder>
void GetPixelBuffer::encode(Encoder& encoder) const
{
    encoder << m_srcRect;
    encoder << m_outputFormat;
}

template<class Decoder>
std::optional<GetPixelBuffer> GetPixelBuffer::decode(Decoder& decoder)
{
    std::optional<IntRect> srcRect;
    decoder >> srcRect;
    if (!srcRect)
        return std::nullopt;

    std::optional<PixelBufferFormat> outputFormat;
    decoder >> outputFormat;
    if (!outputFormat)
        return std::nullopt;

    return {{ WTFMove(*outputFormat), *srcRect }};
}

class PutPixelBuffer {
public:
    static constexpr ItemType itemType = ItemType::PutPixelBuffer;
    static constexpr bool isInlineItem = false;
    static constexpr bool isDrawingItem = true;

    WEBCORE_EXPORT PutPixelBuffer(const PixelBuffer&, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat);
    WEBCORE_EXPORT PutPixelBuffer(PixelBuffer&&, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat);

    PutPixelBuffer(const PutPixelBuffer&);
    PutPixelBuffer(PutPixelBuffer&&) = default;
    PutPixelBuffer& operator=(const PutPixelBuffer&);
    PutPixelBuffer& operator=(PutPixelBuffer&&) = default;

    void swap(PutPixelBuffer&);

    const PixelBuffer& pixelBuffer() const { return m_pixelBuffer; }
    IntRect srcRect() const { return m_srcRect; }
    IntPoint destPoint() const { return m_destPoint; }
    AlphaPremultiplication destFormat() const { return m_destFormat; }

    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return std::nullopt; }
    std::optional<FloatRect> globalBounds() const { return {{ m_destPoint, m_srcRect.size() }}; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<PutPixelBuffer> decode(Decoder&);

private:
    IntRect m_srcRect;
    IntPoint m_destPoint;
    PixelBuffer m_pixelBuffer;
    AlphaPremultiplication m_destFormat;
};

template<class Encoder>
void PutPixelBuffer::encode(Encoder& encoder) const
{
    encoder << m_pixelBuffer;
    encoder << m_srcRect;
    encoder << m_destPoint;
    encoder << m_destFormat;
}

template<class Decoder>
std::optional<PutPixelBuffer> PutPixelBuffer::decode(Decoder& decoder)
{
    std::optional<PixelBuffer> pixelBuffer;
    std::optional<IntRect> srcRect;
    std::optional<IntPoint> destPoint;
    std::optional<AlphaPremultiplication> destFormat;

    decoder >> pixelBuffer;
    if (!pixelBuffer)
        return std::nullopt;

    decoder >> srcRect;
    if (!srcRect)
        return std::nullopt;

    decoder >> destPoint;
    if (!destPoint)
        return std::nullopt;

    decoder >> destFormat;
    if (!destFormat)
        return std::nullopt;

    return {{ WTFMove(*pixelBuffer), *srcRect, *destPoint, *destFormat }};
}

#if ENABLE(VIDEO)
class PaintFrameForMedia {
public:
    static constexpr ItemType itemType = ItemType::PaintFrameForMedia;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    PaintFrameForMedia(MediaPlayer&, const FloatRect& destination);

    const FloatRect& destination() const { return m_destination; }
    MediaPlayerIdentifier identifier() const { return m_identifier; }

    bool isValid() const { return m_identifier.isValid(); }

    NO_RETURN_DUE_TO_ASSERT void apply(GraphicsContext&) const;

    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return std::nullopt; }
    std::optional<FloatRect> globalBounds() const { return { m_destination }; }

private:
    MediaPlayerIdentifier m_identifier;
    FloatRect m_destination;
};
#endif

class StrokeRect {
public:
    static constexpr ItemType itemType = ItemType::StrokeRect;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    StrokeRect(const FloatRect& rect, float lineWidth)
        : m_rect(rect)
        , m_lineWidth(lineWidth)
    {
    }

    FloatRect rect() const { return m_rect; }
    float lineWidth() const { return m_lineWidth; }

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const;

private:
    FloatRect m_rect;
    float m_lineWidth;
};

class StrokeLine {
public:
    static constexpr ItemType itemType = ItemType::StrokeLine;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

#if ENABLE(INLINE_PATH_DATA)
    StrokeLine(const LineData& lineData)
        : m_start(lineData.start)
        , m_end(lineData.end)
    {
    }
#endif
    StrokeLine(const FloatPoint& start, const FloatPoint& end)
        : m_start(start)
        , m_end(end)
    {
    }

    FloatPoint start() const { return m_start; }
    FloatPoint end() const { return m_end; }

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const;

private:
    FloatPoint m_start;
    FloatPoint m_end;
};

#if ENABLE(INLINE_PATH_DATA)

class StrokeArc {
public:
    static constexpr ItemType itemType = ItemType::StrokeArc;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    StrokeArc(const ArcData& arcData)
        : m_arcData(arcData)
    {
    }

    Path path() const { return Path::from({m_arcData}); }
    void apply(GraphicsContext&) const;
    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const;

private:
    ArcData m_arcData;
};

class StrokeQuadCurve {
public:
    static constexpr ItemType itemType = ItemType::StrokeQuadCurve;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    StrokeQuadCurve(const QuadCurveData& quadCurveData)
        : m_quadCurveData(quadCurveData)
    {
    }

    Path path() const { return Path::from({m_quadCurveData}); }
    void apply(GraphicsContext&) const;
    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const;

private:
    QuadCurveData m_quadCurveData;
};

class StrokeBezierCurve {
public:
    static constexpr ItemType itemType = ItemType::StrokeBezierCurve;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    StrokeBezierCurve(const BezierCurveData& bezierCurveData)
        : m_bezierCurveData(bezierCurveData)
    {
    }

    Path path() const { return Path::from({m_bezierCurveData}); }
    void apply(GraphicsContext&) const;
    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const;

private:
    BezierCurveData m_bezierCurveData;
};

#endif // ENABLE(INLINE_PATH_DATA)

class StrokePath {
public:
    static constexpr ItemType itemType = ItemType::StrokePath;
    static constexpr bool isInlineItem = false;
    static constexpr bool isDrawingItem = true;

    StrokePath(Path&& path)
        : m_path(WTFMove(path))
    {
    }

    StrokePath(const Path& path)
        : m_path(path)
    {
    }

    const Path& path() const { return m_path; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<StrokePath> decode(Decoder&);

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const;

private:
    Path m_path;
};

template<class Encoder>
void StrokePath::encode(Encoder& encoder) const
{
    encoder << m_path;
}

template<class Decoder>
std::optional<StrokePath> StrokePath::decode(Decoder& decoder)
{
    std::optional<Path> path;
    decoder >> path;
    if (!path)
        return std::nullopt;

    return {{ WTFMove(*path) }};
}

class StrokeEllipse {
public:
    static constexpr ItemType itemType = ItemType::StrokeEllipse;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    StrokeEllipse(const FloatRect& rect)
        : m_rect(rect)
    {
    }

    const FloatRect& rect() const { return m_rect; }

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const;

private:
    FloatRect m_rect;
};

class ClearRect {
public:
    static constexpr ItemType itemType = ItemType::ClearRect;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = true;

    ClearRect(const FloatRect& rect)
        : m_rect(rect)
    {
    }

    const FloatRect& rect() const { return m_rect; }

    void apply(GraphicsContext&) const;

    std::optional<FloatRect> globalBounds() const { return std::nullopt; }
    std::optional<FloatRect> localBounds(const GraphicsContext&) const { return m_rect; }

private:
    FloatRect m_rect;
};

#if USE(CG)

class ApplyStrokePattern {
public:
    static constexpr ItemType itemType = ItemType::ApplyStrokePattern;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    void apply(GraphicsContext&) const;
};

class ApplyFillPattern {
public:
    static constexpr ItemType itemType = ItemType::ApplyFillPattern;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    void apply(GraphicsContext&) const;
};

#endif

class ApplyDeviceScaleFactor {
public:
    static constexpr ItemType itemType = ItemType::ApplyDeviceScaleFactor;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    ApplyDeviceScaleFactor(float scaleFactor)
        : m_scaleFactor(scaleFactor)
    {
    }

    float scaleFactor() const { return m_scaleFactor; }

    void apply(GraphicsContext&) const;

private:
    float m_scaleFactor { 1 };
};

class FlushContext {
public:
    static constexpr ItemType itemType = ItemType::FlushContext;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    explicit FlushContext(FlushIdentifier identifier)
        : m_identifier(identifier)
    {
    }

    FlushIdentifier identifier() const { return m_identifier; }
    bool isValid() const { return m_identifier.isValid(); }

    void apply(GraphicsContext&) const;

private:
    FlushIdentifier m_identifier;
};

// FIXME: This should be refactored so that the command to "switch to the next item buffer"
// is not itself a display list item.
class MetaCommandChangeItemBuffer {
public:
    static constexpr ItemType itemType = ItemType::MetaCommandChangeItemBuffer;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    explicit MetaCommandChangeItemBuffer(ItemBufferIdentifier identifier)
        : m_identifier(identifier)
    {
    }

    ItemBufferIdentifier identifier() const { return m_identifier; }
    bool isValid() const { return m_identifier.isValid(); }

private:
    ItemBufferIdentifier m_identifier;
};

class MetaCommandChangeDestinationImageBuffer {
public:
    static constexpr ItemType itemType = ItemType::MetaCommandChangeDestinationImageBuffer;
    static constexpr bool isInlineItem = true;
    static constexpr bool isDrawingItem = false;

    explicit MetaCommandChangeDestinationImageBuffer(RenderingResourceIdentifier identifier)
        : m_identifier(identifier)
    {
    }

    RenderingResourceIdentifier identifier() const { return m_identifier; }
    bool isValid() const { return m_identifier.isValid(); }

private:
    RenderingResourceIdentifier m_identifier;
};

using DisplayListItem = Variant
    < ApplyDeviceScaleFactor
    , BeginClipToDrawingCommands
    , BeginTransparencyLayer
    , ClearRect
    , ClearShadow
    , Clip
    , ClipOut
    , ClipOutToPath
    , ClipPath
    , ClipToImageBuffer
    , ConcatenateCTM
    , DrawDotsForDocumentMarker
    , DrawEllipse
    , DrawFocusRingPath
    , DrawFocusRingRects
    , DrawGlyphs
    , DrawImageBuffer
    , DrawLine
    , DrawLinesForText
    , DrawNativeImage
    , DrawPath
    , DrawPattern
    , DrawRect
    , EndClipToDrawingCommands
    , EndTransparencyLayer
    , FillCompositedRect
    , FillEllipse
    , FillPath
    , FillRect
    , FillRectWithColor
    , FillRectWithGradient
    , FillRectWithRoundedHole
    , FillRoundedRect
    , FlushContext
    , GetPixelBuffer
    , MetaCommandChangeDestinationImageBuffer
    , MetaCommandChangeItemBuffer
    , PutPixelBuffer
    , Restore
    , Rotate
    , Save
    , Scale
    , SetCTM
    , SetInlineFillColor
    , SetInlineFillGradient
    , SetInlineStrokeColor
    , SetLineCap
    , SetLineDash
    , SetLineJoin
    , SetMiterLimit
    , SetState
    , SetStrokeThickness
    , StrokeEllipse
    , StrokeLine
    , StrokePath
    , StrokeRect
    , Translate

#if ENABLE(INLINE_PATH_DATA)
    , FillLine
    , FillArc
    , FillQuadCurve
    , FillBezierCurve
    , StrokeArc
    , StrokeQuadCurve
    , StrokeBezierCurve
#endif

#if ENABLE(VIDEO)
    , PaintFrameForMedia
#endif

#if USE(CG)
    , ApplyFillPattern
    , ApplyStrokePattern
#endif
>;

size_t paddedSizeOfTypeAndItemInBytes(const DisplayListItem&);
ItemType displayListItemType(const DisplayListItem&);

TextStream& operator<<(TextStream&, ItemHandle);

} // namespace DisplayList
} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::DisplayList::ItemType> {
    using values = EnumValues<
    WebCore::DisplayList::ItemType,
    WebCore::DisplayList::ItemType::Save,
    WebCore::DisplayList::ItemType::Restore,
    WebCore::DisplayList::ItemType::Translate,
    WebCore::DisplayList::ItemType::Rotate,
    WebCore::DisplayList::ItemType::Scale,
    WebCore::DisplayList::ItemType::SetCTM,
    WebCore::DisplayList::ItemType::ConcatenateCTM,
    WebCore::DisplayList::ItemType::SetInlineFillGradient,
    WebCore::DisplayList::ItemType::SetInlineFillColor,
    WebCore::DisplayList::ItemType::SetInlineStrokeColor,
    WebCore::DisplayList::ItemType::SetStrokeThickness,
    WebCore::DisplayList::ItemType::SetState,
    WebCore::DisplayList::ItemType::SetLineCap,
    WebCore::DisplayList::ItemType::SetLineDash,
    WebCore::DisplayList::ItemType::SetLineJoin,
    WebCore::DisplayList::ItemType::SetMiterLimit,
    WebCore::DisplayList::ItemType::ClearShadow,
    WebCore::DisplayList::ItemType::Clip,
    WebCore::DisplayList::ItemType::ClipOut,
    WebCore::DisplayList::ItemType::ClipToImageBuffer,
    WebCore::DisplayList::ItemType::ClipOutToPath,
    WebCore::DisplayList::ItemType::ClipPath,
    WebCore::DisplayList::ItemType::BeginClipToDrawingCommands,
    WebCore::DisplayList::ItemType::EndClipToDrawingCommands,
    WebCore::DisplayList::ItemType::DrawGlyphs,
    WebCore::DisplayList::ItemType::DrawImageBuffer,
    WebCore::DisplayList::ItemType::DrawNativeImage,
    WebCore::DisplayList::ItemType::DrawPattern,
    WebCore::DisplayList::ItemType::DrawRect,
    WebCore::DisplayList::ItemType::DrawLine,
    WebCore::DisplayList::ItemType::DrawLinesForText,
    WebCore::DisplayList::ItemType::DrawDotsForDocumentMarker,
    WebCore::DisplayList::ItemType::DrawEllipse,
    WebCore::DisplayList::ItemType::DrawPath,
    WebCore::DisplayList::ItemType::DrawFocusRingPath,
    WebCore::DisplayList::ItemType::DrawFocusRingRects,
    WebCore::DisplayList::ItemType::FillRect,
    WebCore::DisplayList::ItemType::FillRectWithColor,
    WebCore::DisplayList::ItemType::FillRectWithGradient,
    WebCore::DisplayList::ItemType::FillCompositedRect,
    WebCore::DisplayList::ItemType::FillRoundedRect,
    WebCore::DisplayList::ItemType::FillRectWithRoundedHole,
#if ENABLE(INLINE_PATH_DATA)
    WebCore::DisplayList::ItemType::FillLine,
    WebCore::DisplayList::ItemType::FillArc,
    WebCore::DisplayList::ItemType::FillQuadCurve,
    WebCore::DisplayList::ItemType::FillBezierCurve,
#endif
    WebCore::DisplayList::ItemType::FillPath,
    WebCore::DisplayList::ItemType::FillEllipse,
    WebCore::DisplayList::ItemType::FlushContext,
    WebCore::DisplayList::ItemType::MetaCommandChangeDestinationImageBuffer,
    WebCore::DisplayList::ItemType::MetaCommandChangeItemBuffer,
    WebCore::DisplayList::ItemType::GetPixelBuffer,
    WebCore::DisplayList::ItemType::PutPixelBuffer,
#if ENABLE(VIDEO)
    WebCore::DisplayList::ItemType::PaintFrameForMedia,
#endif
    WebCore::DisplayList::ItemType::StrokeRect,
    WebCore::DisplayList::ItemType::StrokeLine,
#if ENABLE(INLINE_PATH_DATA)
    WebCore::DisplayList::ItemType::StrokeArc,
    WebCore::DisplayList::ItemType::StrokeQuadCurve,
    WebCore::DisplayList::ItemType::StrokeBezierCurve,
#endif
    WebCore::DisplayList::ItemType::StrokePath,
    WebCore::DisplayList::ItemType::StrokeEllipse,
    WebCore::DisplayList::ItemType::ClearRect,
    WebCore::DisplayList::ItemType::BeginTransparencyLayer,
    WebCore::DisplayList::ItemType::EndTransparencyLayer,
#if USE(CG)
    WebCore::DisplayList::ItemType::ApplyStrokePattern,
    WebCore::DisplayList::ItemType::ApplyFillPattern,
#endif
    WebCore::DisplayList::ItemType::ApplyDeviceScaleFactor
    >;

};

} // namespace WTF
