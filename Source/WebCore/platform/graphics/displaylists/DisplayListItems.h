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

#include "FloatPoint.h"
#include "FloatRect.h"
#include "FloatRoundedRect.h"
#include "Font.h"
#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "Image.h"
#include <wtf/RefCounted.h>
#include <wtf/TypeCasts.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

struct ImagePaintingOptions;

namespace DisplayList {

enum class ItemType : uint8_t {
    Save,
    Restore,
    Translate,
    Rotate,
    Scale,
    ConcatenateCTM,
    SetState,
    SetLineCap,
    SetLineDash,
    SetLineJoin,
    SetMiterLimit,
    ClearShadow,
    Clip,
    ClipOut,
    ClipOutToPath,
    ClipPath,
    DrawGlyphs,
    DrawImage,
    DrawTiledImage,
    DrawTiledScaledImage,
#if USE(CG) || USE(CAIRO) || USE(DIRECT2D)
    DrawNativeImage,
#endif
    DrawPattern,
    DrawRect,
    DrawLine,
    DrawLinesForText,
    DrawDotsForDocumentMarker,
    DrawEllipse,
    DrawPath,
    DrawFocusRingPath,
    DrawFocusRingRects,
    FillRect,
    FillRectWithColor,
    FillRectWithGradient,
    FillCompositedRect,
    FillRoundedRect,
    FillRectWithRoundedHole,
    FillPath,
    FillEllipse,
    StrokeRect,
    StrokePath,
    StrokeEllipse,
    ClearRect,
    BeginTransparencyLayer,
    EndTransparencyLayer,
#if USE(CG)
    ApplyStrokePattern, // FIXME: should not be a recorded item.
    ApplyFillPattern, // FIXME: should not be a recorded item.
#endif
    ApplyDeviceScaleFactor,
};

class Item : public RefCounted<Item> {
public:
    Item() = delete;

    WEBCORE_EXPORT Item(ItemType);
    WEBCORE_EXPORT virtual ~Item();

    ItemType type() const
    {
        return m_type;
    }

    virtual void apply(GraphicsContext&) const = 0;

    static constexpr bool isDisplayListItem = true;

    virtual bool isDrawingItem() const { return false; }
    
    // A state item is one preserved by Save/Restore.
    bool isStateItem() const
    {
        return isStateItemType(m_type);
    }

    static bool isStateItemType(ItemType itemType)
    {
        switch (itemType) {
        case ItemType::Translate:
        case ItemType::Rotate:
        case ItemType::Scale:
        case ItemType::ConcatenateCTM:
        case ItemType::SetState:
        case ItemType::SetLineCap:
        case ItemType::SetLineDash:
        case ItemType::SetLineJoin:
        case ItemType::SetMiterLimit:
        case ItemType::ClearShadow:
            return true;
        default:
            return false;
        }
        return false;
    }

#if !defined(NDEBUG) || !LOG_DISABLED
    WTF::CString description() const;
#endif
    static size_t sizeInBytes(const Item&);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<Item>> decode(Decoder&);

private:
    ItemType m_type;
};

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

    // Index in the display list of the corresponding Restore item. 0 if unmatched.
    size_t restoreIndex() const { return m_restoreIndex; }
    void setRestoreIndex(size_t index) { m_restoreIndex = index; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<Save>> decode(Decoder&);

private:
    WEBCORE_EXPORT Save();

    void apply(GraphicsContext&) const override;
    
    size_t m_restoreIndex { 0 };
};

template<class Encoder>
void Save::encode(Encoder& encoder) const
{
    encoder << static_cast<uint64_t>(m_restoreIndex);
}

template<class Decoder>
Optional<Ref<Save>> Save::decode(Decoder& decoder)
{
    Optional<uint64_t> restoreIndex;
    decoder >> restoreIndex;
    if (!restoreIndex)
        return WTF::nullopt;

    // FIXME: Validate restoreIndex? But we don't have the list context here.
    auto save = Save::create();
    save->setRestoreIndex(static_cast<size_t>(*restoreIndex));
    return save;
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
    encoder << m_state.m_changeFlags;

    // FIXME: Encode the other state changes.

    if (m_state.m_changeFlags.contains(GraphicsContextState::StrokeColorChange))
        encoder << m_state.m_state.strokeColor;

    if (m_state.m_changeFlags.contains(GraphicsContextState::FillColorChange))
        encoder << m_state.m_state.fillColor;

    if (m_state.m_changeFlags.contains(GraphicsContextState::AlphaChange))
        encoder << m_state.m_state.alpha;
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

    if (stateChange.m_changeFlags.contains(GraphicsContextState::AlphaChange)) {
        Optional<float> alpha;
        decoder >> alpha;
        if (!alpha)
            return WTF::nullopt;

        stateChange.m_state.alpha = *alpha;
    }

    return SetState::create(stateChange);
}

class SetLineCap : public Item {
public:
    static Ref<SetLineCap> create(LineCap lineCap)
    {
        return adoptRef(*new SetLineCap(lineCap));
    }
    
