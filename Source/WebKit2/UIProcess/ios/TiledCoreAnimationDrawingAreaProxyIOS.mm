/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "TiledCoreAnimationDrawingAreaProxyIOS.h"

#import "DrawingAreaMessages.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"

using namespace WebCore;

namespace WebKit {

TiledCoreAnimationDrawingAreaProxyIOS::TiledCoreAnimationDrawingAreaProxyIOS(WebPageProxy* webPageProxy)
    : DrawingAreaProxy(DrawingAreaTypeTiledCoreAnimationIOS, webPageProxy)
{
}
    
TiledCoreAnimationDrawingAreaProxyIOS::~TiledCoreAnimationDrawingAreaProxyIOS()
{
}

void TiledCoreAnimationDrawingAreaProxyIOS::deviceScaleFactorDidChange()
{
    m_webPageProxy->process().send(Messages::DrawingArea::SetDeviceScaleFactor(m_webPageProxy->deviceScaleFactor()), m_webPageProxy->pageID());
}

void TiledCoreAnimationDrawingAreaProxyIOS::sizeDidChange()
{
    m_webPageProxy->process().send(Messages::DrawingArea::UpdateGeometry(m_size, IntSize()), m_webPageProxy->pageID());
}

void TiledCoreAnimationDrawingAreaProxyIOS::enterAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext& layerTreeContext)
{
    m_webPageProxy->enterAcceleratedCompositingMode(layerTreeContext);
}

} // namespace WebKit
