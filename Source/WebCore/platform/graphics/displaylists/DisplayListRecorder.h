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
#include "GraphicsContextImpl.h"
#include "Image.h" // For Image::TileRule.
#include "TextFlags.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

enum class AlphaPremultiplication : uint8_t;
class FloatPoint;
class FloatRect;
class GlyphBuffer;
class FloatPoint;
class Font;
class Image;
class ImageData;

struct GraphicsContextState;
struct ImagePaintingOptions;

namespace DisplayList {

class Recorder : public GraphicsContextImpl {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(Recorder);
public:
    class Delegate;
    WEBCORE_EXPORT Recorder(GraphicsContext&, DisplayList&, const GraphicsContextState&, const FloatRect& initialClip, const AffineTransform&, Delegate* = nullptr, DrawGlyphsRecorder::DrawGlyphsDeconstruction = DrawGlyphsRecorder::DrawGlyphsDeconstruction::Deconstruct);
    WEBCORE_EXPORT virtual ~Recorder();

    WEBCORE_EXPORT void putImageData(AlphaPremultiplication inputFormat, const ImageData&, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat);

    bool isEmpty() const { return m_displayList.isEmpty(); }

    class Delegate {
    public:
        virtual ~Delegate() { }
        virtual void willAppendItemOfType(ItemType) { }
        virtual void cacheNativeImage(NativeImage&) { }
        virtual bool isCachedImageBuffer(const ImageBuffer&) const { return false; }
        virtual void cacheFont(Font&) { }
    };

    void flushContext(FlushIdentifier identifier) { append<FlushContext>(identifier); }

private:
    friend class DrawGlyphsRecorder;
    bool hasPlatformContext() const override { return false; }
    bool canDrawImageBuffer(const ImageBuffer&) const override;
    PlatformGraphicsContext* platformContext() const override { return nullptr; }

    void updateState(const GraphicsContextState&, GraphicsContextState::StateChangeFlags) override;
    void clearShadow() override;

    void setLineCap(LineCap) override;
    void setLineDash(const DashArray&, float dashOffset) override;
    void setLineJoin(LineJoin) override;
    void setMiterLimit(float) override;

    void fillRect(const FloatRect&) override;
    void fillRect(const FloatRect&, const Color&) override;
    void fillRect(const FloatRect&, Gradient&) override;
    void fillRect(const FloatRect&, const Color&, CompositeOperator, BlendMode) override;
    void fillRoundedRect(const FloatRoundedRect&, const Color&, BlendMode) override;
    void fillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect& roundedHoleRect, const Color&) override;
    void fillPath(const Path&) override;
    void fillEllipse(const FloatRect&) override;
    void strokeRect(const FloatRect&, float lineWidth) override;
    void strokePath(const Path&) override;
    void strokeEllipse(const FloatRect&) override;
    void clearRect(const FloatRect&) override;

#if USE(CG)
    void applyStrokePattern() override;
    void applyFillPattern() override;
#endif

    void drawGlyphs(const Font&, const GlyphBufferGlyph*, const GlyphBufferAdvance*, unsigned numGlyphs, const FloatPoint& anchorPoint, FontSmoothingMode) override;

    void appendDrawGraphsItemWithCachedFont(const Font&, const GlyphBufferGlyph*, const GlyphBufferAdvance*, unsigned count, const FloatPoint& localAnchor, FontSmoothingMode);

    void drawImageBuffer(WebCore::ImageBuffer&, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions&) override;
    void drawNativeImage(NativeImage&, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions&) override;
    void drawPattern(NativeImage&, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions&) override;

    void drawRect(const FloatRect&, float borderThickness) override;
    void drawLine(const FloatPoint&, const FloatPoint&) override;
    void drawLinesForText(const FloatPoint&, float thickness, const DashArray& widths, bool printing, bool doubleLines) override;
    void drawDotsForDocumentMarker(const FloatRect&, DocumentMarkerLineStyle) override;
    void drawEllipse(const FloatRect&) override;
    void drawPath(const Path&) override;

    void drawFocusRing(const Path&, float width, float offset, const Color&) override;
    void drawFocusRing(const Vector<FloatRect>&, float width, float offset, const Color&) override;

    void save() override;
    void restore() override;

    void translate(float x, float y) override;
    void rotate(float angleInRadians) override;
    void scale(const FloatSize&) override;
    void concatCTM(const AffineTransform&) override;
    void setCTM(const AffineTransform&) override;
    AffineTransform getCTM(GraphicsContext::IncludeDeviceScale) override;

    void beginTransparencyLayer(float opacity) override;
    void endTransparencyLayer() override;

    void clip(const FloatRect&) override;
    void clipOut(const FloatRect&) override;
    void clipOut(const Path&) override;
    void clipPath(const Path&, WindRule) override;
    IntRect clipBounds() override;
    void clipToImageBuffer(WebCore::ImageBuffer&, const FloatRect&) override;
    void clipToDrawingCommands(const FloatRect& destination, DestinationColorSpace, Function<void(GraphicsContext&)>&&) override;
    void paintFrameForMedia(MediaPlayer&, const FloatRect& destination) override;
    bool canPaintFrameForMedia(const MediaPlayer&) const override;

    void applyDeviceScaleFactor(float) override;

    FloatRect roundToDevicePixels(const FloatRect&, GraphicsContext::RoundingMode) override;

    template<typename T, class... Args>
    void append(Args&&... args)
    {
        willAppendItemOfType(T::itemType);
        m_displayList.append<T>(std::forward<Args>(args)...);

        if constexpr (T::isDrawingItem) {
            if (LIKELY(!m_displayList.tracksDrawingItemExtents()))
                return;

            auto item = T(std::forward<Args>(args)...);
            if (auto rect = item.localBounds(graphicsContext()))
                m_displayList.addDrawingItemExtent(extentFromLocalBounds(*rect));
            else if (auto rect = item.globalBounds())
                m_displayList.addDrawingItemExtent(*rect);
            else
                m_displayList.addDrawingItemExtent(WTF::nullopt);
        }
    }

    WEBCORE_EXPORT void willAppendItemOfType(ItemType);

    void cacheNativeImage(NativeImage&);

    void appendStateChangeItem(const GraphicsContextStateChange&, GraphicsContextState::StateChangeFlags);

    FloatRect extentFromLocalBounds(const FloatRect&) const;
    
    const AffineTransform& ctm() const;
    const FloatRect& clipBounds() const;

    struct ContextState {
        AffineTransform ctm;
        FloatRect clipBounds;
        GraphicsContextStateChange stateChange;
        GraphicsContextState lastDrawingState;
        bool wasUsedForDrawing { false };
        
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

    Vector<ContextState, 32> m_stateStack;

    DrawGlyphsRecorder m_drawGlyphsRecorder;
};

}
}