    LineCap lineCap() const { return m_lineCap; }

private:
    SetLineCap(LineCap lineCap)
        : Item(ItemType::SetLineCap)
        , m_lineCap(lineCap)
    {
    }

    void apply(GraphicsContext&) const override;

    LineCap m_lineCap;
};

class SetLineDash : public Item {
public:
    static Ref<SetLineDash> create(const DashArray& dashArray, float dashOffset)
    {
        return adoptRef(*new SetLineDash(dashArray, dashOffset));
    }

    const DashArray& dashArray() const { return m_dashArray; }
    float dashOffset() const { return m_dashOffset; }

private:
    SetLineDash(const DashArray& dashArray, float dashOffset)
        : Item(ItemType::SetLineDash)
        , m_dashArray(dashArray)
        , m_dashOffset(dashOffset)
    {
    }

    void apply(GraphicsContext&) const override;

    DashArray m_dashArray;
    float m_dashOffset;
};

class SetLineJoin : public Item {
public:
    static Ref<SetLineJoin> create(LineJoin lineJoin)
    {
        return adoptRef(*new SetLineJoin(lineJoin));
    }
    
    LineJoin lineJoin() const { return m_lineJoin; }

private:
    SetLineJoin(LineJoin lineJoin)
        : Item(ItemType::SetLineJoin)
        , m_lineJoin(lineJoin)
    {
    }

    void apply(GraphicsContext&) const override;

    LineJoin m_lineJoin;
};

class SetMiterLimit : public Item {
public:
    static Ref<SetMiterLimit> create(float limit)
    {
        return adoptRef(*new SetMiterLimit(limit));
    }

    float miterLimit() const { return m_miterLimit; }

private:
    SetMiterLimit(float miterLimit)
        : Item(ItemType::SetMiterLimit)
        , m_miterLimit(miterLimit)
    {
    }

    void apply(GraphicsContext&) const override;

    float m_miterLimit;
};

class ClearShadow : public Item {
public:
    static Ref<ClearShadow> create()
    {
        return adoptRef(*new ClearShadow);
    }

private:
    ClearShadow()
        : Item(ItemType::ClearShadow)
    {
    }

    void apply(GraphicsContext&) const override;
};

// FIXME: treat as DrawingItem?
class Clip : public Item {
public:
    static Ref<Clip> create(const FloatRect& rect)
    {
        return adoptRef(*new Clip(rect));
    }

    FloatRect rect() const { return m_rect; }

private:
    Clip(const FloatRect& rect)
        : Item(ItemType::Clip)
        , m_rect(rect)
    {
    }

    void apply(GraphicsContext&) const override;

    FloatRect m_rect;
};

class ClipOut : public Item {
public:
    static Ref<ClipOut> create(const FloatRect& rect)
    {
        return adoptRef(*new ClipOut(rect));
    }

    FloatRect rect() const { return m_rect; }

private:
    ClipOut(const FloatRect& rect)
        : Item(ItemType::ClipOut)
        , m_rect(rect)
    {
    }

    void apply(GraphicsContext&) const override;

    FloatRect m_rect;
};

class ClipOutToPath : public Item {
public:
    static Ref<ClipOutToPath> create(const Path& path)
    {
        return adoptRef(*new ClipOutToPath(path));
    }

    const Path& path() const { return m_path; }

private:
    ClipOutToPath(const Path& path)
        : Item(ItemType::ClipOutToPath)
        , m_path(path)
    {
    }

    void apply(GraphicsContext&) const override;

