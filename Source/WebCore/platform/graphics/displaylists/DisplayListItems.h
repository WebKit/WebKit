/*
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
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

#include <WebCore/AlphaPremultiplication.h>
#include <WebCore/ControlPart.h>
#include <WebCore/DashArray.h>
#include <WebCore/DisplayListItem.h>
#include <WebCore/Filter.h>
#include <WebCore/FloatRoundedRect.h>
#include <WebCore/Font.h>
#include <WebCore/GlyphBuffer.h>
#include <WebCore/Gradient.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/Image.h>
#include <WebCore/NativeImage.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/SystemImage.h>
#include <WebCore/TextFlags.h>
#include <wtf/Box.h>
#include <wtf/TypeCasts.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class ControlFactory;
struct ImagePaintingOptions;

namespace DisplayList {
class DisplayList;

class Save {
public:
    static constexpr char name[] = "save";

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const { }
};

class Restore {
public:
    static constexpr char name[] = "restore";

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const { }
};

class Translate {
public:
    static constexpr char name[] = "translate";

    Translate(float x, float y)
        : m_x(x)
        , m_y(y)
    {
    }

    float x() const { return m_x; }
    float y() const { return m_y; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    float m_x { 0 };
    float m_y { 0 };
};

class Rotate {
public:
    static constexpr char name[] = "rotate";

    Rotate(float angle)
        : m_angle(angle)
    {
    }

    float angle() const { return m_angle; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    float m_angle { 0 }; // In radians.
};

class Scale {
public:
    static constexpr char name[] = "scale";

    Scale(const FloatSize& size)
        : m_size(size)
    {
    }

    const FloatSize& amount() const { return m_size; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatSize m_size;
};

class SetCTM {
public:
    static constexpr char name[] = "set-ctm";

    SetCTM(const AffineTransform& transform)
        : m_transform(transform)
    {
    }

    const AffineTransform& transform() const { return m_transform; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    AffineTransform m_transform;
};

class ConcatenateCTM {
public:
    static constexpr char name[] = "concatenate-ctm";

    ConcatenateCTM(const AffineTransform& transform)
        : m_transform(transform)
    {
    }

    const AffineTransform& transform() const { return m_transform; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    AffineTransform m_transform;
};

class SetInlineFillColor {
public:
    static constexpr char name[] = "set-inline-fill-color";

    SetInlineFillColor(PackedColor::RGBA colorData)
        : m_colorData(colorData)
    {
    }

    SetInlineFillColor(SRGBA<uint8_t> colorData)
        : m_colorData(*Color(colorData).tryGetAsPackedInline())
    {
    }

    Color color() const { return { asSRGBA(colorData()) }; }
    const PackedColor::RGBA& colorData() const { return m_colorData; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    PackedColor::RGBA m_colorData;
};

class SetInlineStroke {
public:
    static constexpr char name[] = "set-inline-stroke";

    SetInlineStroke(std::optional<PackedColor::RGBA> colorData, std::optional<float> thickness = std::nullopt)
        : m_colorData(colorData)
        , m_thickness(thickness)
    {
        ASSERT(m_colorData || m_thickness);
    }

    SetInlineStroke(float thickness)
        : m_thickness(thickness)
    {
    }

    SetInlineStroke(SRGBA<uint8_t> colorData)
        : m_colorData(PackedColor::RGBA(colorData))
    {
    }

    std::optional<Color> color() const { return m_colorData ? std::optional<Color>(asSRGBA(*m_colorData)) : std::nullopt; }
    std::optional<PackedColor::RGBA> colorData() const { return m_colorData; }
    std::optional<float> thickness() const { return m_thickness; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    std::optional<PackedColor::RGBA> m_colorData;
    std::optional<float> m_thickness;
};

class SetState {
public:
    static constexpr char name[] = "set-state";

    WEBCORE_EXPORT SetState(const GraphicsContextState&);

    GraphicsContextState& state() { return m_state; }
    const GraphicsContextState& state() const { return m_state; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    GraphicsContextState m_state;
};

class SetLineCap {
public:
    static constexpr char name[] = "set-line-cap";

    SetLineCap(LineCap lineCap)
        : m_lineCap(lineCap)
    {
    }

    LineCap lineCap() const { return m_lineCap; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    LineCap m_lineCap;
};

class SetLineDash {
public:
    static constexpr char name[] = "set-line-dash";

    SetLineDash(const DashArray& dashArray, float dashOffset)
        : m_dashArray(dashArray)
        , m_dashOffset(dashOffset)
    {
    }

    const DashArray& dashArray() const { return m_dashArray; }
    float dashOffset() const { return m_dashOffset; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    DashArray m_dashArray;
    float m_dashOffset;
};

class SetLineJoin {
public:
    static constexpr char name[] = "set-line-join";

    SetLineJoin(LineJoin lineJoin)
        : m_lineJoin(lineJoin)
    {
    }

    LineJoin lineJoin() const { return m_lineJoin; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    LineJoin m_lineJoin;
};

class SetMiterLimit {
public:
    static constexpr char name[] = "set-miter-limit";

    SetMiterLimit(float miterLimit)
        : m_miterLimit(miterLimit)
    {
    }

    float miterLimit() const { return m_miterLimit; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    float m_miterLimit;
};

class Clip {
public:
    static constexpr char name[] = "clip";

    Clip(const FloatRect& rect)
        : m_rect(rect)
    {
    }

    const FloatRect& rect() const { return m_rect; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatRect m_rect;
};

class ClipRoundedRect {
public:
    static constexpr char name[] = "clip-rounded-rect";

    ClipRoundedRect(const FloatRoundedRect& rect)
        : m_rect(rect)
    {
    }

    const FloatRoundedRect& rect() const { return m_rect; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatRoundedRect m_rect;
};

class ClipOut {
public:
    static constexpr char name[] = "clip-out";

    ClipOut(const FloatRect& rect)
        : m_rect(rect)
    {
    }

    const FloatRect& rect() const { return m_rect; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatRect m_rect;
};

class ClipOutRoundedRect {
public:
    static constexpr char name[] = "clip-out-rounded-rect";

    ClipOutRoundedRect(const FloatRoundedRect& rect)
        : m_rect(rect)
    {
    }

    const FloatRoundedRect& rect() const { return m_rect; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatRoundedRect m_rect;
};

class ClipToImageBuffer {
public:
    static constexpr char name[] = "clip-to-image-buffer";

    ClipToImageBuffer(Ref<ImageBuffer> imageBuffer, const FloatRect& destinationRect)
        : m_imageBuffer(imageBuffer)
        , m_destinationRect(destinationRect)
    {
    }

    ImageBuffer& imageBuffer() const { return m_imageBuffer; }
    FloatRect destinationRect() const { return m_destinationRect; }

    void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    const Ref<ImageBuffer> m_imageBuffer;
    FloatRect m_destinationRect;
};

class ClipOutToPath {
public:
    static constexpr char name[] = "clip-out-to-path";

    ClipOutToPath(Path&& path)
        : m_path(WTFMove(path))
    {
    }

    ClipOutToPath(const Path& path)
        : m_path(path)
    {
    }

    const Path& path() const { return m_path; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    Path m_path;
};

class ClipPath {
public:
    static constexpr char name[] = "clip-path";

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

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    Path m_path;
    WindRule m_windRule;
};

class ResetClip {
public:
    static constexpr char name[] = "reset-clip";

    ResetClip()
    {
    }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const { }
};

class DrawFilteredImageBuffer {
public:
    static constexpr char name[] = "draw-filtered-image-buffer";

    DrawFilteredImageBuffer(RefPtr<ImageBuffer>&& sourceImage, const FloatRect& sourceImageRect, Filter& filter)
        : m_sourceImage(WTFMove(sourceImage))
        , m_sourceImageRect(sourceImageRect)
        , m_filter(filter)
    {
    }

    ImageBuffer* sourceImage() const { return m_sourceImage.get(); }
    FloatRect sourceImageRect() const { return m_sourceImageRect; }
    Filter& filter() const { return m_filter; }

    void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    const RefPtr<ImageBuffer> m_sourceImage;
    FloatRect m_sourceImageRect;
    const Ref<Filter> m_filter;
};

class DrawGlyphs {
public:
    static constexpr char name[] = "draw-glyphs";

    DrawGlyphs(Ref<const Font>&& font, Vector<GlyphBufferGlyph>&& glyphs, Vector<GlyphBufferAdvance>&& advances, const FloatPoint& localAnchor, FontSmoothingMode smoothingMode)
        : m_font(WTFMove(font))
        , m_glyphs(WTFMove(glyphs))
        , m_advances(WTFMove(advances))
        , m_localAnchor(localAnchor)
        , m_fontSmoothingMode(smoothingMode)
    {
    }

    Ref<const Font> font() const { return m_font; }
    const Vector<GlyphBufferGlyph>& glyphs() const { return m_glyphs; }
    const Vector<GlyphBufferAdvance>& advances() const { return m_advances; }
    size_t length() const { return m_glyphs.size(); }

    FloatPoint localAnchor() const { return m_localAnchor; }
    FontSmoothingMode fontSmoothingMode() const { return m_fontSmoothingMode; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    Ref<const Font> m_font;
    Vector<GlyphBufferGlyph> m_glyphs;
    Vector<GlyphBufferAdvance> m_advances;
    FloatPoint m_localAnchor;
    FontSmoothingMode m_fontSmoothingMode;
};

#if USE(SKIA)
class DrawTextBlob {
public:
    static constexpr char name[] = "draw-text-blob";

    DrawTextBlob(Ref<const Font>&& font, Vector<GlyphBufferGlyph>&& glyphs, Vector<GlyphBufferAdvance>&& advances, const FloatPoint& localAnchor, FontSmoothingMode smoothingMode)
        : m_textBlob(font->buildTextBlob(glyphs, advances, smoothingMode))
        , m_enableAntialiasing(font->enableAntialiasing(smoothingMode))
        , m_isVertical(font->platformData().orientation() == FontOrientation::Vertical)
        , m_localAnchor(localAnchor)
        , m_fontSmoothingMode(smoothingMode)
    {
    }

    size_t length() const
    {
        if (!m_textBlob)
            return 0;

        size_t glyphsCount = 0;
        auto iter = SkTextBlob::Iter(*m_textBlob);
        SkTextBlob::Iter::Run run;
        while (iter.next(&run))
            glyphsCount += run.fGlyphCount;
        return glyphsCount;
    }

    FloatPoint localAnchor() const { return m_localAnchor; }
    FontSmoothingMode fontSmoothingMode() const { return m_fontSmoothingMode; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    sk_sp<SkTextBlob> m_textBlob;
    bool m_enableAntialiasing { false };
    bool m_isVertical { false };
    FloatPoint m_localAnchor;
    FontSmoothingMode m_fontSmoothingMode;
};
#endif // USE(SKIA)

class DrawDisplayList {
public:
    static constexpr char name[] = "draw-display-list";

    DrawDisplayList(Ref<const DisplayList>&&);
    ~DrawDisplayList();

    Ref<const DisplayList> displayList() const;

    void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    Ref<const DisplayList> m_displayList;
};

class DrawPlaceholder {
public:
    static constexpr char name[] = "draw-placeholder";

    DrawPlaceholder(Function<void(GraphicsContext&)>&&);
    ~DrawPlaceholder();

    void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    using FunctionHolder = Box<Function<void(GraphicsContext&)>>;
    FunctionHolder m_function;
};

class DrawImageBuffer {
public:
    static constexpr char name[] = "draw-image-buffer";

    DrawImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
        : m_imageBuffer(imageBuffer)
        , m_destinationRect(destRect)
        , m_srcRect(srcRect)
        , m_options(options)
    {
    }

    ImageBuffer& imageBuffer() const { return m_imageBuffer; }
    FloatRect source() const { return m_srcRect; }
    FloatRect destinationRect() const { return m_destinationRect; }
    ImagePaintingOptions options() const { return m_options; }

    void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    const Ref<ImageBuffer> m_imageBuffer;
    FloatRect m_destinationRect;
    FloatRect m_srcRect;
    ImagePaintingOptions m_options;
};

class DrawNativeImage {
public:
    static constexpr char name[] = "draw-native-image";

    DrawNativeImage(NativeImage& image, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
        : m_image(image)
        , m_destinationRect(destRect)
        , m_srcRect(srcRect)
        , m_options(options)
    {
    }

    NativeImage& nativeImage() const { return m_image; }
    const FloatRect& destinationRect() const { return m_destinationRect; }
    const FloatRect& source() const { return m_srcRect; }
    ImagePaintingOptions options() const { return m_options; }

    void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    const Ref<NativeImage> m_image;
    FloatRect m_destinationRect;
    FloatRect m_srcRect;
    ImagePaintingOptions m_options;
};

class DrawSystemImage {
public:
    static constexpr char name[] = "draw-system-image";

    DrawSystemImage(SystemImage& systemImage, const FloatRect& destinationRect)
        : m_systemImage(systemImage)
        , m_destinationRect(destinationRect)
    {
    }

    const SystemImage& systemImage() const { return m_systemImage; }
    const FloatRect& destinationRect() const { return m_destinationRect; }

    void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    const Ref<SystemImage> m_systemImage;
    FloatRect m_destinationRect;
};

class DrawPatternNativeImage {
public:
    static constexpr char name[] = "draw-pattern-nativeimage";

    DrawPatternNativeImage(NativeImage& image, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
        : m_image(image)
        , m_destination(destRect)
        , m_tileRect(tileRect)
        , m_patternTransform(patternTransform)
        , m_phase(phase)
        , m_spacing(spacing)
        , m_options(options)
    {
    }
    NativeImage& nativeImage() const { return m_image; }
    FloatRect destRect() const { return m_destination; }
    FloatRect tileRect() const { return m_tileRect; }
    const AffineTransform& patternTransform() const { return m_patternTransform; }
    FloatPoint phase() const { return m_phase; }
    FloatSize spacing() const { return m_spacing; }
    ImagePaintingOptions options() const { return m_options; }

    void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    const Ref<NativeImage> m_image;
    FloatRect m_destination;
    FloatRect m_tileRect;
    AffineTransform m_patternTransform;
    FloatPoint m_phase;
    FloatSize m_spacing;
    ImagePaintingOptions m_options;
};

class DrawPatternImageBuffer {
public:
    static constexpr char name[] = "draw-pattern-imagebuffer";

    DrawPatternImageBuffer(ImageBuffer& image, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
        : m_imageBuffer(image)
        , m_destination(destRect)
        , m_tileRect(tileRect)
        , m_patternTransform(patternTransform)
        , m_phase(phase)
        , m_spacing(spacing)
        , m_options(options)
    {
    }
    ImageBuffer& imageBuffer() const { return m_imageBuffer; }
    FloatRect destRect() const { return m_destination; }
    FloatRect tileRect() const { return m_tileRect; }
    const AffineTransform& patternTransform() const { return m_patternTransform; }
    FloatPoint phase() const { return m_phase; }
    FloatSize spacing() const { return m_spacing; }
    ImagePaintingOptions options() const { return m_options; }

    void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    const Ref<ImageBuffer> m_imageBuffer;
    FloatRect m_destination;
    FloatRect m_tileRect;
    AffineTransform m_patternTransform;
    FloatPoint m_phase;
    FloatSize m_spacing;
    ImagePaintingOptions m_options;
};

class BeginTransparencyLayer {
public:
    static constexpr char name[] = "begin-transparency-layer";

    BeginTransparencyLayer(float opacity)
        : m_opacity(opacity)
    {
    }

    float opacity() const { return m_opacity; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    float m_opacity;
};

class BeginTransparencyLayerWithCompositeMode {
public:
    static constexpr char name[] = "begin-transparency-layer-with-composite-mode";

    explicit BeginTransparencyLayerWithCompositeMode(CompositeMode compositeMode)
        : m_compositeMode(compositeMode)
    {
    }

    CompositeMode compositeMode() const { return m_compositeMode; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    CompositeMode m_compositeMode;
};

class EndTransparencyLayer {
public:
    static constexpr char name[] = "end-transparency-layer";

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const { }
};

class DrawRect {
public:
    static constexpr char name[] = "draw-rect";

    DrawRect(FloatRect rect, float borderThickness)
        : m_rect(rect)
        , m_borderThickness(borderThickness)
    {
    }

    FloatRect rect() const { return m_rect; }
    float borderThickness() const { return m_borderThickness; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatRect m_rect;
    float m_borderThickness;
};

class DrawLine {
public:
    static constexpr char name[] = "draw-line";

    DrawLine(FloatPoint point1, FloatPoint point2)
        : m_point1(point1)
        , m_point2(point2)
    {
    }

    FloatPoint point1() const { return m_point1; }
    FloatPoint point2() const { return m_point2; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatPoint m_point1;
    FloatPoint m_point2;
};

class DrawLinesForText {
public:
    static constexpr char name[] = "draw-lines-for-text";

    WEBCORE_EXPORT DrawLinesForText(const FloatPoint&, std::span<const FloatSegment> lineSegments, float thickness, bool printing, bool doubleLines, StrokeStyle);

    FloatPoint point() const { return m_point; }
    float thickness() const { return m_thickness; }
    const Vector<FloatSegment>& lineSegments() const { return m_lineSegments; }
    bool isPrinting() const { return m_printing; }
    bool doubleLines() const { return m_doubleLines; }
    StrokeStyle style() const { return m_style; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatPoint m_point;
    Vector<FloatSegment> m_lineSegments;
    float m_thickness;
    bool m_printing;
    bool m_doubleLines;
    StrokeStyle m_style;
};

class DrawDotsForDocumentMarker {
public:
    static constexpr char name[] = "draw-dots-for-document-marker";

    DrawDotsForDocumentMarker(const FloatRect& rect, const DocumentMarkerLineStyle& style)
        : m_rect(rect)
        , m_style(style)
    {
    }

    FloatRect rect() const { return m_rect; }
    DocumentMarkerLineStyle style() const { return m_style; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatRect m_rect;
    DocumentMarkerLineStyle m_style;
};

class DrawEllipse {
public:
    static constexpr char name[] = "draw-ellipse";

    DrawEllipse(const FloatRect& rect)
        : m_rect(rect)
    {
    }

    FloatRect rect() const { return m_rect; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatRect m_rect;
};

class DrawPath {
public:
    static constexpr char name[] = "draw-path";

    DrawPath(const Path& path)
        : m_path(path)
    {
    }

    DrawPath(Path&& path)
        : m_path(WTFMove(path))
    {
    }

    const Path& path() const { return m_path; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    Path m_path;
};

class DrawFocusRingPath {
public:
    static constexpr char name[] = "draw-focus-ring-path";

    DrawFocusRingPath(const Path& path, float outlineWidth, const Color& color)
        : m_path(path)
        , m_outlineWidth(outlineWidth)
        , m_color(color)
    {
    }

    DrawFocusRingPath(Path&& path, float outlineWidth, const Color& color)
        : m_path(WTFMove(path))
        , m_outlineWidth(outlineWidth)
        , m_color(color)
    {
    }

    const Path& path() const { return m_path; }
    float outlineWidth() const { return m_outlineWidth; }
    const Color& color() const { return m_color; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    Path m_path;
    float m_outlineWidth;
    Color m_color;
};

class DrawFocusRingRects {
public:
    static constexpr char name[] = "draw-focus-ring-rects";

    DrawFocusRingRects(const Vector<FloatRect>& rects, float outlineOffset, float outlineWidth, const Color& color)
        : m_rects(rects)
        , m_outlineOffset(outlineOffset)
        , m_outlineWidth(outlineWidth)
        , m_color(color)
    {
    }

    DrawFocusRingRects(Vector<FloatRect>&& rects, float outlineOffset, float outlineWidth, Color color)
        : m_rects(WTFMove(rects))
        , m_outlineOffset(outlineOffset)
        , m_outlineWidth(outlineWidth)
        , m_color(color)
    {
    }

    const Vector<FloatRect>& rects() const { return m_rects; }
    float outlineOffset() const { return m_outlineOffset; }
    float outlineWidth() const { return m_outlineWidth; }
    const Color& color() const { return m_color; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    Vector<FloatRect> m_rects;
    float m_outlineOffset;
    float m_outlineWidth;
    Color m_color;
};

class FillRect {
public:
    static constexpr char name[] = "fill-rect";

    FillRect(const FloatRect& rect, GraphicsContext::RequiresClipToRect requiresClipToRect)
        : m_rect(rect)
        , m_requiresClipToRect(requiresClipToRect)
    {
    }

    const FloatRect& rect() const { return m_rect; }
    GraphicsContext::RequiresClipToRect requiresClipToRect() const { return m_requiresClipToRect; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatRect m_rect;
    GraphicsContext::RequiresClipToRect m_requiresClipToRect;
};

class FillRectWithColor {
public:
    static constexpr char name[] = "fill-rect-with-color";

    FillRectWithColor(const FloatRect& rect, const Color& color)
        : m_rect(rect)
        , m_color(color)
    {
    }

    FloatRect rect() const { return m_rect; }
    const Color& color() const { return m_color; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatRect m_rect;
    Color m_color;
};

class FillRectWithGradient {
public:
    static constexpr char name[] = "fill-rect-with-gradient";

    WEBCORE_EXPORT FillRectWithGradient(const FloatRect&, Gradient&);
    WEBCORE_EXPORT FillRectWithGradient(FloatRect&&, Ref<Gradient>&&);

    const FloatRect& rect() const { return m_rect; }
    const Gradient& gradient() const { return m_gradient; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatRect m_rect;
    const Ref<Gradient> m_gradient;
};

class FillRectWithGradientAndSpaceTransform {
public:
    static constexpr char name[] = "fill-rect-with-gradient-and-space-transform";

    WEBCORE_EXPORT FillRectWithGradientAndSpaceTransform(const FloatRect&, Gradient&, const AffineTransform&, GraphicsContext::RequiresClipToRect);
    WEBCORE_EXPORT FillRectWithGradientAndSpaceTransform(FloatRect&&, Ref<Gradient>&&, AffineTransform&&, GraphicsContext::RequiresClipToRect);

    const FloatRect& rect() const { return m_rect; }
    const Gradient& gradient() const { return m_gradient; }
    const AffineTransform& gradientSpaceTransform() const { return m_gradientSpaceTransform; }
    GraphicsContext::RequiresClipToRect requiresClipToRect() const { return m_requiresClipToRect; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatRect m_rect;
    const Ref<Gradient> m_gradient;
    AffineTransform m_gradientSpaceTransform;
    GraphicsContext::RequiresClipToRect m_requiresClipToRect;
};

class FillCompositedRect {
public:
    static constexpr char name[] = "fill-composited-rect";

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

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatRect m_rect;
    Color m_color;
    CompositeOperator m_op;
    BlendMode m_blendMode;
};

class FillRoundedRect {
public:
    static constexpr char name[] = "fill-rounded-rect";

    FillRoundedRect(const FloatRoundedRect& rect, const Color& color, BlendMode blendMode)
        : m_rect(rect)
        , m_color(color)
        , m_blendMode(blendMode)
    {
    }

    const FloatRoundedRect& roundedRect() const { return m_rect; }
    const Color& color() const { return m_color; }
    BlendMode blendMode() const { return m_blendMode; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatRoundedRect m_rect;
    Color m_color;
    BlendMode m_blendMode;
};

class FillRectWithRoundedHole {
public:
    static constexpr char name[] = "fill-rect-with-rounded-hole";

    FillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
        : m_rect(rect)
        , m_roundedHoleRect(roundedHoleRect)
        , m_color(color)
    {
    }

    const FloatRect& rect() const { return m_rect; }
    const FloatRoundedRect& roundedHoleRect() const { return m_roundedHoleRect; }
    const Color& color() const { return m_color; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatRect m_rect;
    FloatRoundedRect m_roundedHoleRect;
    Color m_color;
};

class FillPath {
public:
    static constexpr char name[] = "fill-path";

    FillPath(Path&& path)
        : m_path(WTFMove(path))
    {
    }

    FillPath(const Path& path)
        : m_path(path)
    {
    }

    const Path& path() const { return m_path; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    Path m_path;
};

class FillEllipse {
public:
    static constexpr char name[] = "fill-ellipse";

    FillEllipse(const FloatRect& rect)
        : m_rect(rect)
    {
    }

    FloatRect rect() const { return m_rect; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatRect m_rect;
};

class StrokeRect {
public:
    static constexpr char name[] = "stroke-rect";

    StrokeRect(const FloatRect& rect, float lineWidth)
        : m_rect(rect)
        , m_lineWidth(lineWidth)
    {
    }

    FloatRect rect() const { return m_rect; }
    float lineWidth() const { return m_lineWidth; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatRect m_rect;
    float m_lineWidth;
};

class StrokePath {
public:
    static constexpr char name[] = "stroke-path";

    StrokePath(Path&& path)
        : m_path(WTFMove(path))
    {
    }

    StrokePath(const Path& path)
        : m_path(path)
    {
    }

    const Path& path() const { return m_path; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    Path m_path;
};

class StrokeEllipse {
public:
    static constexpr char name[] = "stroke-ellipse";

    StrokeEllipse(const FloatRect& rect)
        : m_rect(rect)
    {
    }

    const FloatRect& rect() const { return m_rect; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatRect m_rect;
};

class ClearRect {
public:
    static constexpr char name[] = "clear-rect";

    ClearRect(const FloatRect& rect)
        : m_rect(rect)
    {
    }

    const FloatRect& rect() const { return m_rect; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatRect m_rect;
};

class DrawControlPart {
public:
    static constexpr char name[] = "draw-control-part";

    WEBCORE_EXPORT DrawControlPart(ControlPart&, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle&);

    ControlPart& part() const { return m_part; }
    FloatRoundedRect borderRect() const { return m_borderRect; }
    float deviceScaleFactor() const { return m_deviceScaleFactor; }
    const ControlStyle& style() const { return m_style; }
    StyleAppearance type() const { return m_part->type(); }

    WEBCORE_EXPORT void apply(GraphicsContext&, ControlFactory&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    const Ref<ControlPart> m_part;
    FloatRoundedRect m_borderRect;
    float m_deviceScaleFactor;
    ControlStyle m_style;
};

#if USE(CG)

class ApplyStrokePattern {
public:
    static constexpr char name[] = "apply-stroke-pattern";

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const { }
};

class ApplyFillPattern {
public:
    static constexpr char name[] = "apply-fill-pattern";

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const { }
};

#endif

class ApplyDeviceScaleFactor {
public:
    static constexpr char name[] = "apply-device-scale-factor";

    ApplyDeviceScaleFactor(float scaleFactor)
        : m_scaleFactor(scaleFactor)
    {
    }

    float scaleFactor() const { return m_scaleFactor; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    float m_scaleFactor { 1 };
};

class BeginPage {
public:
    static constexpr char name[] = "begin-page";

    BeginPage(const FloatRect& pageRect)
        : m_pageRect(pageRect)
    {
    }

    const FloatRect& pageRect() const { return m_pageRect; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    FloatRect m_pageRect;
};

class EndPage {
public:
    static constexpr char name[] = "end-page";

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const { }
};

class SetURLForRect {
public:
    static constexpr char name[] = "set-URL-for-rect";

    SetURLForRect(const URL& link, const FloatRect& destRect)
        : m_link(link)
        , m_destRect(destRect)
    {
    }

    const URL& link() const { return m_link; }
    const FloatRect& destRect() const { return m_destRect; }

    WEBCORE_EXPORT void apply(GraphicsContext&) const;
    void dump(TextStream&, OptionSet<AsTextFlag>) const;

private:
    URL m_link;
    FloatRect m_destRect;
};

} // namespace DisplayList
} // namespace WebCore
