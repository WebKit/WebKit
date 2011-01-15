/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "LayerBackedDrawingAreaProxy.h"

#include "DrawingAreaMessageKinds.h"
#include "DrawingAreaProxyMessageKinds.h"
#include <QuartzCore/QuartzCore.h>
#include "WKAPICast.h"
#include "WKView.h"
#include "WKViewInternal.h"
#include "WebKitSystemInterface.h"
#include "WebPageProxy.h"

using namespace WebCore;

namespace WebKit {

WebPageProxy* LayerBackedDrawingAreaProxy::page()
{
    return toImpl([m_webView pageRef]);
}

void LayerBackedDrawingAreaProxy::platformSetSize()
{
    [m_compositingRootLayer.get() setBounds:CGRectMake(0, 0, size().width(), size().height())];
}

void LayerBackedDrawingAreaProxy::attachCompositingContext(uint32_t contextID)
{
#if HAVE(HOSTED_CORE_ANIMATION)
    m_compositingRootLayer = WKMakeRenderLayer(contextID);

    // Turn off default animations.
    NSNull *nullValue = [NSNull null];
    NSDictionary *actions = [NSDictionary dictionaryWithObjectsAndKeys:
                             nullValue, @"anchorPoint",
                             nullValue, @"bounds",
                             nullValue, @"contents",
                             nullValue, @"contentsRect",
                             nullValue, @"opacity",
                             nullValue, @"position",
                             nullValue, @"sublayerTransform",
                             nullValue, @"sublayers",
                             nullValue, @"transform",
                             nil];
    [m_compositingRootLayer.get() setStyle:[NSDictionary dictionaryWithObject:actions forKey:@"actions"]];
    
    [m_compositingRootLayer.get() setAnchorPoint:CGPointZero];
    [m_compositingRootLayer.get() setBounds:CGRectMake(0, 0, size().width(), size().height())];
    
    // FIXME: this fixes the layer jiggle when resizing the window, but breaks layer flipping because
    // CA doesn't propagate the kCALayerFlagFlippedAbove through the remote layer.
    // [m_compositingRootLayer.get() setGeometryFlipped:YES];

#ifndef NDEBUG
    [m_compositingRootLayer.get() setName:@"Compositing root layer"];
#endif

    [m_webView _startAcceleratedCompositing:m_compositingRootLayer.get()];
#endif
}

void LayerBackedDrawingAreaProxy::detachCompositingContext()
{
    [m_webView _stopAcceleratedCompositing];
    m_compositingRootLayer = 0;
}

} // namespace WebKit