    const Path m_path;
};

class ClipPath : public Item {
public:
    static Ref<ClipPath> create(const Path& path, WindRule windRule)
    {
        return adoptRef(*new ClipPath(path, windRule));
    }

    const Path& path() const { return m_path; }
    WindRule windRule() const { return m_windRule; }

private:
    ClipPath(const Path& path, WindRule windRule)
        : Item(ItemType::ClipPath)
        , m_path(path)
        , m_windRule(windRule)
    {
    }

    void apply(GraphicsContext&) const override;

    const Path m_path;
    WindRule m_windRule;
};

class DrawGlyphs : public DrawingItem {
public:
    static Ref<DrawGlyphs> create(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned count, const FloatPoint& blockLocation, const FloatSize& localAnchor, FontSmoothingMode smoothingMode)
    {
        return adoptRef(*new DrawGlyphs(font, glyphs, advances, count, blockLocation, localAnchor, smoothingMode));
    }

    const FloatPoint& blockLocation() const { return m_blockLocation; }
    void setBlockLocation(const FloatPoint& blockLocation) { m_blockLocation = blockLocation; }

    const FloatSize& localAnchor() const { return m_localAnchor; }

    FloatPoint anchorPoint() const { return m_blockLocation + m_localAnchor; }

    const Vector<GlyphBufferGlyph, 128>& glyphs() const { return m_glyphs; }

private:
    DrawGlyphs(const Font&, const GlyphBufferGlyph*, const GlyphBufferAdvance*, unsigned count, const FloatPoint& blockLocation, const FloatSize& localAnchor, FontSmoothingMode);

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

class ImageHandle {
public:
    RefPtr<Image> image;
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

    const Image& image() const { return m_image.get(); }
    FloatPoint source() const { return m_source; }
    FloatRect destination() const { return m_destination; }

    FloatSize tileSize() const { return m_tileSize; }
    FloatSize spacing() const { return m_spacing; }

private:
    DrawTiledImage(Image&, const FloatRect& destination, const FloatPoint& source, const FloatSize& tileSize, const FloatSize& spacing, const ImagePaintingOptions&);

    void apply(GraphicsContext&) const override;

    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_destination; }

    mutable Ref<Image> m_image; // FIXME: Drawing images can cause their animations to progress. This shouldn't have to be mutable.
    FloatRect m_destination;
    FloatPoint m_source;
    FloatSize m_tileSize;
    FloatSize m_spacing;
    ImagePaintingOptions m_imagePaintingOptions;
};

class DrawTiledScaledImage : public DrawingItem {
public:
    static Ref<DrawTiledScaledImage> create(Image& image, const FloatRect& destination, const FloatRect& source, const FloatSize& tileScaleFactor, Image::TileRule hRule, Image::TileRule vRule, const ImagePaintingOptions& imagePaintingOptions)
    {
        return adoptRef(*new DrawTiledScaledImage(image, destination, source, tileScaleFactor, hRule, vRule, imagePaintingOptions));
    }

