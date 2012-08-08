/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef ScrollbarLayerChromium_h
#define ScrollbarLayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerChromium.h"
#include "LayerTextureUpdater.h"
#include <public/WebScrollbar.h>
#include <public/WebScrollbarThemeGeometry.h>
#include <public/WebScrollbarThemePainter.h>

namespace WebCore {

class Scrollbar;
class ScrollbarThemeComposite;
class CCTextureUpdateQueue;

class ScrollbarLayerChromium : public LayerChromium {
public:
    virtual PassOwnPtr<CCLayerImpl> createCCLayerImpl() OVERRIDE;
    static PassRefPtr<ScrollbarLayerChromium> create(PassOwnPtr<WebKit::WebScrollbar>, WebKit::WebScrollbarThemePainter, PassOwnPtr<WebKit::WebScrollbarThemeGeometry>, int scrollLayerId);

    // LayerChromium interface
    virtual void setTexturePriorities(const CCPriorityCalculator&) OVERRIDE;
    virtual void update(CCTextureUpdateQueue&, const CCOcclusionTracker*, CCRenderingStats&) OVERRIDE;
    virtual void setLayerTreeHost(CCLayerTreeHost*) OVERRIDE;
    virtual void pushPropertiesTo(CCLayerImpl*) OVERRIDE;

    int scrollLayerId() const { return m_scrollLayerId; }
    void setScrollLayerId(int id) { m_scrollLayerId = id; }

    virtual ScrollbarLayerChromium* toScrollbarLayerChromium() OVERRIDE { return this; }

protected:
    ScrollbarLayerChromium(PassOwnPtr<WebKit::WebScrollbar>, WebKit::WebScrollbarThemePainter, PassOwnPtr<WebKit::WebScrollbarThemeGeometry>, int scrollLayerId);

private:
    void updatePart(LayerTextureUpdater*, LayerTextureUpdater::Texture*, const IntRect&, CCTextureUpdateQueue&, CCRenderingStats&);
    void createTextureUpdaterIfNeeded();

    OwnPtr<WebKit::WebScrollbar> m_scrollbar;
    WebKit::WebScrollbarThemePainter m_painter;
    OwnPtr<WebKit::WebScrollbarThemeGeometry> m_geometry;
    int m_scrollLayerId;

    GC3Denum m_textureFormat;

    RefPtr<LayerTextureUpdater> m_backTrackUpdater;
    RefPtr<LayerTextureUpdater> m_foreTrackUpdater;
    RefPtr<LayerTextureUpdater> m_thumbUpdater;

    // All the parts of the scrollbar except the thumb
    OwnPtr<LayerTextureUpdater::Texture> m_backTrack;
    OwnPtr<LayerTextureUpdater::Texture> m_foreTrack;
    OwnPtr<LayerTextureUpdater::Texture> m_thumb;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
