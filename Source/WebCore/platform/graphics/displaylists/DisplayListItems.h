/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "DisplayList.h"
#include "FloatRoundedRect.h"
#include "Font.h"
#include "GlyphBuffer.h"
#include "Gradient.h"
#include "Image.h"
#include "ImageData.h"
#include "Pattern.h"
#include "SharedBuffer.h"
#include <wtf/TypeCasts.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class ImageData;
struct ImagePaintingOptions;

namespace DisplayList {

class DrawingItem : public Item {
public:
    WEBCORE_EXPORT explicit DrawingItem(ItemType);

    WEBCORE_EXPORT virtual ~DrawingItem();

    void setExtent(const FloatRect& r) { m_extent = r; }
    const FloatRect& extent() const { return m_extent.value(); }

    bool extentKnown() const { return static_cast<bool>(m_extent); }

    // Return bounds of this drawing operation in local coordinates.
    // Does not include effets of transform, shadow etc in the state.
    virtual Optional<FloatRect> localBounds(const GraphicsContext&) const { return WTF::nullopt; }

    // Some operations (e.g. putImageData) operate in global bounds. For these operations, override this method.
    // If applicable, the return value needs to include the effects of shadows, clipping, etc.
    virtual Optional<FloatRect> globalBounds() const { return WTF::nullopt; }

private:
    bool isDrawingItem() const override { return true; }

    Optional<FloatRect> m_extent; // In base coordinates, taking shadows and transforms into account.
};

class Save : public Item {
public:
    static Ref<Save> create()
    {
        return adoptRef(*new Save);
    }

    WEBCORE_EXPORT virtual ~Save();

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<Save>> decode(Decoder&);

private:
    WEBCORE_EXPORT Save();

    void apply(GraphicsContext&) const override;
};

template<class Encoder>
void Save::encode(Encoder&) const
{
}

template<class Decoder>
Optional<Ref<Save>> Save::decode(Decoder&)
{
    return Save::create();
}

class Restore : public Item {
public:
    static Ref<Restore> create()
    {
        return adoptRef(*new Restore);
    }

    WEBCORE_EXPORT virtual ~Restore();

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<Restore>> decode(Decoder&);

private:
    WEBCORE_EXPORT Restore();

    void apply(GraphicsContext&) const override;
};

template<class Encoder>
void Restore::encode(Encoder&) const
{
}

template<class Decoder>
Optional<Ref<Restore>> Restore::decode(Decoder&)
{
    return Restore::create();
}

class Translate : public Item {
public:
    static Ref<Translate> create(float x, float y)
    {
        return adoptRef(*new Translate(x, y));
    }

    WEBCORE_EXPORT virtual ~Translate();

    float x() const { return m_x; }
    float y() const { return m_y; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<Translate>> decode(Decoder&);

private:
    WEBCORE_EXPORT Translate(float x, float y);

    void apply(GraphicsContext&) const override;

    float m_x;
    float m_y;
};

template<class Encoder>
void Translate::encode(Encoder& encoder) const
{
    encoder << m_x;
    encoder << m_y;
}

template<class Decoder>
Optional<Ref<Translate>> Translate::decode(Decoder& decoder)
{
    Optional<float> x;
    decoder >> x;
    if (!x)
        return WTF::nullopt;

    Optional<float> y;
    decoder >> y;
    if (!y)
        return WTF::nullopt;

    return Translate::create(*x, *y);
}

class Rotate : public Item {
public:
    static Ref<Rotate> create(float angleInRadians)
    {
        return adoptRef(*new Rotate(angleInRadians));
    }

    WEBCORE_EXPORT virtual ~Rotate();

    float angle() const { return m_angle; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<Rotate>> decode(Decoder&);

private:
    WEBCORE_EXPORT Rotate(float angle);

    void apply(GraphicsContext&) const override;

    float m_angle; // In radians.
};

template<class Encoder>
void Rotate::encode(Encoder& encoder) const
{
    encoder << m_angle;
}

template<class Decoder>
Optional<Ref<Rotate>> Rotate::decode(Decoder& decoder)
{
    Optional<float> angle;
    decoder >> angle;
    if (!angle)
        return WTF::nullopt;

    return Rotate::create(*angle);
}

class Scale : public Item {
public:
    static Ref<Scale> create(const FloatSize& size)
    {
        return adoptRef(*new Scale(size));
    }

    WEBCORE_EXPORT virtual ~Scale();

    const FloatSize& amount() const { return m_size; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<Scale>> decode(Decoder&);

private:
    WEBCORE_EXPORT Scale(const FloatSize&);

    void apply(GraphicsContext&) const override;

    FloatSize m_size;
};

template<class Encoder>
void Scale::encode(Encoder& encoder) const
{
    encoder << m_size;
}

template<class Decoder>
Optional<Ref<Scale>> Scale::decode(Decoder& decoder)
{
    Optional<FloatSize> scale;
    decoder >> scale;
    if (!scale)
        return WTF::nullopt;

    return Scale::create(*scale);
}

class SetCTM : public Item {
public:
    static Ref<SetCTM> create(const AffineTransform& matrix)
    {
        return adoptRef(*new SetCTM(matrix));
    }

    WEBCORE_EXPORT virtual ~SetCTM();

    const AffineTransform& transform() const { return m_transform; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<SetCTM>> decode(Decoder&);

private:
    WEBCORE_EXPORT SetCTM(const AffineTransform&);

    void apply(GraphicsContext&) const override;

    AffineTransform m_transform;
};

template<class Encoder>
void SetCTM::encode(Encoder& encoder) const
{
    encoder << m_transform;
}

template<class Decoder>
Optional<Ref<SetCTM>> SetCTM::decode(Decoder& decoder)
{
    Optional<AffineTransform> transform;
    decoder >> transform;
    if (!transform)
        return WTF::nullopt;

    return SetCTM::create(*transform);
}

class ConcatenateCTM : public Item {
public:
    static Ref<ConcatenateCTM> create(const AffineTransform& matrix)
    {
        return adoptRef(*new ConcatenateCTM(matrix));
    }

    WEBCORE_EXPORT virtual ~ConcatenateCTM();

    const AffineTransform& transform() const { return m_transform; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<ConcatenateCTM>> decode(Decoder&);

private:
    WEBCORE_EXPORT ConcatenateCTM(const AffineTransform&);

    void apply(GraphicsContext&) const override;

    AffineTransform m_transform;
};

template<class Encoder>
void ConcatenateCTM::encode(Encoder& encoder) const
{
    encoder << m_transform;
}

template<class Decoder>
Optional<Ref<ConcatenateCTM>> ConcatenateCTM::decode(Decoder& decoder)
{
    Optional<AffineTransform> transform;
    decoder >> transform;
    if (!transform)
        return WTF::nullopt;

    return ConcatenateCTM::create(*transform);
}

class SetState : public Item {
public:
    static Ref<SetState> create(const GraphicsContextState& state, GraphicsContextState::StateChangeFlags flags)
    {
        return adoptRef(*new SetState(state, flags));
    }

    static Ref<SetState> create(const GraphicsContextStateChange& stateChange)
    {
        return adoptRef(*new SetState(stateChange));
    }

    WEBCORE_EXPORT virtual ~SetState();
    
    const GraphicsContextStateChange& state() const { return m_state; }

    void accumulate(const GraphicsContextState&, GraphicsContextState::StateChangeFlags);

    void accumulate(GraphicsContextState&) const;

    static void builderState(GraphicsContext&, const GraphicsContextState&, GraphicsContextState::StateChangeFlags);

    static void dumpStateChanges(WTF::TextStream&, const GraphicsContextState&, GraphicsContextState::StateChangeFlags);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<SetState>> decode(Decoder&);

private:
    WEBCORE_EXPORT SetState(const GraphicsContextState&, GraphicsContextState::StateChangeFlags);
    WEBCORE_EXPORT SetState(const GraphicsContextStateChange&);

    void apply(GraphicsContext&) const override;