    const Image& image() const { return m_image.get(); }
    FloatRect source() const { return m_source; }
    FloatRect destination() const { return m_destination; }

private:
    DrawTiledScaledImage(Image&, const FloatRect& destination, const FloatRect& source, const FloatSize& tileScaleFactor, Image::TileRule hRule, Image::TileRule vRule, const ImagePaintingOptions&);

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

#if USE(CG) || USE(CAIRO) || USE(DIRECT2D)
class DrawNativeImage : public DrawingItem {
public:
    static Ref<DrawNativeImage> create(const NativeImagePtr& image, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
    {
        return adoptRef(*new DrawNativeImage(image, imageSize, destRect, srcRect, options));
    }

    FloatRect source() const { return m_srcRect; }
    FloatRect destination() const { return m_destination; }

private:
    DrawNativeImage(const NativeImagePtr&, const FloatSize& selfSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions&);

    void apply(GraphicsContext&) const override;

    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_destination; }

#if USE(CG)
    RetainPtr<CGImageRef> m_image;
#endif
    FloatSize m_imageSize;
    FloatRect m_destination;
    FloatRect m_srcRect;
    ImagePaintingOptions m_options;
};
#endif

class DrawPattern : public DrawingItem {
public:
    static Ref<DrawPattern> create(Image& image, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
    {
        return adoptRef(*new DrawPattern(image, destRect, tileRect, patternTransform, phase, spacing, options));
    }

    const Image& image() const { return m_image.get(); }
    const AffineTransform& patternTransform() const { return m_patternTransform; }
    FloatRect tileRect() const { return m_tileRect; }
    FloatRect destRect() const { return m_destination; }
    FloatPoint phase() const { return m_phase; }
    FloatSize spacing() const { return m_spacing; }

private:
    DrawPattern(Image&, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& = { });

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

// Is DrawingItem because the size of the transparency layer is implicitly the clip bounds.
class BeginTransparencyLayer : public DrawingItem {
public:
    static Ref<BeginTransparencyLayer> create(float opacity)
    {
        return adoptRef(*new BeginTransparencyLayer(opacity));
    }

    float opacity() const { return m_opacity; }

private:
    BeginTransparencyLayer(float opacity)
        : DrawingItem(ItemType::BeginTransparencyLayer)
        , m_opacity(opacity)
    {
    }

    void apply(GraphicsContext&) const override;

    float m_opacity;
};

class EndTransparencyLayer : public DrawingItem {
public:
    static Ref<EndTransparencyLayer> create()
    {
        return adoptRef(*new EndTransparencyLayer);
    }

private:
    EndTransparencyLayer()
        : DrawingItem(ItemType::EndTransparencyLayer)
    {
    }

    void apply(GraphicsContext&) const override;
};

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

    FloatPoint point1() const { return m_point1; }
    FloatPoint point2() const { return m_point2; }

private:
    DrawLine(const FloatPoint& point1, const FloatPoint& point2)
        : DrawingItem(ItemType::DrawLine)
        , m_point1(point1)
        , m_point2(point2)
    {
    }

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override;

    FloatPoint m_point1;
    FloatPoint m_point2;
};

class DrawLinesForText : public DrawingItem {
public:
    static Ref<DrawLinesForText> create(const FloatPoint& blockLocation, const FloatSize& localAnchor, float thickness, const DashArray& widths, bool printing, bool doubleLines)
    {
        return adoptRef(*new DrawLinesForText(blockLocation, localAnchor, thickness, widths, printing, doubleLines));
    }

    void setBlockLocation(const FloatPoint& blockLocation) { m_blockLocation = blockLocation; }
    const FloatPoint& blockLocation() const { return m_blockLocation; }
    const FloatSize& localAnchor() const { return m_localAnchor; }
    FloatPoint point() const { return m_blockLocation + m_localAnchor; }
    float thickness() const { return m_thickness; }
    const DashArray& widths() const { return m_widths; }
    bool isPrinting() const { return m_printing; }
    bool doubleLines() const { return m_doubleLines; }

private:
    DrawLinesForText(const FloatPoint& blockLocation, const FloatSize& localAnchor, float thickness, const DashArray& widths, bool printing, bool doubleLines)
        : DrawingItem(ItemType::DrawLinesForText)
        , m_blockLocation(blockLocation)
        , m_localAnchor(localAnchor)
        , m_widths(widths)
        , m_thickness(thickness)
        , m_printing(printing)
        , m_doubleLines(doubleLines)
    {
    }

    void apply(GraphicsContext&) const override;

    Optional<FloatRect> localBounds(const GraphicsContext&) const override;

    FloatPoint m_blockLocation;
    FloatSize m_localAnchor;
    DashArray m_widths;
    float m_thickness;
    bool m_printing;
    bool m_doubleLines;
};

class DrawDotsForDocumentMarker : public DrawingItem {
public:
    static Ref<DrawDotsForDocumentMarker> create(const FloatRect& rect, DocumentMarkerLineStyle style)
    {
        return adoptRef(*new DrawDotsForDocumentMarker(rect, style));
    }

    FloatRect rect() const { return m_rect; }

private:
    DrawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle style)
        : DrawingItem(ItemType::DrawDotsForDocumentMarker)
        , m_rect(rect)
        , m_style(style)
    {
    }

    void apply(GraphicsContext&) const override;

    Optional<FloatRect> localBounds(const GraphicsContext&) const override;

    FloatRect m_rect;
    DocumentMarkerLineStyle m_style;
};

class DrawEllipse : public DrawingItem {
public:
    static Ref<DrawEllipse> create(const FloatRect& rect)
    {
        return adoptRef(*new DrawEllipse(rect));
    }

