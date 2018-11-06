/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "GraphicsContextImpl.h"
#include "Image.h" // For Image::TileRule.
#include "TextFlags.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class FloatPoint;
class FloatRect;
class GlyphBuffer;
class FloatPoint;
class Font;
class Image;

struct GraphicsContextState;
struct ImagePaintingOptions;

namespace DisplayList {

class DrawingItem;

class Recorder : public GraphicsContextImpl {
    WTF_MAKE_NONCOPYABLE(Recorder);
public:
    Recorder(GraphicsContext&, DisplayList&, const GraphicsContextState&, const FloatRect& initialClip, const AffineTransform&);
    virtual ~Recorder();

    size_t itemCount() const { return m_displayList.itemCount(); }

private:
    bool hasPlatformContext() const override { return false; }
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

    void drawGlyphs(const Font&, const GlyphBuffer&, unsigned from, unsigned numGlyphs, const FloatPoint& anchorPoint, FontSmoothingMode) override;

    ImageDrawResult drawImage(Image&, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions&) override;
    ImageDrawResult drawTiledImage(Image&, const FloatRect& destination, const FloatPoint& source, const FloatSize& tileSize, const FloatSize& spacing, const ImagePaintingOptions&) override;
    ImageDrawResult drawTiledImage(Image&, const FloatRect& destination, const FloatRect& source, const FloatSize& tileScaleFactor, Image::TileRule hRule, Image::TileRule vRule, const ImagePaintingOptions&) override;
#if USE(CG) || USE(CAIRO)
    void drawNativeImage(const NativeImagePtr&, const FloatSize& selfSize, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator, BlendMode, ImageOrientation) override;
#endif
    void drawPattern(Image&, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, CompositeOperator, BlendMode = BlendMode::Normal) override;

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
    void clipToImageBuffer(ImageBuffer&, const FloatRect&) override;
    
    void applyDeviceScaleFactor(float) override;

    FloatRect roundToDevicePixels(const FloatRect&, GraphicsContext::RoundingMode) override;

    Item& appendItem(Ref<Item>&&);
    void willAppendItem(const Item&);

    FloatRect extentFromLocalBounds(const FloatRect&) const;
    void updateItemExtent(DrawingItem&) const;
    
    const AffineTransform& ctm() const;
    const FloatRect& clipBounds() const;

    struct ContextState {
        AffineTransform ctm;
        FloatRect clipBounds;
        GraphicsContextStateChange stateChange;
        GraphicsContextState lastDrawingState;
        bool wasUsedForDrawing { false };
        size_t saveItemIndex { 0 };
        
        ContextState(const GraphicsContextState& state, const AffineTransform& transform, const FloatRect& clip)
            : ctm(transform)
            , clipBounds(clip)
            , lastDrawingState(state)
        {
        }
        
        ContextState cloneForSave(size_t saveIndex) const
        {
            ContextState state(lastDrawingState, ctm, clipBounds);
            state.stateChange = stateChange;
            state.saveItemIndex = saveIndex;
            return state;
        }

        void translate(float x, float y);
        void rotate(float angleInRadians);
        void scale(const FloatSize&);
        void concatCTM(const AffineTransform&);
    };
    
    const ContextState& currentState() const;
    ContextState& currentState();

    DisplayList& m_displayList;

    Vector<ContextState, 32> m_stateStack;
};

}
}

