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

#ifndef DisplayListRecorder_h
#define DisplayListRecorder_h

#include "DisplayList.h"
#include "GraphicsContext.h" // For InterpolationQuality.
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

class Recorder {
    WTF_MAKE_NONCOPYABLE(Recorder);
public:
    Recorder(GraphicsContext&, DisplayList&, const FloatRect& initialClip, const AffineTransform&);
    ~Recorder();

    void updateState(const GraphicsContextState&, GraphicsContextState::StateChangeFlags);
    void clearShadow();

    void setLineCap(LineCap);
    void setLineDash(const DashArray&, float dashOffset);
    void setLineJoin(LineJoin);
    void setMiterLimit(float);

    void fillRect(const FloatRect&);
    void fillRect(const FloatRect&, const Color&);
    void fillRect(const FloatRect&, Gradient&);
    void fillRect(const FloatRect&, const Color&, CompositeOperator, BlendMode);
    void fillRoundedRect(const FloatRoundedRect&, const Color&, BlendMode);
    void fillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect& roundedHoleRect, const Color&);
    void fillPath(const Path&);
    void fillEllipse(const FloatRect&);
    void strokeRect(const FloatRect&, float lineWidth);
    void strokePath(const Path&);
    void strokeEllipse(const FloatRect&);
    void clearRect(const FloatRect&);

#if USE(CG)
    void applyStrokePattern();
    void applyFillPattern();
#endif

    void drawGlyphs(const Font&, const GlyphBuffer&, unsigned from, unsigned numGlyphs, const FloatPoint& anchorPoint, FontSmoothingMode);

    void drawImage(Image&, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions&);
    void drawTiledImage(Image&, const FloatRect& destination, const FloatPoint& source, const FloatSize& tileSize, const FloatSize& spacing, const ImagePaintingOptions&);
    void drawTiledImage(Image&, const FloatRect& destination, const FloatRect& source, const FloatSize& tileScaleFactor, Image::TileRule hRule, Image::TileRule vRule, const ImagePaintingOptions&);
#if USE(CG) || USE(CAIRO)
    void drawNativeImage(const NativeImagePtr&, const FloatSize& selfSize, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator, BlendMode, ImageOrientation);
#endif
    void drawPattern(Image&, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, CompositeOperator, BlendMode = BlendModeNormal);

    void drawRect(const FloatRect&, float borderThickness);
    void drawLine(const FloatPoint&, const FloatPoint&);
    void drawLinesForText(const FloatPoint&, const DashArray& widths, bool printing, bool doubleLines, float strokeThickness);
    void drawLineForDocumentMarker(const FloatPoint&, float width, GraphicsContext::DocumentMarkerLineStyle);
    void drawEllipse(const FloatRect&);
    void drawPath(const Path&);

    void drawFocusRing(const Path&, int width, int offset, const Color&);
    void drawFocusRing(const Vector<FloatRect>&, int width, int offset, const Color&);

    void save();
    void restore();

    void translate(float x, float y);
    void rotate(float angleInRadians);
    void scale(const FloatSize&);
    void concatCTM(const AffineTransform&);

    void beginTransparencyLayer(float opacity);
    void endTransparencyLayer();

    void clip(const FloatRect&);
    void clipOut(const FloatRect&);
    void clipOut(const Path&);
    void clipPath(const Path&, WindRule);
    
    void applyDeviceScaleFactor(float);

    size_t itemCount() const { return m_displayList.itemCount(); }

private:
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
        
        ContextState(const AffineTransform& transform, const FloatRect& clip)
            : ctm(transform)
            , clipBounds(clip)
        {
        }
        
        ContextState cloneForSave(size_t saveIndex) const
        {
            ContextState state(ctm, clipBounds);
            state.stateChange = stateChange;
            state.lastDrawingState = lastDrawingState;
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

    GraphicsContext& m_graphicsContext;
    DisplayList& m_displayList;

    Vector<ContextState, 32> m_stateStack;
};

}
}

#endif // DisplayListRecorder_h
