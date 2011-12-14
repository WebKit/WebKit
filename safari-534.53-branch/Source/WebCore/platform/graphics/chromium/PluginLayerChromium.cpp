/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#if USE(ACCELERATED_COMPOSITING)

#include "PluginLayerChromium.h"

#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCPluginLayerImpl.h"

namespace WebCore {

PassRefPtr<PluginLayerChromium> PluginLayerChromium::create(GraphicsLayerChromium* owner)
{
    return adoptRef(new PluginLayerChromium(owner));
}

PluginLayerChromium::PluginLayerChromium(GraphicsLayerChromium* owner)
    : LayerChromium(owner)
    , m_textureId(0)
{
}

PassRefPtr<CCLayerImpl> PluginLayerChromium::createCCLayerImpl()
{
    return CCPluginLayerImpl::create(this, m_layerId);
}

void PluginLayerChromium::setTextureId(unsigned id)
{
    m_textureId = id;
}

void PluginLayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    LayerChromium::pushPropertiesTo(layer);

    CCPluginLayerImpl* pluginLayer = static_cast<CCPluginLayerImpl*>(layer);
    pluginLayer->setTextureId(m_textureId);
}

}
#endif // USE(ACCELERATED_COMPOSITING)
