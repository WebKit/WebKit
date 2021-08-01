/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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

#include "DisplayList.h"
#include "DisplayListDrawGlyphsRecorder.h"
#include "DisplayListItems.h"
#include "GraphicsContext.h"
#include "Image.h" // For Image::TileRule.
#include "TextFlags.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

enum class AlphaPremultiplication : uint8_t;

class FloatPoint;
class FloatPoint;
class FloatRect;
class Font;
class GlyphBuffer;
class Image;
class PixelBuffer;

struct GraphicsContextState;
struct ImagePaintingOptions;

namespace DisplayList {

class Recorder : public GraphicsContext {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(Recorder);
public:
    class Delegate;
    WEBCORE_EXPORT Recorder(DisplayList&, const GraphicsContextState&, const FloatRect& initialClip, const AffineTransform&, Delegate* = nullptr, DrawGlyphsRecorder::DrawGlyphsDeconstruction = DrawGlyphsRecorder::DrawGlyphsDeconstruction::Deconstruct);
    WEBCORE_EXPORT virtual ~Recorder();

    WEBCORE_EXPORT void getPixelBuffer(const PixelBufferFormat& outputFormat, const IntRect& sourceRect);
    WEBCORE_EXPORT void putPixelBuffer(const PixelBuffer&, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat);

    bool isEmpty() const { return m_displayList.isEmpty(); }

    class Delegate {
    public:
        virtual ~Delegate() { }
        virtual bool canAppendItemOfType(ItemType) { return false; }
        virtual void recordNativeImageUse(NativeImage&) { }
        virtual bool isCachedImageBuffer(const ImageBuffer&) const { return false; }
        virtual void recordFontUse(Font&) { }
        virtual void recordImageBufferUse(ImageBuffer&) { }
        virtual RenderingMode renderingMode() const { return RenderingMode::Unaccelerated; }
    };

    void flushContext(FlushIdentifier identifier) { append<FlushContext>(identifier); }

private:
    friend class DrawGlyphsRecorder;
    Recorder(Recorder& parent, const GraphicsContextState&, const FloatRect& initialClip, const AffineTransform& initialCTM);

    bool hasPlatformContext() const final { return false; }
    // FIXME: Maybe remove this?
    bool canDrawImageBuffer(const ImageBuffer&) const;
    PlatformGraphicsContext* platformContext() const final { return nullptr; }
    RenderingMode renderingMode() const final;

#if USE(CG) || USE(DIRECT2D)
    void setIsCALayerContext(bool) final { }
    bool isCALayerContext() const final { return false; }
    void setIsAcceleratedContext(bool) final { }
#endif

    void fillRoundedRectImpl(const FloatRoundedRect&, const Color&) final { ASSERT_NOT_REACHED(); }
    void drawLineForText(const FloatRect&, bool, bool, StrokeStyle) final { ASSERT_NOT_REACHED(); }

    void updateState(const GraphicsContextState&, GraphicsContextState::StateChangeFlags) final;

    void setLineCap(LineCap) final;
    void setLineDash(const DashArray&, float dashOffset) final;
    void setLineJoin(LineJoin) final;
    void setMiterLimit(float) final;

    void fillRect(const FloatRect&) final;
    void fillRect(const FloatRect&, const Color&) final;
    void fillRect(const FloatRect&, Gradient&) final;
    void fillRect(const FloatRect&, const Color&, CompositeOperator, BlendMode) final;
    void fillRoundedRect(const FloatRoundedRect&, const Color&, BlendMode) final;
    void fillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect& roundedHoleRect, const Color&) final;
    void fillPath(const Path&) final;
    void fillEllipse(const FloatRect&) final;
    void strokeRect(const FloatRect&, float lineWidth) final;
    void strokePath(const Path&) final;
    void strokeEllipse(const FloatRect&) final;
    void clearRect(const FloatRect&) final;

#if USE(CG) || USE(DIRECT2D)
    void applyStrokePattern() final;
    void applyFillPattern() final;
#endif

    void drawGlyphs(const Font&, const GlyphBufferGlyph*, const GlyphBufferAdvance*, unsigned numGlyphs, const FloatPoint& anchorPoint, FontSmoothingMode) final;

    void appendDrawGlyphsItemWithCachedFont(const Font&, const GlyphBufferGlyph*, const GlyphBufferAdvance*, unsigned count, const FloatPoint& localAnchor, FontSmoothingMode);

    void drawImageBuffer(WebCore::ImageBuffer&, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions&) final;
    void drawNativeImage(NativeImage&, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions&) final;
    void drawPattern(NativeImage&, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions&) final;

