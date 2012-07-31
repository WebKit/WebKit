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

#include "config.h"

#include "HeadsUpDisplayLayerChromium.h"

#include "TraceEvent.h"
#include "cc/CCHeadsUpDisplayLayerImpl.h"
#include "cc/CCLayerTreeHost.h"

namespace WebCore {

PassRefPtr<HeadsUpDisplayLayerChromium> HeadsUpDisplayLayerChromium::create(const CCLayerTreeSettings& settings)
{
    return adoptRef(new HeadsUpDisplayLayerChromium(settings));
}

HeadsUpDisplayLayerChromium::HeadsUpDisplayLayerChromium(const CCLayerTreeSettings& settings)
    : LayerChromium()
{

    setBounds(IntSize(512, 128));

    // Only allocate the font atlas if we have reason to use the heads-up display.
    if (settings.showFPSCounter || settings.showPlatformLayerTree) {
        TRACE_EVENT0("cc", "HeadsUpDisplayLayerChromium::initializeFontAtlas");
        m_fontAtlas = CCFontAtlas::create();
        m_fontAtlas->initialize();
    }
}

HeadsUpDisplayLayerChromium::~HeadsUpDisplayLayerChromium()
{
}

void HeadsUpDisplayLayerChromium::update(CCTextureUpdater&, const CCOcclusionTracker*, CCRenderingStats&)
{
    const CCLayerTreeSettings& settings = layerTreeHost()->settings();
    int maxTextureSize = layerTreeHost()->layerRendererCapabilities().maxTextureSize;

    IntSize bounds;
    if (settings.showPlatformLayerTree || settings.showDebugRects()) {
        bounds.setWidth(std::min(maxTextureSize, layerTreeHost()->deviceViewportSize().width()));
        bounds.setHeight(std::min(maxTextureSize, layerTreeHost()->deviceViewportSize().height()));
    } else {
        bounds.setWidth(512);
        bounds.setHeight(128);
    }

    setBounds(bounds);
}

PassOwnPtr<CCLayerImpl> HeadsUpDisplayLayerChromium::createCCLayerImpl()
{
    return CCHeadsUpDisplayLayerImpl::create(m_layerId, m_fontAtlas.release());
}

}
