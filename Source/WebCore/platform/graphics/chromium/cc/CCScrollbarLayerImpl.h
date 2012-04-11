/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCScrollbarLayerImpl_h
#define CCScrollbarLayerImpl_h

#if USE(ACCELERATED_COMPOSITING)

#include "CCLayerImpl.h"
#include "ManagedTexture.h"
#include "ScrollbarThemeClient.h"

namespace WebCore {

class GraphicsContext;
class ScrollView;

class CCScrollbarLayerImpl : public CCLayerImpl {
public:
    static PassOwnPtr<CCScrollbarLayerImpl> create(int id);

    void setScrollbarOverlayStyle(ScrollbarOverlayStyle scrollbarOverlayStyle) { m_scrollbarOverlayStyle = scrollbarOverlayStyle; }
    void setTickmarks(const Vector<IntRect>& tickmarks) { m_tickmarks = tickmarks; }
    void setIsScrollableAreaActive(bool isScrollableAreaActive) { m_isScrollableAreaActive = isScrollableAreaActive; }
    void setIsScrollViewScrollbar(bool isScrollViewScrollbar) { m_isScrollViewScrollbar = isScrollViewScrollbar; }

    void setOrientation(ScrollbarOrientation orientation) { m_orientation = orientation; }

    void setControlSize(ScrollbarControlSize controlSize) { m_controlSize = controlSize; }

    void setPressedPart(ScrollbarPart pressedPart) { m_pressedPart = pressedPart; }
    void setHoveredPart(ScrollbarPart hoveredPart) { m_hoveredPart = hoveredPart; }

    void setEnabled(bool enabled) { m_enabled = enabled; }


    CCLayerImpl* scrollLayer() const { return m_scrollLayer; }
    void setScrollLayer(CCLayerImpl* scrollLayer) { m_scrollLayer = scrollLayer; }

    virtual void willDraw(LayerRendererChromium*) OVERRIDE;
    virtual void appendQuads(CCQuadCuller&, const CCSharedQuadState*, bool& hadMissingTiles) OVERRIDE;
    virtual void didDraw() OVERRIDE;

protected:
    explicit CCScrollbarLayerImpl(int id);

    virtual void paint(GraphicsContext*);

private:
    CCLayerImpl* m_scrollLayer;
    OwnPtr<ManagedTexture> m_texture;

    // nested class only to avoid namespace problem
    class CCScrollbar : public ScrollbarThemeClient {
    public:
        explicit CCScrollbar(CCScrollbarLayerImpl* owner) : m_owner(owner) { }

        // ScrollbarThemeClient implementation
        virtual int x() const;
        virtual int y() const;
        virtual int width() const;
        virtual int height() const;
        virtual IntSize size() const;
        virtual IntPoint location() const;

        virtual ScrollView* parent() const;
        virtual ScrollView* root() const;

        virtual void setFrameRect(const IntRect&);
        virtual IntRect frameRect() const;

        virtual void invalidate();
        virtual void invalidateRect(const IntRect&);

        virtual ScrollbarOverlayStyle scrollbarOverlayStyle() const;
        virtual void getTickmarks(Vector<IntRect>&) const;
        virtual bool isScrollableAreaActive() const;
        virtual bool isScrollViewScrollbar() const;

        virtual IntPoint convertFromContainingWindow(const IntPoint& windowPoint);

        virtual bool isCustomScrollbar() const;
        virtual ScrollbarOrientation orientation() const;

        virtual int value() const;
        virtual float currentPos() const;
        virtual int visibleSize() const;
        virtual int totalSize() const;
        virtual int maximum() const;
        virtual ScrollbarControlSize controlSize() const;

        virtual int lineStep() const;
        virtual int pageStep() const;

        virtual ScrollbarPart pressedPart() const;
        virtual ScrollbarPart hoveredPart() const;

        virtual void styleChanged();

        virtual bool enabled() const;
        virtual void setEnabled(bool);

    private:
        CCScrollbarLayerImpl* m_owner;

    };
    CCScrollbar m_scrollbar;

    ScrollbarOverlayStyle m_scrollbarOverlayStyle;
    Vector<IntRect> m_tickmarks;
    bool m_isScrollableAreaActive;
    bool m_isScrollViewScrollbar;

    ScrollbarOrientation m_orientation;

    ScrollbarControlSize m_controlSize;

    ScrollbarPart m_pressedPart;
    ScrollbarPart m_hoveredPart;

    bool m_enabled;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