    void drawRect(const FloatRect&, float borderThickness) final;
    void drawLine(const FloatPoint&, const FloatPoint&) final;
    void drawLinesForText(const FloatPoint&, float thickness, const DashArray& widths, bool printing, bool doubleLines, StrokeStyle) final;
    void drawDotsForDocumentMarker(const FloatRect&, DocumentMarkerLineStyle) final;
    void drawEllipse(const FloatRect&) final;

    void drawPath(const Path&) final;

    void drawFocusRing(const Path&, float width, float offset, const Color&) final;
    void drawFocusRing(const Vector<FloatRect>&, float width, float offset, const Color&) final;

#if PLATFORM(MAC)
    void drawFocusRing(const Path&, double timeOffset, bool& needsRedraw, const Color&) final;
    void drawFocusRing(const Vector<FloatRect>&, double timeOffset, bool& needsRedraw, const Color&) final;
#endif

    void save() final;
    void restore() final;

    void translate(float x, float y) final;
    void rotate(float angleInRadians) final;
    void scale(const FloatSize&) final;
    void concatCTM(const AffineTransform&) final;
    void setCTM(const AffineTransform&) final;
    AffineTransform getCTM(GraphicsContext::IncludeDeviceScale) const final;

    void beginTransparencyLayer(float opacity) final;
    void endTransparencyLayer() final;

    void clip(const FloatRect&) final;
    void clipOut(const FloatRect&) final;
    void clipOut(const Path&) final;
    void clipPath(const Path&, WindRule) final;
    IntRect clipBounds() const final;
    void clipToImageBuffer(WebCore::ImageBuffer&, const FloatRect&) final;
    WebCore::GraphicsContext::ClipToDrawingCommandsResult clipToDrawingCommands(const FloatRect& destination, const DestinationColorSpace&, Function<void(GraphicsContext&)>&&) final;

#if ENABLE(VIDEO)
    void paintFrameForMedia(MediaPlayer&, const FloatRect& destination) final;
#endif

    void applyDeviceScaleFactor(float) final;

    FloatRect roundToDevicePixels(const FloatRect&, GraphicsContext::RoundingMode) final;

    template<typename T, class... Args>
    void append(Args&&... args)
    {
        if (UNLIKELY(!canAppendItemOfType(T::itemType)))
            return;

        if constexpr (itemNeedsState<T>())
            appendStateChangeItemIfNecessary();

        m_displayList.append<T>(std::forward<Args>(args)...);

        if constexpr (T::isDrawingItem) {
            if (LIKELY(!m_displayList.tracksDrawingItemExtents()))
                return;

            auto item = T(std::forward<Args>(args)...);
            if (auto rect = item.localBounds(*this))
                m_displayList.addDrawingItemExtent(extentFromLocalBounds(*rect));
            else if (auto rect = item.globalBounds())
                m_displayList.addDrawingItemExtent(*rect);
            else
                m_displayList.addDrawingItemExtent(std::nullopt);
        }
    }

    WEBCORE_EXPORT bool canAppendItemOfType(ItemType) const;

    template<typename T>
    static constexpr bool itemNeedsState();

    void recordNativeImageUse(NativeImage&);

    void appendStateChangeItemIfNecessary();
    void appendStateChangeItem(const GraphicsContextStateChange&, GraphicsContextState::StateChangeFlags);

    FloatRect extentFromLocalBounds(const FloatRect&) const;
    
    const AffineTransform& ctm() const;

    struct ContextState {
        AffineTransform ctm;
        FloatRect clipBounds;
        GraphicsContextStateChange stateChange;
        GraphicsContextState lastDrawingState;
        
        ContextState(const GraphicsContextState& state, const AffineTransform& transform, const FloatRect& clip)
            : ctm(transform)
            , clipBounds(clip)
            , lastDrawingState(state)
        {
        }
        
        ContextState cloneForSave() const
        {
            ContextState state(lastDrawingState, ctm, clipBounds);
            state.stateChange = stateChange;
            return state;
        }

        ContextState cloneForTransparencyLayer() const
        {
            auto state = cloneForSave();
            state.lastDrawingState.alpha = 1;
            return state;
        }

        void translate(float x, float y);
        void rotate(float angleInRadians);
        void scale(const FloatSize&);
        void concatCTM(const AffineTransform&);
        void setCTM(const AffineTransform&);
    };
    
    const ContextState& currentState() const;
    ContextState& currentState();

    DisplayList& m_displayList;
    Delegate* m_delegate;
    bool m_isNested;

    Vector<ContextState, 4> m_stateStack;

    DrawGlyphsRecorder m_drawGlyphsRecorder;
};

template<typename T>
constexpr bool Recorder::itemNeedsState()
{
    if (T::isDrawingItem)
        return true;

#if USE(CG)
    if (T::itemType == ItemType::ApplyFillPattern || T::itemType == ItemType::ApplyStrokePattern)
        return true;
#endif

    return false;
}

}
}