    GraphicsContextStateChange m_state;
};

template<class Encoder>
void SetState::encode(Encoder& encoder) const
{
    auto changeFlags = m_state.m_changeFlags;
    encoder << changeFlags;

    auto& state = m_state.m_state;

    if (changeFlags.contains(GraphicsContextState::StrokeGradientChange)) {
        encoder << !!state.strokeGradient;
        if (state.strokeGradient)
            encoder << *state.strokeGradient;
    }

    if (changeFlags.contains(GraphicsContextState::StrokePatternChange)) {
        encoder << !!state.strokePattern;
        if (state.strokePattern)
            encoder << *state.strokePattern;
    }

    if (changeFlags.contains(GraphicsContextState::FillGradientChange)) {
        encoder << !!state.fillGradient;
        if (state.fillGradient)
            encoder << *state.fillGradient;
    }

    if (changeFlags.contains(GraphicsContextState::FillPatternChange)) {
        encoder << !!state.fillPattern;
        if (state.fillPattern)
            encoder << *state.fillPattern;
    }

    if (changeFlags.contains(GraphicsContextState::ShadowChange)) {
        encoder << state.shadowOffset;
        encoder << state.shadowBlur;
        encoder << state.shadowColor;
#if USE(CG)
        encoder << state.shadowsUseLegacyRadius;
#endif // USE(CG)
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
Optional<Ref<SetState>> SetState::decode(Decoder& decoder)
{
    Optional<GraphicsContextState::StateChangeFlags> changeFlags;
    decoder >> changeFlags;
    if (!changeFlags)
        return WTF::nullopt;

    GraphicsContextStateChange stateChange;
    stateChange.m_changeFlags = *changeFlags;

    if (stateChange.m_changeFlags.contains(GraphicsContextState::StrokeGradientChange)) {
        Optional<bool> hasStrokeGradient;
        decoder >> hasStrokeGradient;
        if (!hasStrokeGradient.hasValue())
            return WTF::nullopt;

        if (hasStrokeGradient.value()) {
            auto strokeGradient = Gradient::decode(decoder);
            if (!strokeGradient)
                return WTF::nullopt;

            stateChange.m_state.strokeGradient = WTFMove(*strokeGradient);
        }
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::StrokePatternChange)) {
        Optional<bool> hasStrokePattern;
        decoder >> hasStrokePattern;
        if (!hasStrokePattern.hasValue())
            return WTF::nullopt;

        if (hasStrokePattern.value()) {
            auto strokePattern = Pattern::decode(decoder);
            if (!strokePattern)
                return WTF::nullopt;

            stateChange.m_state.strokePattern = WTFMove(*strokePattern);
        }
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::FillGradientChange)) {
        Optional<bool> hasFillGradient;
        decoder >> hasFillGradient;
        if (!hasFillGradient.hasValue())
            return WTF::nullopt;

        if (hasFillGradient.value()) {
            auto fillGradient = Gradient::decode(decoder);
            if (!fillGradient)
                return WTF::nullopt;

            stateChange.m_state.fillGradient = WTFMove(*fillGradient);
        }
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::FillPatternChange)) {
        Optional<bool> hasFillPattern;
        decoder >> hasFillPattern;
        if (!hasFillPattern.hasValue())
            return WTF::nullopt;

        if (hasFillPattern.value()) {
            auto fillPattern = Pattern::decode(decoder);
            if (!fillPattern)
                return WTF::nullopt;

            stateChange.m_state.fillPattern = WTFMove(*fillPattern);
        }
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::ShadowChange)) {
        Optional<FloatSize> shadowOffset;
        decoder >> shadowOffset;
        if (!shadowOffset)
            return WTF::nullopt;

        stateChange.m_state.shadowOffset = *shadowOffset;

        Optional<float> shadowBlur;
        decoder >> shadowBlur;
        if (!shadowBlur)
            return WTF::nullopt;

        stateChange.m_state.shadowBlur = *shadowBlur;

        Optional<Color> shadowColor;
        decoder >> shadowColor;
        if (!shadowColor)
            return WTF::nullopt;

        stateChange.m_state.shadowColor = *shadowColor;

#if USE(CG)
        Optional<bool> shadowsUseLegacyRadius;
        decoder >> shadowsUseLegacyRadius;
        if (!shadowsUseLegacyRadius)
            return WTF::nullopt;

        stateChange.m_state.shadowsUseLegacyRadius = *shadowsUseLegacyRadius;
#endif // USE(CG)
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::StrokeThicknessChange)) {
        Optional<float> strokeThickness;
        decoder >> strokeThickness;
        if (!strokeThickness)
            return WTF::nullopt;

        stateChange.m_state.strokeThickness = *strokeThickness;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::TextDrawingModeChange)) {
        Optional<TextDrawingModeFlags> textDrawingMode;
        decoder >> textDrawingMode;
        if (!textDrawingMode)
            return WTF::nullopt;

        stateChange.m_state.textDrawingMode = WTFMove(*textDrawingMode);
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::StrokeColorChange)) {
        Optional<Color> strokeColor;
        decoder >> strokeColor;
        if (!strokeColor)
            return WTF::nullopt;

        stateChange.m_state.strokeColor = *strokeColor;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::FillColorChange)) {
        Optional<Color> fillColor;
        decoder >> fillColor;
        if (!fillColor)
            return WTF::nullopt;

        stateChange.m_state.fillColor = *fillColor;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::StrokeStyleChange)) {
        StrokeStyle strokeStyle;
        if (!decoder.decode(strokeStyle))
            return WTF::nullopt;

        stateChange.m_state.strokeStyle = strokeStyle;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::FillRuleChange)) {
        Optional<WindRule> fillRule;
        decoder >> fillRule;
        if (!fillRule)
            return WTF::nullopt;

        stateChange.m_state.fillRule = *fillRule;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::CompositeOperationChange)) {
        Optional<CompositeOperator> compositeOperator;
        decoder >> compositeOperator;
        if (!compositeOperator)
            return WTF::nullopt;

        stateChange.m_state.compositeOperator = *compositeOperator;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::BlendModeChange)) {
        Optional<BlendMode> blendMode;
        decoder >> blendMode;
        if (!blendMode)
            return WTF::nullopt;

        stateChange.m_state.blendMode = *blendMode;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::ImageInterpolationQualityChange)) {
        Optional<InterpolationQuality> imageInterpolationQuality;
        decoder >> imageInterpolationQuality;
        if (!imageInterpolationQuality)
            return WTF::nullopt;

        stateChange.m_state.imageInterpolationQuality = *imageInterpolationQuality;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::AlphaChange)) {
        Optional<float> alpha;
        decoder >> alpha;
        if (!alpha)
            return WTF::nullopt;

        stateChange.m_state.alpha = *alpha;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::ShouldAntialiasChange)) {
        Optional<bool> shouldAntialias;
        decoder >> shouldAntialias;
        if (!shouldAntialias)
            return WTF::nullopt;

        stateChange.m_state.shouldAntialias = *shouldAntialias;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::ShouldSmoothFontsChange)) {
        Optional<bool> shouldSmoothFonts;
        decoder >> shouldSmoothFonts;
        if (!shouldSmoothFonts)
            return WTF::nullopt;

        stateChange.m_state.shouldSmoothFonts = *shouldSmoothFonts;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::ShouldSubpixelQuantizeFontsChange)) {
        Optional<bool> shouldSubpixelQuantizeFonts;
        decoder >> shouldSubpixelQuantizeFonts;
        if (!shouldSubpixelQuantizeFonts)
            return WTF::nullopt;

        stateChange.m_state.shouldSubpixelQuantizeFonts = *shouldSubpixelQuantizeFonts;
    }

    if (stateChange.m_changeFlags.contains(GraphicsContextState::ShadowsIgnoreTransformsChange)) {
        Optional<bool> shadowsIgnoreTransforms;
        decoder >> shadowsIgnoreTransforms;
        if (!shadowsIgnoreTransforms)
            return WTF::nullopt;

        stateChange.m_state.shadowsIgnoreTransforms = *shadowsIgnoreTransforms;
    }

    return SetState::create(stateChange);
}

class SetLineCap : public Item {
public:
    static Ref<SetLineCap> create(LineCap lineCap)
    {
        return adoptRef(*new SetLineCap(lineCap));
    }

    WEBCORE_EXPORT virtual ~SetLineCap();
    
    LineCap lineCap() const { return m_lineCap; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<SetLineCap>> decode(Decoder&);

private:
    WEBCORE_EXPORT SetLineCap(LineCap);

    void apply(GraphicsContext&) const override;

    LineCap m_lineCap;
};

template<class Encoder>
void SetLineCap::encode(Encoder& encoder) const
{
    encoder << m_lineCap;
}

template<class Decoder>
Optional<Ref<SetLineCap>> SetLineCap::decode(Decoder& decoder)
{
    Optional<LineCap> lineCap;
    decoder >> lineCap;
    if (!lineCap)
        return WTF::nullopt;

    return SetLineCap::create(*lineCap);
}

class SetLineDash : public Item {
public:
    static Ref<SetLineDash> create(const DashArray& dashArray, float dashOffset)
    {
        return adoptRef(*new SetLineDash(dashArray, dashOffset));
    }

    WEBCORE_EXPORT virtual ~SetLineDash();

