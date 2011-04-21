/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef CCHeadsUpDisplay_h
#define CCHeadsUpDisplay_h

#if USE(ACCELERATED_COMPOSITING)

#include "Font.h"
#include "ProgramBinding.h"
#include "ShaderChromium.h"

namespace WebCore {

class GeometryBinding;
class GraphicsContext3D;
class LayerRendererChromium;
class LayerTexture;

// Class that handles drawing of composited render layers using GL.
class CCHeadsUpDisplay {
    WTF_MAKE_NONCOPYABLE(CCHeadsUpDisplay);
public:
    static PassOwnPtr<CCHeadsUpDisplay> create(LayerRendererChromium* owner)
    {
        return adoptPtr(new CCHeadsUpDisplay(owner));
    }

    ~CCHeadsUpDisplay();

    void onPresent();

    void setShowFPSCounter(bool enable) { m_showFPSCounter = enable; }
    bool showFPSCounter() const { return m_showFPSCounter; }

    void setShowPlatformLayerTree(bool enable) { m_showPlatformLayerTree = enable; }
    bool showPlatformLayerTree() const { return m_showPlatformLayerTree; }

    bool enabled() const { return m_showPlatformLayerTree || m_showFPSCounter; }
    void draw();

    typedef ProgramBinding<VertexShaderPosTex, FragmentShaderBGRATexAlpha> Program;

private:
    explicit CCHeadsUpDisplay(LayerRendererChromium* owner);
    void drawHudContents(GraphicsContext*, const IntSize& hudSize);
    void drawFPSCounter(GraphicsContext*, int top, int height);
    void drawPlatformLayerTree(GraphicsContext*, int top);


    int m_currentFrameNumber;

    double m_filteredFrameTime;

    OwnPtr<LayerTexture> m_hudTexture;

    LayerRendererChromium* m_layerRenderer;

    static const int kPresentHistorySize = 64;
    double m_presentTimeHistoryInSec[kPresentHistorySize];

    bool m_showFPSCounter;
    bool m_showPlatformLayerTree;

    OwnPtr<Font> m_smallFont;
    OwnPtr<Font> m_mediumFont;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif
