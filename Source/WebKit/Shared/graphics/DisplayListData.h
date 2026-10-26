/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/FontCustomPlatformData.h>
#include <WebCore/SourceBrushLogicalGradient.h>
#include <wtf/Vector.h>

namespace WebKit {

struct DisplayListDataFont {
    WebCore::FontInternalAttributes attributes;
    WebCore::FontPlatformDataAttributes platformData;
    std::optional<uint64_t> customFontIndex;
};

struct DrawDisplayListAtIndex {
    uint64_t index { 0 };
};

struct DrawGlyphsWithFontAtIndex {
    uint64_t fontIndex { 0 };
    Vector<WebCore::GlyphBufferGlyph> glyphs;
    Vector<WebCore::FloatSize> advances;
    WebCore::FloatPoint localAnchor;
    WebCore::FontSmoothingMode fontSmoothingMode { WebCore::FontSmoothingMode::Auto };
};

// FIXME: Add the source image once image buffers can be serialized by value.
struct DrawFilteredImageBuffer {
    WebCore::FloatRect sourceImageRect;
    Ref<WebCore::Filter> filter;
};

struct SourceBrushData {
    WebCore::Color color;
    Variant<std::monostate, WebCore::SourceBrushLogicalGradient> patternGradient;
};

struct SetStateData {
    WebCore::GraphicsContextState::ChangeFlags changes;
    SourceBrushData fillBrush;
    WebCore::WindRule fillRule { WebCore::WindRule::NonZero };
    SourceBrushData strokeBrush;
    float strokeThickness { 0 };
    WebCore::StrokeStyle strokeStyle { WebCore::StrokeStyle::SolidStroke };
    WebCore::CompositeMode compositeMode;
    std::optional<WebCore::GraphicsDropShadow> dropShadow;
    std::optional<WebCore::GraphicsStyle> style;
    float alpha { 1 };
    WebCore::InterpolationQuality imageInterpolationQuality { WebCore::InterpolationQuality::Default };
    WebCore::TextDrawingModeFlags textDrawingMode;
    bool shouldAntialias { true };
    bool shouldSmoothFonts { true };
    bool shouldSubpixelQuantizeFonts { true };
    bool shadowsIgnoreTransforms { false };
};

using DisplayListDataItem = Variant
    < WebCore::DisplayList::ApplyDeviceScaleFactor
    , WebCore::DisplayList::BeginTransparencyLayer
    , WebCore::DisplayList::BeginTransparencyLayerWithCompositeMode
    , WebCore::DisplayList::ClearRect
    , WebCore::DisplayList::Clip
    , WebCore::DisplayList::ClipRoundedRect
    , WebCore::DisplayList::ClipOut
    , WebCore::DisplayList::ClipOutRoundedRect
    , WebCore::DisplayList::ClipOutToPath
    , WebCore::DisplayList::ClipPath
    , WebCore::DisplayList::ConcatenateCTM
    , WebCore::DisplayList::DrawControlPart
    , WebCore::DisplayList::DrawDotsForDocumentMarker
    , WebCore::DisplayList::DrawEllipse
    , DrawFilteredImageBuffer
    , WebCore::DisplayList::DrawFocusRingPath
    , WebCore::DisplayList::DrawFocusRingRects
    , DrawGlyphsWithFontAtIndex
    , DrawDisplayListAtIndex
    , WebCore::DisplayList::DrawLine
    , WebCore::DisplayList::DrawLinesForText
    , WebCore::DisplayList::DrawPath
    , WebCore::DisplayList::DrawRect
    , WebCore::DisplayList::DrawSystemImage
    , WebCore::DisplayList::EndTransparencyLayer
    , WebCore::DisplayList::FillCompositedRect
    , WebCore::DisplayList::FillEllipse
    , WebCore::DisplayList::FillPath
    , WebCore::DisplayList::FillRect
    , WebCore::DisplayList::FillRectWithColor
    , WebCore::DisplayList::FillRectWithGradient
    , WebCore::DisplayList::FillRectWithGradientAndSpaceTransform
    , WebCore::DisplayList::FillRectWithRoundedHole
    , WebCore::DisplayList::FillRoundedRect
    , WebCore::DisplayList::ResetClip
    , WebCore::DisplayList::Restore
    , WebCore::DisplayList::Rotate
    , WebCore::DisplayList::Save
    , WebCore::DisplayList::Scale
    , WebCore::DisplayList::SetCTM
    , WebCore::DisplayList::SetInlineFillColor
    , WebCore::DisplayList::SetInlineStroke
    , WebCore::DisplayList::SetLineCap
    , WebCore::DisplayList::SetLineDash
    , WebCore::DisplayList::SetLineJoin
    , WebCore::DisplayList::SetMiterLimit
    , SetStateData
    , WebCore::DisplayList::StrokeEllipse
    , WebCore::DisplayList::StrokePath
    , WebCore::DisplayList::StrokeRect
    , WebCore::DisplayList::Translate
#if USE(CG)
    , WebCore::DisplayList::ApplyFillPattern
    , WebCore::DisplayList::ApplyStrokePattern
#endif
    , WebCore::DisplayList::BeginPage
    , WebCore::DisplayList::EndPage
    , WebCore::DisplayList::SetURLForRect
>;

// A display list only draws display lists that come before it, and the last one is the root.
class DisplayListData {
public:
    // Placeholders are drawn into the result, so whatever they draw has to be ready.
    static std::optional<DisplayListData> create(const WebCore::DisplayList::DisplayList&);

    DisplayListData(Vector<WebCore::FontCustomPlatformSerializedData>&& customFonts, Vector<DisplayListDataFont>&&, Vector<Vector<DisplayListDataItem>>&&);

    const Vector<WebCore::FontCustomPlatformSerializedData>& customFonts() const LIFETIME_BOUND { return m_customFonts; }
    const Vector<DisplayListDataFont>& fonts() const LIFETIME_BOUND { return m_fonts; }
    const Vector<Vector<DisplayListDataItem>>& displayLists() const LIFETIME_BOUND { return m_displayLists; }

    RefPtr<const WebCore::DisplayList::DisplayList> createDisplayList(bool shouldUseLockdownFontParser) const;

private:
    Vector<WebCore::FontCustomPlatformSerializedData> m_customFonts;
    Vector<DisplayListDataFont> m_fonts;
    Vector<Vector<DisplayListDataItem>> m_displayLists;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
