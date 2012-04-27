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

#if USE(ACCELERATED_COMPOSITING)

#include "IOSurfaceLayerChromium.h"

#include "cc/CCIOSurfaceLayerImpl.h"

namespace WebCore {

PassRefPtr<IOSurfaceLayerChromium> IOSurfaceLayerChromium::create()
{
    return adoptRef(new IOSurfaceLayerChromium());
}

IOSurfaceLayerChromium::IOSurfaceLayerChromium()
    : LayerChromium()
    , m_ioSurfaceId(0)
{
}

IOSurfaceLayerChromium::~IOSurfaceLayerChromium()
{
}

void IOSurfaceLayerChromium::setIOSurfaceProperties(uint32_t ioSurfaceId, const IntSize& size)
{
    m_ioSurfaceId = ioSurfaceId;
    m_ioSurfaceSize = size;
    setNeedsCommit();
}

PassOwnPtr<CCLayerImpl> IOSurfaceLayerChromium::createCCLayerImpl()
{
    return CCIOSurfaceLayerImpl::create(m_layerId);
}

bool IOSurfaceLayerChromium::drawsContent() const
{
    return m_ioSurfaceId && LayerChromium::drawsContent();
}

void IOSurfaceLayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    LayerChromium::pushPropertiesTo(layer);

    CCIOSurfaceLayerImpl* textureLayer = static_cast<CCIOSurfaceLayerImpl*>(layer);
    textureLayer->setIOSurfaceProperties(m_ioSurfaceId, m_ioSurfaceSize);
}

}
#endif // USE(ACCELERATED_COMPOSITING)