    FloatRect rect() const { return m_rect; }

private:
    DrawEllipse(const FloatRect& rect)
        : DrawingItem(ItemType::DrawEllipse)
        , m_rect(rect)
    {
    }

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_rect; }

    FloatRect m_rect;
};

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

    const Path& path() const { return m_path; }
    float width() const { return m_width; }
    float offset() const { return m_offset; }
    const Color& color() const { return m_color; }

private:
    DrawFocusRingPath(const Path& path, float width, float offset, const Color& color)
        : DrawingItem(ItemType::DrawFocusRingPath)
        , m_path(path)
        , m_width(width)
        , m_offset(offset)
        , m_color(color)
    {
    }

    void apply(GraphicsContext&) const override;

    Optional<FloatRect> localBounds(const GraphicsContext&) const override;

    const Path m_path;
    float m_width;
    float m_offset;
    Color m_color;
};

class DrawFocusRingRects : public DrawingItem {
public:
    static Ref<DrawFocusRingRects> create(const Vector<FloatRect>& rects, float width, float offset, const Color& color)
    {
        return adoptRef(*new DrawFocusRingRects(rects, width, offset, color));
    }

    const Vector<FloatRect> rects() const { return m_rects; }
    float width() const { return m_width; }
    float offset() const { return m_offset; }
    const Color& color() const { return m_color; }

private:
    DrawFocusRingRects(const Vector<FloatRect>& rects, float width, float offset, const Color& color)
        : DrawingItem(ItemType::DrawFocusRingRects)
        , m_rects(rects)
        , m_width(width)
        , m_offset(offset)
        , m_color(color)
    {
    }

    void apply(GraphicsContext&) const override;

    Optional<FloatRect> localBounds(const GraphicsContext&) const override;

    Vector<FloatRect> m_rects;
    float m_width;
    float m_offset;
    Color m_color;
};

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

    FloatRect rect() const { return m_rect; }

private:
    FillRectWithGradient(const FloatRect& rect, Gradient& gradient)
        : DrawingItem(ItemType::FillRectWithGradient)
        , m_rect(rect)
        , m_gradient(gradient)
    {
    }

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_rect; }

    FloatRect m_rect;
    mutable Ref<Gradient> m_gradient; // FIXME: Make this not mutable
};

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

    const FloatRect& rect() const { return m_rect; }
    const FloatRoundedRect& roundedHoleRect() const { return m_roundedHoleRect; }
    const Color& color() const { return m_color; }

private:
    FillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
        : DrawingItem(ItemType::FillRectWithRoundedHole)
        , m_rect(rect)
        , m_roundedHoleRect(roundedHoleRect)
        , m_color(color)
    {
    }

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_rect; }

    FloatRect m_rect;
    FloatRoundedRect m_roundedHoleRect;
    Color m_color;
};

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

    FloatRect rect() const { return m_rect; }

private:
    FillEllipse(const FloatRect& rect)
        : DrawingItem(ItemType::FillEllipse)
        , m_rect(rect)
    {
    }

    void apply(GraphicsContext&) const override;

    Optional<FloatRect> localBounds(const GraphicsContext&) const override { return m_rect; }

    FloatRect m_rect;
};

class StrokeRect : public DrawingItem {
public:
    static Ref<StrokeRect> create(const FloatRect& rect, float lineWidth)
    {
        return adoptRef(*new StrokeRect(rect, lineWidth));
    }

    FloatRect rect() const { return m_rect; }
    float lineWidth() const { return m_lineWidth; }

private:
    StrokeRect(const FloatRect& rect, float lineWidth)
        : DrawingItem(ItemType::StrokeRect)
        , m_rect(rect)
        , m_lineWidth(lineWidth)
    {
    }

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override;

    FloatRect m_rect;
    float m_lineWidth;
};

class StrokePath : public DrawingItem {
public:
    static Ref<StrokePath> create(const Path& path)
    {
        return adoptRef(*new StrokePath(path));
    }

    const Path& path() const { return m_path; }

private:
    StrokePath(const Path& path)
        : DrawingItem(ItemType::StrokePath)
        , m_path(path)
    {
    }

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override;

    const Path m_path;
    FloatPoint m_blockLocation;
};

class StrokeEllipse : public DrawingItem {
public:
    static Ref<StrokeEllipse> create(const FloatRect& rect)
    {
        return adoptRef(*new StrokeEllipse(rect));
    }

