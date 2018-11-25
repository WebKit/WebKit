/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RemoteLayerTreeNode.h"

#import <WebCore/WebActionDisablingCALayerDelegate.h>

namespace WebKit {

RemoteLayerTreeNode::RemoteLayerTreeNode(WebCore::GraphicsLayer::PlatformLayerID layerID, RetainPtr<CALayer> layer)
    : m_layer(WTFMove(layer))
{
    setLayerID(layerID);
    [m_layer setDelegate:[WebActionDisablingCALayerDelegate shared]];
}

#if PLATFORM(IOS_FAMILY)
RemoteLayerTreeNode::RemoteLayerTreeNode(WebCore::GraphicsLayer::PlatformLayerID layerID, RetainPtr<UIView> uiView)
    : m_layer([uiView.get() layer])
    , m_uiView(WTFMove(uiView))
{
    setLayerID(layerID);
}
#endif

RemoteLayerTreeNode::~RemoteLayerTreeNode() = default;

void RemoteLayerTreeNode::detachFromParent()
{
#if PLATFORM(IOS_FAMILY)
    [uiView() removeFromSuperview];
#else
    [layer() removeFromSuperlayer];
#endif
}

static NSString* const WKLayerIDPropertyKey = @"WKLayerID";

void RemoteLayerTreeNode::setLayerID(WebCore::GraphicsLayer::PlatformLayerID layerID)
{
    [layer() setValue:@(layerID) forKey:WKLayerIDPropertyKey];
}

WebCore::GraphicsLayer::PlatformLayerID RemoteLayerTreeNode::layerID(CALayer* layer)
{
    return [[layer valueForKey:WKLayerIDPropertyKey] unsignedLongLongValue];
}

}
