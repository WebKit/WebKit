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
#include <public/WebRect.h>
#include <public/WebScrollbar.h>
#include <public/WebScrollbarThemeGeometry.h>
#include <public/WebVector.h>

namespace WebCore {

class ScrollView;

class CCScrollbarLayerImpl : public CCLayerImpl {
public:
    static PassOwnPtr<CCScrollbarLayerImpl> create(int id);

    WebKit::WebScrollbarThemeGeometry* scrollbarGeometry() const { return m_geometry.get(); }
    void setScrollbarGeometry(PassOwnPtr<WebKit::WebScrollbarThemeGeometry>);
    void setScrollbarData(const WebKit::WebScrollbar*);

    void setBackTrackResourceId(CCResourceProvider::ResourceId id) { m_backTrackResourceId = id; }
    void setForeTrackResourceId(CCResourceProvider::ResourceId id) { m_foreTrackResourceId = id; }
    void setThumbResourceId(CCResourceProvider::ResourceId id) { m_thumbResourceId = id; }

    float currentPos() const { return m_currentPos; }
    void setCurrentPos(float currentPos) { m_currentPos = currentPos; }

    int totalSize() const { return m_totalSize; }
    void setTotalSize(int totalSize) { m_totalSize = totalSize; }

    int maximum() const { return m_maximum; }
    void setMaximum(int maximum) { m_maximum = maximum; }

    WebKit::WebScrollbar::Orientation orientation() const { return m_orientation; }

    virtual void appendQuads(CCQuadSink&, const CCSharedQuadState*, bool& hadMissingTiles) OVERRIDE;

    virtual void didLoseContext() OVERRIDE;

protected:
    explicit CCScrollbarLayerImpl(int id);

private:
    // nested class only to avoid namespace problem
    class CCScrollbar : public WebKit::WebScrollbar {
    public:
        explicit CCScrollbar(CCScrollbarLayerImpl* owner) : m_owner(owner) { }

        // WebScrollbar implementation
        virtual bool isOverlay() const;
        virtual int value() const;
        virtual WebKit::WebPoint location() const;
        virtual WebKit::WebSize size() const;
        virtual bool enabled() const;
        virtual int maximum() const;
        virtual int totalSize() const;
        virtual bool isScrollViewScrollbar() const;
        virtual bool isScrollableAreaActive() const;
        virtual void getTickmarks(WebKit::WebVector<WebKit::WebRect>& tickmarks) const;
        virtual WebScrollbar::ScrollbarControlSize controlSize() const;
        virtual WebScrollbar::ScrollbarPart pressedPart() const;
        virtual WebScrollbar::ScrollbarPart hoveredPart() const;
        virtual WebScrollbar::ScrollbarOverlayStyle scrollbarOverlayStyle() const;
        virtual WebScrollbar::Orientation orientation() const;
        virtual bool isCustomScrollbar() const;

    private:
        CCScrollbarLayerImpl* m_owner;

    };

    CCScrollbar m_scrollbar;

    CCResourceProvider::ResourceId m_backTrackResourceId;
    CCResourceProvider::ResourceId m_foreTrackResourceId;
    CCResourceProvider::ResourceId m_thumbResourceId;

    OwnPtr<WebKit::WebScrollbarThemeGeometry> m_geometry;

    // Data to implement CCScrollbar
    WebKit::WebScrollbar::ScrollbarOverlayStyle m_scrollbarOverlayStyle;
    WebKit::WebVector<WebKit::WebRect> m_tickmarks;
    WebKit::WebScrollbar::Orientation m_orientation;
    WebKit::WebScrollbar::ScrollbarControlSize m_controlSize;
    WebKit::WebScrollbar::ScrollbarPart m_pressedPart;
    WebKit::WebScrollbar::ScrollbarPart m_hoveredPart;

    float m_currentPos;
    int m_totalSize;
    int m_maximum;

    bool m_isScrollableAreaActive;
    bool m_isScrollViewScrollbar;
    bool m_enabled;
    bool m_isCustomScrollbar;
    bool m_isOverlayScrollbar;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