    FloatRect rect() const { return m_rect; }

private:
    StrokeEllipse(const FloatRect& rect)
        : DrawingItem(ItemType::StrokeEllipse)
        , m_rect(rect)
    {
    }

    void apply(GraphicsContext&) const override;
    Optional<FloatRect> localBounds(const GraphicsContext&) const override;

    FloatRect m_rect;
};

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

private:
    ApplyStrokePattern()
        : Item(ItemType::ApplyStrokePattern)
    {
    }

    void apply(GraphicsContext&) const override;
};

class ApplyFillPattern : public Item {
public:
    static Ref<ApplyFillPattern> create()
    {
        return adoptRef(*new ApplyFillPattern);
    }

private:
    ApplyFillPattern()
        : Item(ItemType::ApplyFillPattern)
    {
    }

    void apply(GraphicsContext&) const override;
};
#endif

class ApplyDeviceScaleFactor : public Item {
public:
    static Ref<ApplyDeviceScaleFactor> create(float scaleFactor)
    {
        return adoptRef(*new ApplyDeviceScaleFactor(scaleFactor));
    }

    float scaleFactor() const { return m_scaleFactor; }

private:
    ApplyDeviceScaleFactor(float scaleFactor)
        : Item(ItemType::ApplyDeviceScaleFactor)
        , m_scaleFactor(scaleFactor)
    {
    }

    void apply(GraphicsContext&) const override;

    float m_scaleFactor;
};

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
    case ItemType::ConcatenateCTM:
        encoder << downcast<ConcatenateCTM>(*this);
        break;
    case ItemType::SetState:
        encoder << downcast<SetState>(*this);
        break;
    case ItemType::SetLineCap:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode SetLineCap");
        break;
    case ItemType::SetLineDash:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode SetLineDash");
        break;
    case ItemType::SetLineJoin:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode SetLineJoin");
        break;
    case ItemType::SetMiterLimit:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode SetMiterLimit");
        break;
    case ItemType::ClearShadow:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode ClearShadow");
        break;
    case ItemType::Clip:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode Clip");
        break;
    case ItemType::ClipOut:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode ClipOut");
        break;
    case ItemType::ClipOutToPath:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode ClipOutToPath");
        break;
    case ItemType::ClipPath:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode ClipPath");
        break;
    case ItemType::DrawGlyphs:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode DrawGlyphs");
        break;
    case ItemType::DrawImage:
        encoder << downcast<DrawImage>(*this);
        break;
    case ItemType::DrawTiledImage:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode DrawTiledImage");
        break;
    case ItemType::DrawTiledScaledImage:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode DrawTiledScaledImage");
        break;
#if USE(CG) || USE(CAIRO) || USE(DIRECT2D)
    case ItemType::DrawNativeImage:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode DrawNativeImage");
        break;
#endif
    case ItemType::DrawPattern:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode DrawPattern");
        break;
    case ItemType::DrawRect:
        encoder << downcast<DrawRect>(*this);
        break;
    case ItemType::DrawLine:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode DrawLine");
        break;
    case ItemType::DrawLinesForText:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode DrawLinesForText");
        break;
    case ItemType::DrawDotsForDocumentMarker:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode DrawDotsForDocumentMarker");
        break;
    case ItemType::DrawEllipse:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode DrawEllipse");
        break;
    case ItemType::DrawPath:
        encoder << downcast<DrawPath>(*this);
        break;
    case ItemType::DrawFocusRingPath:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode DrawFocusRingPath");
        break;
    case ItemType::DrawFocusRingRects:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode DrawFocusRingRects");
        break;
    case ItemType::FillRect:
        encoder << downcast<FillRect>(*this);
        break;
    case ItemType::FillRectWithColor:
        encoder << downcast<FillRectWithColor>(*this);
        break;
    case ItemType::FillRectWithGradient:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode FillRectWithGradient");
        break;
    case ItemType::FillCompositedRect:
        encoder << downcast<FillCompositedRect>(*this);
        break;
    case ItemType::FillRoundedRect:
        encoder << downcast<FillRoundedRect>(*this);
        break;
    case ItemType::FillRectWithRoundedHole:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode FillRectWithRoundedHole");
        break;
    case ItemType::FillPath:
        encoder << downcast<FillPath>(*this);
        break;
    case ItemType::FillEllipse:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode FillEllipse");
        break;
    case ItemType::StrokeRect:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode StrokeRect");
        break;
    case ItemType::StrokePath:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode StrokePath");
        break;
    case ItemType::StrokeEllipse:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode StrokeEllipse");
        break;
    case ItemType::ClearRect:
        encoder << downcast<ClearRect>(*this);
        break;
    case ItemType::BeginTransparencyLayer:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode BeginTransparencyLayer");
        break;
    case ItemType::EndTransparencyLayer:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode EndTransparencyLayer");
        break;