    const DashArray& dashArray() const { return m_dashArray; }
    float dashOffset() const { return m_dashOffset; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<SetLineDash>> decode(Decoder&);

private:
    WEBCORE_EXPORT SetLineDash(const DashArray&, float dashOffset);

    void apply(GraphicsContext&) const override;

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
Optional<Ref<SetLineDash>> SetLineDash::decode(Decoder& decoder)
{
    Optional<DashArray> dashArray;
    decoder >> dashArray;
    if (!dashArray)
        return WTF::nullopt;

    Optional<float> dashOffset;
    decoder >> dashOffset;
    if (!dashOffset)
        return WTF::nullopt;

    return SetLineDash::create(*dashArray, *dashOffset);
}

class SetLineJoin : public Item {
public:
    static Ref<SetLineJoin> create(LineJoin lineJoin)
    {
        return adoptRef(*new SetLineJoin(lineJoin));
    }

    WEBCORE_EXPORT virtual ~SetLineJoin();
    
    LineJoin lineJoin() const { return m_lineJoin; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<SetLineJoin>> decode(Decoder&);

private:
    WEBCORE_EXPORT SetLineJoin(LineJoin);

    void apply(GraphicsContext&) const override;

    LineJoin m_lineJoin;
};

template<class Encoder>
void SetLineJoin::encode(Encoder& encoder) const
{
    encoder << m_lineJoin;
}

template<class Decoder>
Optional<Ref<SetLineJoin>> SetLineJoin::decode(Decoder& decoder)
{
    Optional<LineJoin> lineJoin;
    decoder >> lineJoin;
    if (!lineJoin)
        return WTF::nullopt;

    return SetLineJoin::create(*lineJoin);
}

class SetMiterLimit : public Item {
public:
    static Ref<SetMiterLimit> create(float limit)
    {
        return adoptRef(*new SetMiterLimit(limit));
    }

    WEBCORE_EXPORT virtual ~SetMiterLimit();

    float miterLimit() const { return m_miterLimit; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<SetMiterLimit>> decode(Decoder&);

private:
    WEBCORE_EXPORT SetMiterLimit(float);

    void apply(GraphicsContext&) const override;

    float m_miterLimit;
};

template<class Encoder>
void SetMiterLimit::encode(Encoder& encoder) const
{
    encoder << m_miterLimit;
}

template<class Decoder>
Optional<Ref<SetMiterLimit>> SetMiterLimit::decode(Decoder& decoder)
{
    Optional<float> miterLimit;
    decoder >> miterLimit;
    if (!miterLimit)
        return WTF::nullopt;

    return SetMiterLimit::create(*miterLimit);
}

class ClearShadow : public Item {
public:
    static Ref<ClearShadow> create()
    {
        return adoptRef(*new ClearShadow);
    }

    WEBCORE_EXPORT virtual ~ClearShadow();

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<ClearShadow>> decode(Decoder&);

private:
    WEBCORE_EXPORT ClearShadow();

    void apply(GraphicsContext&) const override;
};

template<class Encoder>
void ClearShadow::encode(Encoder&) const
{
}

template<class Decoder>
Optional<Ref<ClearShadow>> ClearShadow::decode(Decoder&)
{
    return ClearShadow::create();
}

// FIXME: treat as DrawingItem?
class Clip : public Item {
public:
    static Ref<Clip> create(const FloatRect& rect)
    {
        return adoptRef(*new Clip(rect));
    }

    WEBCORE_EXPORT virtual ~Clip();

    FloatRect rect() const { return m_rect; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<Clip>> decode(Decoder&);

private:
    WEBCORE_EXPORT Clip(const FloatRect&);

    void apply(GraphicsContext&) const override;

    FloatRect m_rect;
};

template<class Encoder>
void Clip::encode(Encoder& encoder) const
{
    encoder << m_rect;
}

template<class Decoder>
Optional<Ref<Clip>> Clip::decode(Decoder& decoder)
{
    Optional<FloatRect> rect;
    decoder >> rect;
    if (!rect)
        return WTF::nullopt;

    return Clip::create(*rect);
}

class ClipOut : public Item {
public:
    static Ref<ClipOut> create(const FloatRect& rect)
    {
        return adoptRef(*new ClipOut(rect));
    }

    WEBCORE_EXPORT virtual ~ClipOut();

    FloatRect rect() const { return m_rect; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<ClipOut>> decode(Decoder&);

private:
    WEBCORE_EXPORT ClipOut(const FloatRect&);

    void apply(GraphicsContext&) const override;

    FloatRect m_rect;
};

template<class Encoder>
void ClipOut::encode(Encoder& encoder) const
{
    encoder << m_rect;
}

template<class Decoder>
Optional<Ref<ClipOut>> ClipOut::decode(Decoder& decoder)
{
    Optional<FloatRect> rect;
    decoder >> rect;
    if (!rect)
        return WTF::nullopt;

    return ClipOut::create(*rect);
}

class ClipOutToPath : public Item {
public:
    static Ref<ClipOutToPath> create(const Path& path)
    {
        return adoptRef(*new ClipOutToPath(path));
    }

    WEBCORE_EXPORT virtual ~ClipOutToPath();

    const Path& path() const { return m_path; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<ClipOutToPath>> decode(Decoder&);

private:
    WEBCORE_EXPORT ClipOutToPath(const Path&);

    void apply(GraphicsContext&) const override;

    const Path m_path;
};

template<class Encoder>
void ClipOutToPath::encode(Encoder& encoder) const
{
    encoder << m_path;
}

template<class Decoder>
Optional<Ref<ClipOutToPath>> ClipOutToPath::decode(Decoder& decoder)
{
    Optional<Path> path;
    decoder >> path;
    if (!path)
        return WTF::nullopt;

    return ClipOutToPath::create(*path);
}

class ClipPath : public Item {
public:
    static Ref<ClipPath> create(const Path& path, WindRule windRule)
    {
        return adoptRef(*new ClipPath(path, windRule));
    }

    WEBCORE_EXPORT ~ClipPath();

    const Path& path() const { return m_path; }
    WindRule windRule() const { return m_windRule; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<ClipPath>> decode(Decoder&);

private:
    WEBCORE_EXPORT ClipPath(const Path&, WindRule);

    void apply(GraphicsContext&) const override;

    const Path m_path;
    WindRule m_windRule;
};

template<class Encoder>
void ClipPath::encode(Encoder& encoder) const
{
    encoder << m_path;
    encoder << m_windRule;
}

template<class Decoder>
Optional<Ref<ClipPath>> ClipPath::decode(Decoder& decoder)
{
    Optional<Path> path;
    decoder >> path;
    if (!path)
        return WTF::nullopt;

    Optional<WindRule> windRule;
    decoder >> windRule;
    if (!windRule)
        return WTF::nullopt;

    return ClipPath::create(*path, *windRule);
}

class ClipToDrawingCommands : public Item {
public:
    static Ref<ClipToDrawingCommands> create(const FloatRect& destination, ColorSpace colorSpace, DisplayList&& drawingCommands)
    {
        return adoptRef(*new ClipToDrawingCommands(destination, colorSpace, WTFMove(drawingCommands)));
    }

    WEBCORE_EXPORT ~ClipToDrawingCommands();

    const FloatRect& destination() const { return m_destination; }
    ColorSpace colorSpace() const { return m_colorSpace; }
    const DisplayList& drawingCommands() const { return m_drawingCommands; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<ClipToDrawingCommands>> decode(Decoder&);

private:
    WEBCORE_EXPORT ClipToDrawingCommands(const FloatRect& destination, ColorSpace, DisplayList&& drawingCommands);

    void apply(GraphicsContext&) const override;

    FloatRect m_destination;
    ColorSpace m_colorSpace;
    DisplayList m_drawingCommands;
};

template<class Encoder>
void ClipToDrawingCommands::encode(Encoder& encoder) const
{
    encoder << m_destination;
    encoder << m_colorSpace;
    encoder << m_drawingCommands;
}

template<class Decoder>
Optional<Ref<ClipToDrawingCommands>> ClipToDrawingCommands::decode(Decoder& decoder)
{
    Optional<FloatRect> destination;
    decoder >> destination;
    if (!destination)
        return WTF::nullopt;

    Optional<ColorSpace> colorSpace;
    decoder >> colorSpace;
    if (!colorSpace)
        return WTF::nullopt;

    Optional<DisplayList> drawingCommands;
    decoder >> drawingCommands;
    if (!drawingCommands)
        return WTF::nullopt;

    return ClipToDrawingCommands::create(*destination, *colorSpace, WTFMove(*drawingCommands));
}

class DrawGlyphs : public DrawingItem {
public:
    static Ref<DrawGlyphs> create(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned count, const FloatPoint& blockLocation, const FloatSize& localAnchor, FontSmoothingMode smoothingMode)
    {
        return adoptRef(*new DrawGlyphs(font, glyphs, advances, count, blockLocation, localAnchor, smoothingMode));
    }

    static Ref<DrawGlyphs> create(const Font& font, Vector<GlyphBufferGlyph, 128>&& glyphs, Vector<GlyphBufferAdvance, 128>&& advances, const FloatPoint& blockLocation, const FloatSize& localAnchor, FontSmoothingMode smoothingMode)
    {
        return adoptRef(*new DrawGlyphs(font, WTFMove(glyphs), WTFMove(advances), blockLocation, localAnchor, smoothingMode));
    }

    WEBCORE_EXPORT virtual ~DrawGlyphs();

    const FloatPoint& blockLocation() const { return m_blockLocation; }
    void setBlockLocation(const FloatPoint& blockLocation) { m_blockLocation = blockLocation; }

    const FloatSize& localAnchor() const { return m_localAnchor; }

    FloatPoint anchorPoint() const { return m_blockLocation + m_localAnchor; }

    const Vector<GlyphBufferGlyph, 128>& glyphs() const { return m_glyphs; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<DrawGlyphs>> decode(Decoder&);

private:
    DrawGlyphs(const Font&, const GlyphBufferGlyph*, const GlyphBufferAdvance*, unsigned count, const FloatPoint& blockLocation, const FloatSize& localAnchor, FontSmoothingMode);
    WEBCORE_EXPORT DrawGlyphs(const Font&, Vector<GlyphBufferGlyph, 128>&&, Vector<GlyphBufferAdvance, 128>&&, const FloatPoint& blockLocation, const FloatSize& localAnchor, FontSmoothingMode);

    void computeBounds();

    void apply(GraphicsContext&) const override;

    Optional<FloatRect> localBounds(const GraphicsContext&) const override;

    GlyphBuffer generateGlyphBuffer() const;

    Ref<Font> m_font;
    Vector<GlyphBufferGlyph, 128> m_glyphs;
    Vector<GlyphBufferAdvance, 128> m_advances;
    FloatRect m_bounds;
    FloatPoint m_blockLocation;
    FloatSize m_localAnchor;
    FontSmoothingMode m_smoothingMode;
};

template<class Encoder>
void DrawGlyphs::encode(Encoder& encoder) const
{
    FontHandle handle;
    handle.font = m_font.ptr();
    encoder << handle;
    encoder << m_glyphs;
    encoder << m_advances;
    encoder << m_blockLocation;
    encoder << m_localAnchor;
    encoder << m_smoothingMode;
}

template<class Decoder>
Optional<Ref<DrawGlyphs>> DrawGlyphs::decode(Decoder& decoder)
{
    Optional<FontHandle> handle;
    decoder >> handle;
    if (!handle || !handle->font)
        return WTF::nullopt;

    Optional<Vector<GlyphBufferGlyph, 128>> glyphs;
    decoder >> glyphs;
    if (!glyphs)
        return WTF::nullopt;

    Optional<Vector<GlyphBufferAdvance, 128>> advances;
    decoder >> advances;
    if (!advances)
        return WTF::nullopt;

    if (glyphs->size() != advances->size())
        return WTF::nullopt;

    Optional<FloatPoint> blockLocation;
    decoder >> blockLocation;
    if (!blockLocation)
        return WTF::nullopt;

    Optional<FloatSize> localAnchor;
    decoder >> localAnchor;
    if (!localAnchor)
        return WTF::nullopt;

    Optional<FontSmoothingMode> smoothingMode;
    decoder >> smoothingMode;
    if (!smoothingMode)
        return WTF::nullopt;

    return DrawGlyphs::create(handle->font.releaseNonNull(), WTFMove(*glyphs), WTFMove(*advances), *blockLocation, *localAnchor, *smoothingMode);
}

class DrawImage : public DrawingItem {
public:
    static Ref<DrawImage> create(Image& image, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions& imagePaintingOptions)
    {
        return adoptRef(*new DrawImage(image, destination, source, imagePaintingOptions));
    }

    WEBCORE_EXPORT virtual ~DrawImage();

    const Image& image() const { return m_image.get(); }
    FloatRect source() const { return m_source; }
    FloatRect destination() const { return m_destination; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<DrawImage>> decode(Decoder&);

private:
    WEBCORE_EXPORT DrawImage(Image&, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions&);

    void apply(GraphicsContext&) const override;

    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_destination; }

    mutable Ref<Image> m_image; // FIXME: Drawing images can cause their animations to progress. This shouldn't have to be mutable.
    FloatRect m_destination;
    FloatRect m_source;
    ImagePaintingOptions m_imagePaintingOptions;
};

template<class Encoder>
void DrawImage::encode(Encoder& encoder) const
{
    ImageHandle imageHandle;
    imageHandle.image = m_image.ptr();

    encoder << imageHandle;
    encoder << m_destination;
    encoder << m_source;
    encoder << m_imagePaintingOptions;
}

template<class Decoder>
Optional<Ref<DrawImage>> DrawImage::decode(Decoder& decoder)
{
    Optional<ImageHandle> imageHandle;
    decoder >> imageHandle;
    if (!imageHandle)
        return WTF::nullopt;

    Optional<FloatRect> destination;
    decoder >> destination;
    if (!destination)
        return WTF::nullopt;

    Optional<FloatRect> source;
    decoder >> source;
    if (!source)
        return WTF::nullopt;

    Optional<ImagePaintingOptions> imagePaintingOptions;
    decoder >> imagePaintingOptions;
    if (!imagePaintingOptions)
        return WTF::nullopt;

    return DrawImage::create(*imageHandle->image, *destination, *source, *imagePaintingOptions);
}

class DrawTiledImage : public DrawingItem {
public:
    static Ref<DrawTiledImage> create(Image& image, const FloatRect& destination, const FloatPoint& source, const FloatSize& tileSize, const FloatSize& spacing, const ImagePaintingOptions& imagePaintingOptions)
    {
        return adoptRef(*new DrawTiledImage(image, destination, source, tileSize, spacing, imagePaintingOptions));
    }

    WEBCORE_EXPORT virtual ~DrawTiledImage();

    const Image& image() const { return m_image.get(); }
    FloatPoint source() const { return m_source; }
    FloatRect destination() const { return m_destination; }

    FloatSize tileSize() const { return m_tileSize; }
    FloatSize spacing() const { return m_spacing; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<DrawTiledImage>> decode(Decoder&);

private:
    WEBCORE_EXPORT DrawTiledImage(Image&, const FloatRect& destination, const FloatPoint& source, const FloatSize& tileSize, const FloatSize& spacing, const ImagePaintingOptions&);

    void apply(GraphicsContext&) const override;

    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_destination; }

    mutable Ref<Image> m_image; // FIXME: Drawing images can cause their animations to progress. This shouldn't have to be mutable.
    FloatRect m_destination;
    FloatPoint m_source;
    FloatSize m_tileSize;
    FloatSize m_spacing;
    ImagePaintingOptions m_imagePaintingOptions;
};

template<class Encoder>
void DrawTiledImage::encode(Encoder& encoder) const
{
    ImageHandle imageHandle;
    imageHandle.image = m_image.ptr();
    encoder << imageHandle;
    encoder << m_destination;
    encoder << m_source;
    encoder << m_tileSize;
    encoder << m_spacing;
    encoder << m_imagePaintingOptions;
}

template<class Decoder>
Optional<Ref<DrawTiledImage>> DrawTiledImage::decode(Decoder& decoder)
{
    Optional<ImageHandle> imageHandle;
    decoder >> imageHandle;
    if (!imageHandle)
        return WTF::nullopt;

    Optional<FloatRect> destination;
    decoder >> destination;
    if (!destination)
        return WTF::nullopt;

    Optional<FloatPoint> source;
    decoder >> source;
    if (!source)
        return WTF::nullopt;

    Optional<FloatSize> tileSize;
    decoder >> tileSize;
    if (!tileSize)
        return WTF::nullopt;

    Optional<FloatSize> spacing;
    decoder >> spacing;
    if (!spacing)
        return WTF::nullopt;

    Optional<ImagePaintingOptions> imagePaintingOptions;
    decoder >> imagePaintingOptions;
    if (!imagePaintingOptions)
        return WTF::nullopt;

    return DrawTiledImage::create(*imageHandle->image, *destination, *source, *tileSize, *spacing, *imagePaintingOptions);
}

class DrawTiledScaledImage : public DrawingItem {
public:
    static Ref<DrawTiledScaledImage> create(Image& image, const FloatRect& destination, const FloatRect& source, const FloatSize& tileScaleFactor, Image::TileRule hRule, Image::TileRule vRule, const ImagePaintingOptions& imagePaintingOptions)
    {
        return adoptRef(*new DrawTiledScaledImage(image, destination, source, tileScaleFactor, hRule, vRule, imagePaintingOptions));
    }

    WEBCORE_EXPORT virtual ~DrawTiledScaledImage();

    const Image& image() const { return m_image.get(); }
    FloatRect source() const { return m_source; }
    FloatRect destination() const { return m_destination; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<DrawTiledScaledImage>> decode(Decoder&);

private:
    WEBCORE_EXPORT DrawTiledScaledImage(Image&, const FloatRect& destination, const FloatRect& source, const FloatSize& tileScaleFactor, Image::TileRule hRule, Image::TileRule vRule, const ImagePaintingOptions&);

    void apply(GraphicsContext&) const override;

    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_destination; }

    mutable Ref<Image> m_image; // FIXME: Drawing images can cause their animations to progress. This shouldn't have to be mutable.
    FloatRect m_destination;
    FloatRect m_source;
    FloatSize m_tileScaleFactor;
    Image::TileRule m_hRule;
    Image::TileRule m_vRule;
    ImagePaintingOptions m_imagePaintingOptions;
};

template<class Encoder>
void DrawTiledScaledImage::encode(Encoder& encoder) const
{
    ImageHandle imageHandle;
    imageHandle.image = m_image.ptr();
    encoder << imageHandle;
    encoder << m_destination;
    encoder << m_source;
    encoder << m_tileScaleFactor;
    encoder << m_hRule;
    encoder << m_vRule;
    encoder << m_imagePaintingOptions;
}

template<class Decoder>
Optional<Ref<DrawTiledScaledImage>> DrawTiledScaledImage::decode(Decoder& decoder)
{
    Optional<ImageHandle> imageHandle;
    decoder >> imageHandle;
    if (!imageHandle)
        return WTF::nullopt;

    Optional<FloatRect> destination;
    decoder >> destination;
    if (!destination)
        return WTF::nullopt;

    Optional<FloatRect> source;
    decoder >> source;
    if (!source)
        return WTF::nullopt;

    Optional<FloatSize> tileScaleFactor;
    decoder >> tileScaleFactor;
    if (!tileScaleFactor)
        return WTF::nullopt;

    Image::TileRule hRule;
    if (!decoder.decode(hRule))
        return WTF::nullopt;

    Image::TileRule vRule;
    if (!decoder.decode(vRule))
        return WTF::nullopt;

    Optional<ImagePaintingOptions> imagePaintingOptions;
    decoder >> imagePaintingOptions;
    if (!imagePaintingOptions)
        return WTF::nullopt;

    return DrawTiledScaledImage::create(*imageHandle->image, *destination, *source, *tileScaleFactor, hRule, vRule, *imagePaintingOptions);
}

#if USE(CG) || USE(CAIRO) || USE(DIRECT2D)
class DrawNativeImage : public DrawingItem {
public:
    static Ref<DrawNativeImage> create(const NativeImagePtr& image, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
    {
        return adoptRef(*new DrawNativeImage(image, imageSize, destRect, srcRect, options));
    }

    WEBCORE_EXPORT virtual ~DrawNativeImage();

    FloatRect source() const { return m_srcRect; }
    FloatRect destinationRect() const { return m_destinationRect; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<DrawNativeImage>> decode(Decoder&);

private:
    WEBCORE_EXPORT DrawNativeImage(const NativeImagePtr&, const FloatSize& selfSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions&);

    void apply(GraphicsContext&) const override;

    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_destinationRect; }

#if USE(CG)
    NativeImagePtr m_image;
#endif
    FloatSize m_imageSize;
    FloatRect m_destinationRect;
    FloatRect m_srcRect;
    ImagePaintingOptions m_options;
};

template<class Encoder>
void DrawNativeImage::encode(Encoder& encoder) const
{
#if USE(CG)
    NativeImageHandle handle { m_image };
    encoder << handle;
#endif
    encoder << m_imageSize;
    encoder << m_destinationRect;
    encoder << m_srcRect;
    encoder << m_options;
}

template<class Decoder>
Optional<Ref<DrawNativeImage>> DrawNativeImage::decode(Decoder& decoder)
{
#if USE(CG)
    Optional<NativeImageHandle> handle;
    decoder >> handle;
    if (!handle)
        return WTF::nullopt;
#endif

    Optional<FloatSize> imageSize;
    decoder >> imageSize;
    if (!imageSize)
        return WTF::nullopt;

    Optional<FloatRect> destinationRect;
    decoder >> destinationRect;
    if (!destinationRect)
        return WTF::nullopt;

    Optional<FloatRect> srcRect;
    decoder >> srcRect;
    if (!srcRect)
        return WTF::nullopt;

    Optional<ImagePaintingOptions> options;
    decoder >> options;
    if (!options)
        return WTF::nullopt;

#if USE(CG)
    NativeImagePtr image = handle->image;
#else
    NativeImagePtr image = nullptr;
#endif
    return DrawNativeImage::create(image, *imageSize, *destinationRect, *srcRect, *options);
}
#endif

class DrawPattern : public DrawingItem {
public:
    static Ref<DrawPattern> create(Image& image, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
    {
        return adoptRef(*new DrawPattern(image, destRect, tileRect, patternTransform, phase, spacing, options));
    }

    WEBCORE_EXPORT virtual ~DrawPattern();

    const Image& image() const { return m_image.get(); }
    const AffineTransform& patternTransform() const { return m_patternTransform; }
    FloatRect tileRect() const { return m_tileRect; }
    FloatRect destRect() const { return m_destination; }
    FloatPoint phase() const { return m_phase; }
    FloatSize spacing() const { return m_spacing; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<DrawPattern>> decode(Decoder&);

private:
    WEBCORE_EXPORT DrawPattern(Image&, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& = { });

    void apply(GraphicsContext&) const override;

    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_destination; }

    mutable Ref<Image> m_image; // FIXME: Drawing images can cause their animations to progress. This shouldn't have to be mutable.
    AffineTransform m_patternTransform;
    FloatRect m_tileRect;
    FloatRect m_destination;
    FloatPoint m_phase;
    FloatSize m_spacing;
    ImagePaintingOptions m_options;
};

template<class Encoder>
void DrawPattern::encode(Encoder& encoder) const
{
    ImageHandle imageHandle;
    imageHandle.image = m_image.ptr();
    encoder << imageHandle;
    encoder << m_patternTransform;
    encoder << m_tileRect;
    encoder << m_destination;
    encoder << m_phase;
    encoder << m_spacing;
    encoder << m_options;
}

template<class Decoder>
Optional<Ref<DrawPattern>> DrawPattern::decode(Decoder& decoder)
{
    Optional<ImageHandle> imageHandle;
    decoder >> imageHandle;
    if (!imageHandle)
        return WTF::nullopt;

    Optional<AffineTransform> patternTransform;
    decoder >> patternTransform;
    if (!patternTransform)
        return WTF::nullopt;

    Optional<FloatRect> tileRect;
    decoder >> tileRect;
    if (!tileRect)
        return WTF::nullopt;

    Optional<FloatRect> destination;
    decoder >> destination;
    if (!destination)
        return WTF::nullopt;

    Optional<FloatPoint> phase;
    decoder >> phase;
    if (!phase)
        return WTF::nullopt;

    Optional<FloatSize> spacing;
    decoder >> spacing;
    if (!spacing)
        return WTF::nullopt;

    Optional<ImagePaintingOptions> options;
    decoder >> options;
    if (!options)
        return WTF::nullopt;

    return DrawPattern::create(*imageHandle->image, *destination, *tileRect, *patternTransform, *phase, *spacing, *options);
}

// Is DrawingItem because the size of the transparency layer is implicitly the clip bounds.
class BeginTransparencyLayer : public DrawingItem {
public:
    static Ref<BeginTransparencyLayer> create(float opacity)
    {
        return adoptRef(*new BeginTransparencyLayer(opacity));
    }

    WEBCORE_EXPORT virtual ~BeginTransparencyLayer();

    float opacity() const { return m_opacity; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<BeginTransparencyLayer>> decode(Decoder&);

private:
    WEBCORE_EXPORT BeginTransparencyLayer(float opacity);

    void apply(GraphicsContext&) const override;

    float m_opacity;
};

template<class Encoder>
void BeginTransparencyLayer::encode(Encoder& encoder) const
{
    encoder << m_opacity;
}

template<class Decoder>
Optional<Ref<BeginTransparencyLayer>> BeginTransparencyLayer::decode(Decoder& decoder)
{
    Optional<float> opacity;
    decoder >> opacity;
    if (!opacity)
        return WTF::nullopt;

    return BeginTransparencyLayer::create(*opacity);
}

class EndTransparencyLayer : public DrawingItem {
public:
    static Ref<EndTransparencyLayer> create()
    {
        return adoptRef(*new EndTransparencyLayer);
    }

    WEBCORE_EXPORT virtual ~EndTransparencyLayer();

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<EndTransparencyLayer>> decode(Decoder&);

private:
    WEBCORE_EXPORT EndTransparencyLayer();

    void apply(GraphicsContext&) const override;
};

template<class Encoder>
void EndTransparencyLayer::encode(Encoder&) const
{
}

template<class Decoder>
Optional<Ref<EndTransparencyLayer>> EndTransparencyLayer::decode(Decoder&)
{
    return EndTransparencyLayer::create();
}

class DrawRect : public DrawingItem {
public:
    static Ref<DrawRect> create(const FloatRect& rect, float borderThickness)
    {
        return adoptRef(*new DrawRect(rect, borderThickness));
    }

    WEBCORE_EXPORT virtual ~DrawRect();

    FloatRect rect() const { return m_rect; }
    float borderThickness() const { return m_borderThickness; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<DrawRect>> decode(Decoder&);

private:
    WEBCORE_EXPORT DrawRect(const FloatRect&, float borderThickness);

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_rect; }

    FloatRect m_rect;
    float m_borderThickness;
};

template<class Encoder>
void DrawRect::encode(Encoder& encoder) const
{
    encoder << m_rect;
    encoder << m_borderThickness;
}

template<class Decoder>
Optional<Ref<DrawRect>> DrawRect::decode(Decoder& decoder)
{
    Optional<FloatRect> rect;
    decoder >> rect;
    if (!rect)
        return WTF::nullopt;

    Optional<float> borderThickness;
    decoder >> borderThickness;
    if (!borderThickness)
        return WTF::nullopt;

    return DrawRect::create(*rect, *borderThickness);
}

class DrawLine : public DrawingItem {
public:
    static Ref<DrawLine> create(const FloatPoint& point1, const FloatPoint& point2)
    {
        return adoptRef(*new DrawLine(point1, point2));
    }

    WEBCORE_EXPORT virtual ~DrawLine();

    FloatPoint point1() const { return m_point1; }
    FloatPoint point2() const { return m_point2; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<DrawLine>> decode(Decoder&);

private:
    WEBCORE_EXPORT DrawLine(const FloatPoint&, const FloatPoint&);

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override;

    FloatPoint m_point1;
    FloatPoint m_point2;
};

template<class Encoder>
void DrawLine::encode(Encoder& encoder) const
{
    encoder << m_point1;
    encoder << m_point2;
}

template<class Decoder>
Optional<Ref<DrawLine>> DrawLine::decode(Decoder& decoder)
{
    Optional<FloatPoint> point1;
    decoder >> point1;
    if (!point1)
        return WTF::nullopt;

    Optional<FloatPoint> point2;
    decoder >> point2;
    if (!point2)
        return WTF::nullopt;

    return DrawLine::create(*point1, *point2);
}

class DrawLinesForText : public DrawingItem {
public:
    static Ref<DrawLinesForText> create(const FloatPoint& blockLocation, const FloatSize& localAnchor, float thickness, const DashArray& widths, bool printing, bool doubleLines)
    {
        return adoptRef(*new DrawLinesForText(blockLocation, localAnchor, thickness, widths, printing, doubleLines));
    }

    WEBCORE_EXPORT virtual ~DrawLinesForText();

    void setBlockLocation(const FloatPoint& blockLocation) { m_blockLocation = blockLocation; }
    const FloatPoint& blockLocation() const { return m_blockLocation; }
    const FloatSize& localAnchor() const { return m_localAnchor; }
    FloatPoint point() const { return m_blockLocation + m_localAnchor; }
    float thickness() const { return m_thickness; }
    const DashArray& widths() const { return m_widths; }
    bool isPrinting() const { return m_printing; }
    bool doubleLines() const { return m_doubleLines; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<DrawLinesForText>> decode(Decoder&);

private:
    WEBCORE_EXPORT DrawLinesForText(const FloatPoint& blockLocation, const FloatSize& localAnchor, float thickness, const DashArray& widths, bool printing, bool doubleLines);

    void apply(GraphicsContext&) const override;

    Optional<FloatRect> localBounds(const GraphicsContext&) const override;

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
Optional<Ref<DrawLinesForText>> DrawLinesForText::decode(Decoder& decoder)
{
    Optional<FloatPoint> blockLocation;
    decoder >> blockLocation;
    if (!blockLocation)
        return WTF::nullopt;

    Optional<FloatSize> localAnchor;
    decoder >> localAnchor;
    if (!localAnchor)
        return WTF::nullopt;

    Optional<DashArray> widths;
    decoder >> widths;
    if (!widths)
        return WTF::nullopt;

    Optional<float> thickness;
    decoder >> thickness;
    if (!thickness)
        return WTF::nullopt;

    Optional<bool> printing;
    decoder >> printing;
    if (!printing)
        return WTF::nullopt;

    Optional<bool> doubleLines;
    decoder >> doubleLines;
    if (!doubleLines)
        return WTF::nullopt;

    return DrawLinesForText::create(*blockLocation, *localAnchor, *thickness, *widths, *printing, *doubleLines);
}

class DrawDotsForDocumentMarker : public DrawingItem {
public:
    static Ref<DrawDotsForDocumentMarker> create(const FloatRect& rect, DocumentMarkerLineStyle style)
    {
        return adoptRef(*new DrawDotsForDocumentMarker(rect, style));
    }

    WEBCORE_EXPORT virtual ~DrawDotsForDocumentMarker();

    FloatRect rect() const { return m_rect; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<DrawDotsForDocumentMarker>> decode(Decoder&);

private:
    WEBCORE_EXPORT DrawDotsForDocumentMarker(const FloatRect&, DocumentMarkerLineStyle);

    void apply(GraphicsContext&) const override;

    Optional<FloatRect> localBounds(const GraphicsContext&) const override;

    FloatRect m_rect;
    DocumentMarkerLineStyle m_style;
};

template<class Encoder>
void DrawDotsForDocumentMarker::encode(Encoder& encoder) const
{
    encoder << m_rect;
    encoder << m_style;
}

template<class Decoder>
Optional<Ref<DrawDotsForDocumentMarker>> DrawDotsForDocumentMarker::decode(Decoder& decoder)
{
    Optional<FloatRect> rect;
    decoder >> rect;
    if (!rect)
        return WTF::nullopt;

    Optional<DocumentMarkerLineStyle> style;
    decoder >> style;
    if (!style)
        return WTF::nullopt;

    return DrawDotsForDocumentMarker::create(*rect, *style);
}

class DrawEllipse : public DrawingItem {
public:
    static Ref<DrawEllipse> create(const FloatRect& rect)
    {
        return adoptRef(*new DrawEllipse(rect));
    }

    WEBCORE_EXPORT ~DrawEllipse();

    FloatRect rect() const { return m_rect; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<DrawEllipse>> decode(Decoder&);

private:
    WEBCORE_EXPORT DrawEllipse(const FloatRect&);

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_rect; }

    FloatRect m_rect;
};

template<class Encoder>
void DrawEllipse::encode(Encoder& encoder) const
{
    encoder << m_rect;
}

template<class Decoder>
Optional<Ref<DrawEllipse>> DrawEllipse::decode(Decoder& decoder)
{
    Optional<FloatRect> rect;
    decoder >> rect;
    if (!rect)
        return WTF::nullopt;

    return DrawEllipse::create(*rect);
}

class DrawPath : public DrawingItem {
public:
    static Ref<DrawPath> create(const Path& path)
    {
        return adoptRef(*new DrawPath(path));
    }

    WEBCORE_EXPORT virtual ~DrawPath();

    const Path& path() const { return m_path; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<DrawPath>> decode(Decoder&);

private:
    WEBCORE_EXPORT DrawPath(const Path&);

    void apply(GraphicsContext&) const override;

    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_path.fastBoundingRect(); }

    const Path m_path;
};

template<class Encoder>
void DrawPath::encode(Encoder& encoder) const
{
    encoder << m_path;
}

template<class Decoder>
Optional<Ref<DrawPath>> DrawPath::decode(Decoder& decoder)
{
    Optional<Path> path;
    decoder >> path;
    if (!path)
        return WTF::nullopt;

    return DrawPath::create(*path);
}

class DrawFocusRingPath : public DrawingItem {
public:
    static Ref<DrawFocusRingPath> create(const Path& path, float width, float offset, const Color& color)
    {
        return adoptRef(*new DrawFocusRingPath(path, width, offset, color));
    }

    WEBCORE_EXPORT virtual ~DrawFocusRingPath();

    const Path& path() const { return m_path; }
    float width() const { return m_width; }
    float offset() const { return m_offset; }
    const Color& color() const { return m_color; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<DrawFocusRingPath>> decode(Decoder&);

private:
    WEBCORE_EXPORT DrawFocusRingPath(const Path&, float width, float offset, const Color&);

    void apply(GraphicsContext&) const override;

    Optional<FloatRect> localBounds(const GraphicsContext&) const override;

    const Path m_path;
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
Optional<Ref<DrawFocusRingPath>> DrawFocusRingPath::decode(Decoder& decoder)
{
    Optional<Path> path;
    decoder >> path;
    if (!path)
        return WTF::nullopt;

    Optional<float> width;
    decoder >> width;
    if (!width)
        return WTF::nullopt;

    Optional<float> offset;
    decoder >> offset;
    if (!offset)
        return WTF::nullopt;

    Optional<Color> color;
    decoder >> color;
    if (!color)
        return WTF::nullopt;

    return DrawFocusRingPath::create(*path, *width, *offset, *color);
}

class DrawFocusRingRects : public DrawingItem {
public:
    static Ref<DrawFocusRingRects> create(const Vector<FloatRect>& rects, float width, float offset, const Color& color)
    {
        return adoptRef(*new DrawFocusRingRects(rects, width, offset, color));
    }

    WEBCORE_EXPORT virtual ~DrawFocusRingRects();

    const Vector<FloatRect> rects() const { return m_rects; }
    float width() const { return m_width; }
    float offset() const { return m_offset; }
    const Color& color() const { return m_color; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<DrawFocusRingRects>> decode(Decoder&);

private:
    WEBCORE_EXPORT DrawFocusRingRects(const Vector<FloatRect>&, float width, float offset, const Color&);

    void apply(GraphicsContext&) const override;

    Optional<FloatRect> localBounds(const GraphicsContext&) const override;

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
Optional<Ref<DrawFocusRingRects>> DrawFocusRingRects::decode(Decoder& decoder)
{
    Optional<Vector<FloatRect>> rects;
    decoder >> rects;
    if (!rects)
        return WTF::nullopt;

    Optional<float> width;
    decoder >> width;
    if (!width)
        return WTF::nullopt;

    Optional<float> offset;
    decoder >> offset;
    if (!offset)
        return WTF::nullopt;

    Optional<Color> color;
    decoder >> color;
    if (!color)
        return WTF::nullopt;

    return DrawFocusRingRects::create(*rects, *width, *offset, *color);
}

class FillRect : public DrawingItem {
public:
    static Ref<FillRect> create(const FloatRect& rect)
    {
        return adoptRef(*new FillRect(rect));
    }

    WEBCORE_EXPORT virtual ~FillRect();

    FloatRect rect() const { return m_rect; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<FillRect>> decode(Decoder&);

private:
    WEBCORE_EXPORT FillRect(const FloatRect&);

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_rect; }

    FloatRect m_rect;
};

template<class Encoder>
void FillRect::encode(Encoder& encoder) const
{
    encoder << m_rect;
}

template<class Decoder>
Optional<Ref<FillRect>> FillRect::decode(Decoder& decoder)
{
    Optional<FloatRect> rect;
    decoder >> rect;
    if (!rect)
        return WTF::nullopt;

    return FillRect::create(*rect);
}

// FIXME: Make these inherit from FillRect proper.
class FillRectWithColor : public DrawingItem {
public:
    static Ref<FillRectWithColor> create(const FloatRect& rect, const Color& color)
    {
        return adoptRef(*new FillRectWithColor(rect, color));
    }

    WEBCORE_EXPORT virtual ~FillRectWithColor();

    FloatRect rect() const { return m_rect; }
    const Color& color() const { return m_color; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<FillRectWithColor>> decode(Decoder&);

private:
    WEBCORE_EXPORT FillRectWithColor(const FloatRect&, const Color&);

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_rect; }

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
Optional<Ref<FillRectWithColor>> FillRectWithColor::decode(Decoder& decoder)
{
    Optional<FloatRect> rect;
    decoder >> rect;
    if (!rect)
        return WTF::nullopt;

    Optional<Color> color;
    decoder >> color;
    if (!color)
        return WTF::nullopt;

    return FillRectWithColor::create(*rect, *color);
}

class FillRectWithGradient : public DrawingItem {
public:
    static Ref<FillRectWithGradient> create(const FloatRect& rect, Gradient& gradient)
    {
        return adoptRef(*new FillRectWithGradient(rect, gradient));
    }

    WEBCORE_EXPORT virtual ~FillRectWithGradient();

    FloatRect rect() const { return m_rect; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<FillRectWithGradient>> decode(Decoder&);

private:
    WEBCORE_EXPORT FillRectWithGradient(const FloatRect&, Gradient&);

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_rect; }

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
Optional<Ref<FillRectWithGradient>> FillRectWithGradient::decode(Decoder& decoder)
{
    Optional<FloatRect> rect;
    decoder >> rect;
    if (!rect)
        return WTF::nullopt;

    auto gradient = Gradient::decode(decoder);
    if (!gradient)
        return WTF::nullopt;

    return FillRectWithGradient::create(*rect, gradient->get());
}

class FillCompositedRect : public DrawingItem {
public:
    static Ref<FillCompositedRect> create(const FloatRect& rect, const Color& color, CompositeOperator op, BlendMode blendMode)
    {
        return adoptRef(*new FillCompositedRect(rect, color, op, blendMode));
    }

    WEBCORE_EXPORT virtual ~FillCompositedRect();

    FloatRect rect() const { return m_rect; }
    const Color& color() const { return m_color; }
    CompositeOperator compositeOperator() const { return m_op; }
    BlendMode blendMode() const { return m_blendMode; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<FillCompositedRect>> decode(Decoder&);

private:
    WEBCORE_EXPORT FillCompositedRect(const FloatRect&, const Color&, CompositeOperator, BlendMode);

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_rect; }

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
Optional<Ref<FillCompositedRect>> FillCompositedRect::decode(Decoder& decoder)
{
    Optional<FloatRect> rect;
    decoder >> rect;
    if (!rect)
        return WTF::nullopt;

    Optional<Color> color;
    decoder >> color;
    if (!color)
        return WTF::nullopt;

    Optional<CompositeOperator> op;
    decoder >> op;
    if (!op)
        return WTF::nullopt;

    Optional<BlendMode> blendMode;
    decoder >> blendMode;
    if (!blendMode)
        return WTF::nullopt;

    return FillCompositedRect::create(*rect, *color, *op, *blendMode);
}

class FillRoundedRect : public DrawingItem {
public:
    static Ref<FillRoundedRect> create(const FloatRoundedRect& rect, const Color& color, BlendMode blendMode)
    {
        return adoptRef(*new FillRoundedRect(rect, color, blendMode));
    }

    WEBCORE_EXPORT virtual ~FillRoundedRect();

    const FloatRoundedRect& roundedRect() const { return m_rect; }
    const Color& color() const { return m_color; }
    BlendMode blendMode() const { return m_blendMode; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<FillRoundedRect>> decode(Decoder&);

private:
    WEBCORE_EXPORT FillRoundedRect(const FloatRoundedRect&, const Color&, BlendMode);

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_rect.rect(); }

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
Optional<Ref<FillRoundedRect>> FillRoundedRect::decode(Decoder& decoder)
{
    Optional<FloatRoundedRect> rect;
    decoder >> rect;
    if (!rect)
        return WTF::nullopt;

    Optional<Color> color;
    decoder >> color;
    if (!color)
        return WTF::nullopt;

    Optional<BlendMode> blendMode;
    decoder >> blendMode;
    if (!blendMode)
        return WTF::nullopt;

    return FillRoundedRect::create(*rect, *color, *blendMode);
}

class FillRectWithRoundedHole : public DrawingItem {
public:
    static Ref<FillRectWithRoundedHole> create(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
    {
        return adoptRef(*new FillRectWithRoundedHole(rect, roundedHoleRect, color));
    }

    WEBCORE_EXPORT virtual ~FillRectWithRoundedHole();

    const FloatRect& rect() const { return m_rect; }
    const FloatRoundedRect& roundedHoleRect() const { return m_roundedHoleRect; }
    const Color& color() const { return m_color; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<FillRectWithRoundedHole>> decode(Decoder&);

private:
    WEBCORE_EXPORT FillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect&, const Color&);

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_rect; }

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
Optional<Ref<FillRectWithRoundedHole>> FillRectWithRoundedHole::decode(Decoder& decoder)
{
    Optional<FloatRect> rect;
    decoder >> rect;
    if (!rect)
        return WTF::nullopt;

    Optional<FloatRoundedRect> roundedHoleRect;
    decoder >> roundedHoleRect;
    if (!roundedHoleRect)
        return WTF::nullopt;

    Optional<Color> color;
    decoder >> color;
    if (!color)
        return WTF::nullopt;

    return FillRectWithRoundedHole::create(*rect, *roundedHoleRect, *color);
}

class FillPath : public DrawingItem {
public:
    static Ref<FillPath> create(const Path& path)
    {
        return adoptRef(*new FillPath(path));
    }

    WEBCORE_EXPORT virtual ~FillPath();

    const Path& path() const { return m_path; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<FillPath>> decode(Decoder&);

private:
    WEBCORE_EXPORT FillPath(const Path&);

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_path.fastBoundingRect(); }

    const Path m_path;
};

template<class Encoder>
void FillPath::encode(Encoder& encoder) const
{
    encoder << m_path;
}

template<class Decoder>
Optional<Ref<FillPath>> FillPath::decode(Decoder& decoder)
{
    Optional<Path> path;
    decoder >> path;
    if (!path)
        return WTF::nullopt;

    return FillPath::create(*path);
}

class FillEllipse : public DrawingItem {
public:
    static Ref<FillEllipse> create(const FloatRect& rect)
    {
        return adoptRef(*new FillEllipse(rect));
    }

    WEBCORE_EXPORT virtual ~FillEllipse();

    FloatRect rect() const { return m_rect; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<FillEllipse>> decode(Decoder&);

private:
    WEBCORE_EXPORT FillEllipse(const FloatRect&);

    void apply(GraphicsContext&) const override;

    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_rect; }

    FloatRect m_rect;
};

template<class Encoder>
void FillEllipse::encode(Encoder& encoder) const
{
    encoder << m_rect;
}

template<class Decoder>
Optional<Ref<FillEllipse>> FillEllipse::decode(Decoder& decoder)
{
    Optional<FloatRect> rect;
    decoder >> rect;
    if (!rect)
        return WTF::nullopt;

    return FillEllipse::create(*rect);
}

class PutImageData : public DrawingItem {
public:
    static Ref<PutImageData> create(AlphaPremultiplication inputFormat, const ImageData& imageData, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
    {
        return adoptRef(*new PutImageData(inputFormat, imageData, srcRect, destPoint, destFormat));
    }
    static Ref<PutImageData> create(AlphaPremultiplication inputFormat, Ref<ImageData>&& imageData, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
    {
        return adoptRef(*new PutImageData(inputFormat, WTFMove(imageData), srcRect, destPoint, destFormat));
    }

    WEBCORE_EXPORT virtual ~PutImageData();

    AlphaPremultiplication inputFormat() const { return m_inputFormat; }
    ImageData& imageData() const { return m_imageData; }
    IntRect srcRect() const { return m_srcRect; }
    IntPoint destPoint() const { return m_destPoint; }
    AlphaPremultiplication destFormat() const { return m_destFormat; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<PutImageData>> decode(Decoder&);

private:
    WEBCORE_EXPORT PutImageData(AlphaPremultiplication inputFormat, const ImageData&, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat);
    WEBCORE_EXPORT PutImageData(AlphaPremultiplication inputFormat, Ref<ImageData>&&, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat);

    void apply(GraphicsContext&) const override;

    Optional<FloatRect> globalBounds() const override { return FloatRect(m_destPoint, m_srcRect.size()); }

    IntRect m_srcRect;
    IntPoint m_destPoint;
    Ref<ImageData> m_imageData;
    AlphaPremultiplication m_inputFormat;
    AlphaPremultiplication m_destFormat;
};

template<class Encoder>
void PutImageData::encode(Encoder& encoder) const
{
    encoder << m_inputFormat;
    encoder << m_imageData;
    encoder << m_srcRect;
    encoder << m_destPoint;
    encoder << m_destFormat;
}

template<class Decoder>
Optional<Ref<PutImageData>> PutImageData::decode(Decoder& decoder)
{
    Optional<AlphaPremultiplication> inputFormat;
    Optional<Ref<ImageData>> imageData;
    Optional<IntRect> srcRect;
    Optional<IntPoint> destPoint;
    Optional<AlphaPremultiplication> destFormat;

    decoder >> inputFormat;
    if (!inputFormat)
        return WTF::nullopt;

    decoder >> imageData;
    if (!imageData)
        return WTF::nullopt;

    decoder >> srcRect;
    if (!srcRect)
        return WTF::nullopt;

    decoder >> destPoint;
    if (!destPoint)
        return WTF::nullopt;

    decoder >> destFormat;
    if (!destFormat)
        return WTF::nullopt;

    return PutImageData::create(*inputFormat, WTFMove(*imageData), *srcRect, *destPoint, *destFormat);
}

class StrokeRect : public DrawingItem {
public:
    static Ref<StrokeRect> create(const FloatRect& rect, float lineWidth)
    {
        return adoptRef(*new StrokeRect(rect, lineWidth));
    }

    WEBCORE_EXPORT virtual ~StrokeRect();

    FloatRect rect() const { return m_rect; }
    float lineWidth() const { return m_lineWidth; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<StrokeRect>> decode(Decoder&);

private:
    WEBCORE_EXPORT StrokeRect(const FloatRect&, float);

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override;

    FloatRect m_rect;
    float m_lineWidth;
};

template<class Encoder>
void StrokeRect::encode(Encoder& encoder) const
{
    encoder << m_rect;
    encoder << m_lineWidth;
}

template<class Decoder>
Optional<Ref<StrokeRect>> StrokeRect::decode(Decoder& decoder)
{
    Optional<FloatRect> rect;
    decoder >> rect;
    if (!rect)
        return WTF::nullopt;

    Optional<float> lineWidth;
    decoder >> lineWidth;
    if (!lineWidth)
        return WTF::nullopt;

    return StrokeRect::create(*rect, *lineWidth);
}

class StrokePath : public DrawingItem {
public:
    static Ref<StrokePath> create(const Path& path)
    {
        return adoptRef(*new StrokePath(path));
    }

    WEBCORE_EXPORT virtual ~StrokePath();

    const Path& path() const { return m_path; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<StrokePath>> decode(Decoder&);

private:
    WEBCORE_EXPORT StrokePath(const Path&);

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override;

    mutable Path m_path;
};

template<class Encoder>
void StrokePath::encode(Encoder& encoder) const
{
    encoder << m_path;
}

template<class Decoder>
Optional<Ref<StrokePath>> StrokePath::decode(Decoder& decoder)
{
    Optional<Path> path;
    decoder >> path;
    if (!path)
        return WTF::nullopt;

    return StrokePath::create(*path);
}

class StrokeEllipse : public DrawingItem {
public:
    static Ref<StrokeEllipse> create(const FloatRect& rect)
    {
        return adoptRef(*new StrokeEllipse(rect));
    }

    WEBCORE_EXPORT ~StrokeEllipse();

    FloatRect rect() const { return m_rect; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<StrokeEllipse>> decode(Decoder&);

private:
    WEBCORE_EXPORT StrokeEllipse(const FloatRect&);

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override;

    FloatRect m_rect;
};

template<class Encoder>
void StrokeEllipse::encode(Encoder& encoder) const
{
    encoder << m_rect;
}

template<class Decoder>
Optional<Ref<StrokeEllipse>> StrokeEllipse::decode(Decoder& decoder)
{
    Optional<FloatRect> rect;
    decoder >> rect;
    if (!rect)
        return WTF::nullopt;

    return StrokeEllipse::create(*rect);
}

class ClearRect : public DrawingItem {
public:
    static Ref<ClearRect> create(const FloatRect& rect)
    {
        return adoptRef(*new ClearRect(rect));
    }

    WEBCORE_EXPORT virtual ~ClearRect();

    FloatRect rect() const { return m_rect; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<ClearRect>> decode(Decoder&);

private:
    WEBCORE_EXPORT ClearRect(const FloatRect&);

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_rect; }

    FloatRect m_rect;
};

template<class Encoder>
void ClearRect::encode(Encoder& encoder) const
{
    encoder << m_rect;
}

template<class Decoder>
Optional<Ref<ClearRect>> ClearRect::decode(Decoder& decoder)
{
    Optional<FloatRect> rect;
    decoder >> rect;
    if (!rect)
        return WTF::nullopt;

    return ClearRect::create(*rect);
}

#if USE(CG)
class ApplyStrokePattern : public Item {
public:
    static Ref<ApplyStrokePattern> create()
    {
        return adoptRef(*new ApplyStrokePattern);
    }

    WEBCORE_EXPORT virtual ~ApplyStrokePattern();

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<ApplyStrokePattern>> decode(Decoder&);

private:
    WEBCORE_EXPORT ApplyStrokePattern();

    void apply(GraphicsContext&) const override;
};

template<class Encoder>
void ApplyStrokePattern::encode(Encoder&) const
{
}

template<class Decoder>
Optional<Ref<ApplyStrokePattern>> ApplyStrokePattern::decode(Decoder&)
{
    return ApplyStrokePattern::create();
}

class ApplyFillPattern : public Item {
public:
    static Ref<ApplyFillPattern> create()
    {
        return adoptRef(*new ApplyFillPattern);
    }

    WEBCORE_EXPORT virtual ~ApplyFillPattern();

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<ApplyFillPattern>> decode(Decoder&);

private:
    WEBCORE_EXPORT ApplyFillPattern();

    void apply(GraphicsContext&) const override;
};

template<class Encoder>
void ApplyFillPattern::encode(Encoder&) const
{
}

template<class Decoder>
Optional<Ref<ApplyFillPattern>> ApplyFillPattern::decode(Decoder&)
{
    return ApplyFillPattern::create();
}
#endif

class ApplyDeviceScaleFactor : public Item {
public:
    static Ref<ApplyDeviceScaleFactor> create(float scaleFactor)
    {
        return adoptRef(*new ApplyDeviceScaleFactor(scaleFactor));
    }

    WEBCORE_EXPORT virtual ~ApplyDeviceScaleFactor();

    float scaleFactor() const { return m_scaleFactor; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<ApplyDeviceScaleFactor>> decode(Decoder&);

private:
    WEBCORE_EXPORT ApplyDeviceScaleFactor(float scaleFactor);

    void apply(GraphicsContext&) const override;

    float m_scaleFactor;
};

template<class Encoder>
void ApplyDeviceScaleFactor::encode(Encoder& encoder) const
{
    encoder << m_scaleFactor;
}

template<class Decoder>
Optional<Ref<ApplyDeviceScaleFactor>> ApplyDeviceScaleFactor::decode(Decoder& decoder)
{
    Optional<float> scaleFactor;
    decoder >> scaleFactor;
    if (!scaleFactor)
        return WTF::nullopt;

    return ApplyDeviceScaleFactor::create(*scaleFactor);
}


WTF::TextStream& operator<<(WTF::TextStream&, const Item&);

template<class Encoder>
void Item::encode(Encoder& encoder) const
{
    encoder << m_type;

    switch (m_type) {
    case ItemType::Save:
        encoder << downcast<Save>(*this);
        break;
    case ItemType::Restore:
        encoder << downcast<Restore>(*this);
        break;
    case ItemType::Translate:
        encoder << downcast<Translate>(*this);
        break;
    case ItemType::Rotate:
        encoder << downcast<Rotate>(*this);
        break;
    case ItemType::Scale:
        encoder << downcast<Scale>(*this);
        break;
    case ItemType::SetCTM:
        encoder << downcast<SetCTM>(*this);
        break;
    case ItemType::ConcatenateCTM:
        encoder << downcast<ConcatenateCTM>(*this);
        break;
    case ItemType::SetState:
        encoder << downcast<SetState>(*this);
        break;
    case ItemType::SetLineCap:
        encoder << downcast<SetLineCap>(*this);
        break;
    case ItemType::SetLineDash:
        encoder << downcast<SetLineDash>(*this);
        break;
    case ItemType::SetLineJoin:
        encoder << downcast<SetLineJoin>(*this);
        break;
    case ItemType::SetMiterLimit:
        encoder << downcast<SetMiterLimit>(*this);
        break;
    case ItemType::ClearShadow:
        encoder << downcast<ClearShadow>(*this);
        break;
    case ItemType::Clip:
        encoder << downcast<Clip>(*this);
        break;
    case ItemType::ClipOut:
        encoder << downcast<ClipOut>(*this);
        break;
    case ItemType::ClipOutToPath:
        encoder << downcast<ClipOutToPath>(*this);
        break;
    case ItemType::ClipPath:
        encoder << downcast<ClipPath>(*this);
        break;
    case ItemType::ClipToDrawingCommands:
        encoder << downcast<ClipToDrawingCommands>(*this);
        break;
    case ItemType::DrawGlyphs:
        encoder << downcast<DrawGlyphs>(*this);
        break;
    case ItemType::DrawImage:
        encoder << downcast<DrawImage>(*this);
        break;
    case ItemType::DrawTiledImage:
        encoder << downcast<DrawTiledImage>(*this);
        break;
    case ItemType::DrawTiledScaledImage:
        encoder << downcast<DrawTiledScaledImage>(*this);
        break;
#if USE(CG) || USE(CAIRO) || USE(DIRECT2D)
    case ItemType::DrawNativeImage:
        encoder << downcast<DrawNativeImage>(*this);
        break;
#endif
    case ItemType::DrawPattern:
        encoder << downcast<DrawPattern>(*this);
        break;
    case ItemType::DrawRect:
        encoder << downcast<DrawRect>(*this);
        break;
    case ItemType::DrawLine:
        encoder << downcast<DrawLine>(*this);
        break;
    case ItemType::DrawLinesForText:
        encoder << downcast<DrawLinesForText>(*this);
        break;
    case ItemType::DrawDotsForDocumentMarker:
        encoder << downcast<DrawDotsForDocumentMarker>(*this);
        break;
    case ItemType::DrawEllipse:
        encoder << downcast<DrawEllipse>(*this);
        break;
    case ItemType::DrawPath:
        encoder << downcast<DrawPath>(*this);
        break;
    case ItemType::DrawFocusRingPath:
        encoder << downcast<DrawFocusRingPath>(*this);
        break;
    case ItemType::DrawFocusRingRects:
        encoder << downcast<DrawFocusRingRects>(*this);
        break;
    case ItemType::FillRect:
        encoder << downcast<FillRect>(*this);
        break;
    case ItemType::FillRectWithColor:
        encoder << downcast<FillRectWithColor>(*this);
        break;
    case ItemType::FillRectWithGradient:
        encoder << downcast<FillRectWithGradient>(*this);
        break;
    case ItemType::FillCompositedRect:
        encoder << downcast<FillCompositedRect>(*this);
        break;
    case ItemType::FillRoundedRect:
        encoder << downcast<FillRoundedRect>(*this);
        break;
    case ItemType::FillRectWithRoundedHole:
        encoder << downcast<FillRectWithRoundedHole>(*this);
        break;
    case ItemType::FillPath:
        encoder << downcast<FillPath>(*this);
        break;
    case ItemType::FillEllipse:
        encoder << downcast<FillEllipse>(*this);
        break;
    case ItemType::PutImageData:
        encoder << downcast<PutImageData>(*this);
        break;
    case ItemType::StrokeRect:
        encoder << downcast<StrokeRect>(*this);
        break;
    case ItemType::StrokePath:
        encoder << downcast<StrokePath>(*this);
        break;
    case ItemType::StrokeEllipse:
        encoder << downcast<StrokeEllipse>(*this);
        break;
    case ItemType::ClearRect:
        encoder << downcast<ClearRect>(*this);
        break;
    case ItemType::BeginTransparencyLayer:
        encoder << downcast<BeginTransparencyLayer>(*this);
        break;
    case ItemType::EndTransparencyLayer:
        encoder << downcast<EndTransparencyLayer>(*this);
        break;
#if USE(CG)
    case ItemType::ApplyStrokePattern:
        encoder << downcast<ApplyStrokePattern>(*this);
        break;
    case ItemType::ApplyFillPattern:
        encoder << downcast<ApplyFillPattern>(*this);
        break;
#endif
    case ItemType::ApplyDeviceScaleFactor:
        encoder << downcast<ApplyDeviceScaleFactor>(*this);
        break;
    }
}

template<class Decoder>
Optional<Ref<Item>> Item::decode(Decoder& decoder)
{
    Optional<ItemType> itemType;
    decoder >> itemType;
    if (!itemType)
        return WTF::nullopt;

    switch (*itemType) {
    case ItemType::Save:
        if (auto item = Save::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::Restore:
        if (auto item = Restore::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::Translate:
        if (auto item = Translate::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::Rotate:
        if (auto item = Rotate::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::Scale:
        if (auto item = Scale::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::SetCTM:
        if (auto item = SetCTM::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::ConcatenateCTM:
        if (auto item = ConcatenateCTM::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::SetState:
        if (auto item = SetState::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::SetLineCap:
        if (auto item = SetLineCap::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::SetLineDash:
        if (auto item = SetLineDash::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::SetLineJoin:
        if (auto item = SetLineJoin::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::SetMiterLimit:
        if (auto item = SetMiterLimit::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::ClearShadow:
        if (auto item = ClearShadow::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::Clip:
        if (auto item = Clip::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::ClipOut:
        if (auto item = ClipOut::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::ClipOutToPath:
        if (auto item = ClipOutToPath::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::ClipPath:
        if (auto item = ClipPath::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::ClipToDrawingCommands:
        if (auto item = ClipToDrawingCommands::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::DrawGlyphs:
        if (auto item = DrawGlyphs::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::DrawImage:
        if (auto item = DrawImage::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::DrawTiledImage:
        if (auto item = DrawTiledImage::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::DrawTiledScaledImage:
        if (auto item = DrawTiledScaledImage::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
#if USE(CG) || USE(CAIRO) || USE(DIRECT2D)
    case ItemType::DrawNativeImage:
        if (auto item = DrawNativeImage::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
#endif
    case ItemType::DrawPattern:
        if (auto item = DrawPattern::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::DrawRect:
        if (auto item = DrawRect::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::DrawLine:
        if (auto item = DrawLine::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::DrawLinesForText:
        if (auto item = DrawLinesForText::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::DrawDotsForDocumentMarker:
        if (auto item = DrawDotsForDocumentMarker::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::DrawEllipse:
        if (auto item = DrawEllipse::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::DrawPath:
        if (auto item = DrawPath::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::DrawFocusRingPath:
        if (auto item = DrawFocusRingPath::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::DrawFocusRingRects:
        if (auto item = DrawFocusRingRects::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::FillRect:
        if (auto item = FillRect::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::FillRectWithColor:
        if (auto item = FillRectWithColor::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::FillRectWithGradient:
        if (auto item = FillRectWithGradient::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::FillCompositedRect:
        if (auto item = FillCompositedRect::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::FillRoundedRect:
        if (auto item = FillRoundedRect::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::FillRectWithRoundedHole:
        if (auto item = FillRectWithRoundedHole::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::FillPath:
        if (auto item = FillPath::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::FillEllipse:
        if (auto item = FillEllipse::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::PutImageData:
        if (auto item = PutImageData::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::StrokeRect:
        if (auto item = StrokeRect::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::StrokePath:
        if (auto item = StrokePath::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::StrokeEllipse:
        if (auto item = StrokeEllipse::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::ClearRect:
        if (auto item = ClearRect::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::BeginTransparencyLayer:
        if (auto item = BeginTransparencyLayer::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::EndTransparencyLayer:
        if (auto item = EndTransparencyLayer::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
#if USE(CG)
    case ItemType::ApplyStrokePattern:
        if (auto item = ApplyStrokePattern::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::ApplyFillPattern:
        if (auto item = ApplyFillPattern::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
#endif
    case ItemType::ApplyDeviceScaleFactor:
        if (auto item = ApplyDeviceScaleFactor::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    }

    return WTF::nullopt;
}

} // namespace DisplayList
} // namespace WebCore


#define SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_DRAWINGITEM(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::DisplayList::ToValueTypeName) \
    static bool isType(const WebCore::DisplayList::Item& object) { return object.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_DRAWINGITEM(DrawingItem, isDrawingItem())

#define SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(ToValueTypeName) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::DisplayList::ToValueTypeName) \
    static bool isType(const WebCore::DisplayList::Item& item) { return item.type() == WebCore::DisplayList::ItemType::ToValueTypeName; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(Save)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(Restore)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(Translate)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(Rotate)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(Scale)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(SetCTM)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(ConcatenateCTM)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(SetState)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(SetLineCap)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(SetLineDash)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(SetLineJoin)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(SetMiterLimit)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(Clip)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(ClipOut)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(ClipOutToPath)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(ClipPath)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(ClipToDrawingCommands)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(DrawGlyphs)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(DrawImage)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(DrawTiledImage)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(DrawTiledScaledImage)
#if USE(CG) || USE(CAIRO)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(DrawNativeImage)
#endif
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(DrawPattern)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(DrawRect)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(DrawLine)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(DrawLinesForText)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(DrawDotsForDocumentMarker)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(DrawEllipse)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(DrawPath)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(DrawFocusRingPath)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(DrawFocusRingRects)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(FillRect)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(FillRectWithColor)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(FillRectWithGradient)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(FillCompositedRect)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(FillRoundedRect)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(FillRectWithRoundedHole)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(FillPath)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(FillEllipse)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(PutImageData)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(StrokeRect)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(StrokePath)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(StrokeEllipse)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(ClearRect)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(BeginTransparencyLayer)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(EndTransparencyLayer)
#if USE(CG)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(ApplyStrokePattern)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(ApplyFillPattern)
#endif
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(ApplyDeviceScaleFactor)
SPECIALIZE_TYPE_TRAITS_DISPLAYLIST_ITEM(ClearShadow)

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
    WebCore::DisplayList::ItemType::SetState,
    WebCore::DisplayList::ItemType::SetLineCap,
    WebCore::DisplayList::ItemType::SetLineDash,
    WebCore::DisplayList::ItemType::SetLineJoin,
    WebCore::DisplayList::ItemType::SetMiterLimit,
    WebCore::DisplayList::ItemType::ClearShadow,
    WebCore::DisplayList::ItemType::Clip,
    WebCore::DisplayList::ItemType::ClipOut,
    WebCore::DisplayList::ItemType::ClipOutToPath,
    WebCore::DisplayList::ItemType::ClipPath,
    WebCore::DisplayList::ItemType::ClipToDrawingCommands,
    WebCore::DisplayList::ItemType::DrawGlyphs,
    WebCore::DisplayList::ItemType::DrawImage,
    WebCore::DisplayList::ItemType::DrawTiledImage,
    WebCore::DisplayList::ItemType::DrawTiledScaledImage,
#if USE(CG) || USE(CAIRO) || USE(DIRECT2D)
    WebCore::DisplayList::ItemType::DrawNativeImage,
#endif
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
    WebCore::DisplayList::ItemType::FillPath,
    WebCore::DisplayList::ItemType::FillEllipse,
    WebCore::DisplayList::ItemType::PutImageData,
    WebCore::DisplayList::ItemType::StrokeRect,
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
