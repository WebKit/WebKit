/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#import "PlatformCALayerRemoteCustom.h"

#import "LayerHostingContext.h"
#import "RemoteLayerTreeContext.h"
#import "RemoteLayerTreePropertyApplier.h"
#import "WebProcess.h"
#import <WebCore/GraphicsLayerCA.h>
#import <WebCore/PlatformCALayerMac.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/WebCoreCALayerExtras.h>
#import <wtf/RetainPtr.h>

using namespace WebCore;
using namespace WebKit;

PlatformCALayerRemoteCustom::PlatformCALayerRemoteCustom(PlatformLayer* customLayer, PlatformCALayerClient* owner, RemoteLayerTreeContext* context)
    : PlatformCALayerRemote(LayerTypeCustom, owner, context)
{
    switch (context->layerHostingMode()) {
    case LayerHostingMode::InProcess:
        m_layerHostingContext = LayerHostingContext::createForPort(WebProcess::shared().compositingRenderServerPort());
        break;
#if HAVE(OUT_OF_PROCESS_LAYER_HOSTING)
    case LayerHostingMode::OutOfProcess:
        m_layerHostingContext = LayerHostingContext::createForExternalHostingProcess();
        break;
#endif
    }

    m_layerHostingContext->setRootLayer(customLayer);

    m_platformLayer = customLayer;
    [customLayer web_disableAllActions];
}

PlatformCALayerRemoteCustom::~PlatformCALayerRemoteCustom()
{
}

uint32_t PlatformCALayerRemoteCustom::hostingContextID()
{
    return m_layerHostingContext->contextID();
}