#if USE(CG)
    case ItemType::ApplyStrokePattern:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode ApplyStrokePattern");
        break;
    case ItemType::ApplyFillPattern:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode ApplyFillPattern");
        break;
#endif
    case ItemType::ApplyDeviceScaleFactor:
        WTFLogAlways("DisplayList::Item::encode cannot yet encode ApplyDeviceScaleFactor");
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
    case ItemType::ConcatenateCTM:
        if (auto item = ConcatenateCTM::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::SetState:
        if (auto item = SetState::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::SetLineCap:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode SetLineCap");
        break;
    case ItemType::SetLineDash:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode SetLineDash");
        break;
    case ItemType::SetLineJoin:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode SetLineJoin");
        break;
    case ItemType::SetMiterLimit:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode SetMiterLimit");
        break;
    case ItemType::ClearShadow:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode ClearShadow");
        break;
    case ItemType::Clip:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode Clip");
        break;
    case ItemType::ClipOut:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode ClipOut");
        break;
    case ItemType::ClipOutToPath:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode ClipOutToPath");
        break;
    case ItemType::ClipPath:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode ClipPath");
        break;
    case ItemType::DrawGlyphs:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode DrawGlyphs");
        break;
    case ItemType::DrawImage:
        if (auto item = DrawImage::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::DrawTiledImage:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode DrawTiledImage");
        break;
    case ItemType::DrawTiledScaledImage:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode DrawTiledScaledImage");
        break;
#if USE(CG) || USE(CAIRO) || USE(DIRECT2D)
    case ItemType::DrawNativeImage:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode DrawNativeImage");
        break;
#endif
    case ItemType::DrawPattern:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode DrawPattern");
        break;
    case ItemType::DrawRect:
        if (auto item = DrawRect::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::DrawLine:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode DrawLine");
        break;
    case ItemType::DrawLinesForText:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode DrawLinesForText");
        break;
    case ItemType::DrawDotsForDocumentMarker:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode DrawDotsForDocumentMarker");
        break;
    case ItemType::DrawEllipse:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode DrawEllipse");
        break;
    case ItemType::DrawPath:
        if (auto item = DrawPath::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::DrawFocusRingPath:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode DrawFocusRingPath");
        break;
    case ItemType::DrawFocusRingRects:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode DrawFocusRingRects");
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
        WTFLogAlways("DisplayList::Item::decode cannot yet decode FillRectWithGradient");
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
        WTFLogAlways("DisplayList::Item::decode cannot yet decode FillRectWithRoundedHole");
        break;
    case ItemType::FillPath:
        if (auto item = FillPath::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::FillEllipse:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode FillEllipse");
        break;
    case ItemType::StrokeRect:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode StrokeRect");
        break;
    case ItemType::StrokePath:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode StrokePath");
        break;
    case ItemType::StrokeEllipse:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode StrokeEllipse");
        break;
    case ItemType::ClearRect:
        if (auto item = ClearRect::decode(decoder))
            return static_reference_cast<Item>(WTFMove(*item));
        break;
    case ItemType::BeginTransparencyLayer:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode BeginTransparencyLayer");
        break;
    case ItemType::EndTransparencyLayer:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode EndTransparencyLayer");
        break;
#if USE(CG)
    case ItemType::ApplyStrokePattern:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode ApplyStrokePattern");
        break;
    case ItemType::ApplyFillPattern:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode ApplyFillPattern");
        break;
#endif
    case ItemType::ApplyDeviceScaleFactor:
        WTFLogAlways("DisplayList::Item::decode cannot yet decode ApplyDeviceScaleFactor");
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
