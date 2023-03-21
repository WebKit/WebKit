/*
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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
#import "RemoteLayerTreeNode.h"

#import "RemoteLayerTreeLayers.h"
#import <QuartzCore/CALayer.h>
#import <WebCore/WebActionDisablingCALayerDelegate.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIView.h>
#endif

namespace WebKit {

static NSString *const WKRemoteLayerTreeNodePropertyKey = @"WKRemoteLayerTreeNode";

RemoteLayerTreeNode::RemoteLayerTreeNode(WebCore::PlatformLayerIdentifier layerID, Markable<WebCore::LayerHostingContextIdentifier> hostIdentifier, RetainPtr<CALayer> layer)
    : m_layerID(layerID)
    , m_remoteContextHostingIdentifier(hostIdentifier)
    , m_layer(WTFMove(layer))
{
    initializeLayer();
    [m_layer setDelegate:[WebActionDisablingCALayerDelegate shared]];
}

#if PLATFORM(IOS_FAMILY)
RemoteLayerTreeNode::RemoteLayerTreeNode(WebCore::PlatformLayerIdentifier layerID, Markable<WebCore::LayerHostingContextIdentifier> hostIdentifier, RetainPtr<UIView> uiView)
    : m_layerID(layerID)
    , m_remoteContextHostingIdentifier(hostIdentifier)
    , m_layer([uiView.get() layer])
    , m_uiView(WTFMove(uiView))
{
    initializeLayer();
}
#endif

RemoteLayerTreeNode::~RemoteLayerTreeNode()
{
    [layer() setValue:nil forKey:WKRemoteLayerTreeNodePropertyKey];
}

std::unique_ptr<RemoteLayerTreeNode> RemoteLayerTreeNode::createWithPlainLayer(WebCore::PlatformLayerIdentifier layerID)
{
    RetainPtr<CALayer> layer = adoptNS([[WKCompositingLayer alloc] init]);
    return makeUnique<RemoteLayerTreeNode>(layerID, std::nullopt, WTFMove(layer));
}

void RemoteLayerTreeNode::detachFromParent()
{
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    [interactionRegionsLayer() removeFromSuperlayer];
#endif

#if PLATFORM(IOS_FAMILY)
    if (auto view = uiView()) {
        [view removeFromSuperview];
        return;
    }
#endif
    [layer() removeFromSuperlayer];
}

void RemoteLayerTreeNode::setEventRegion(const WebCore::EventRegion& eventRegion)
{
    m_eventRegion = eventRegion;
}

void RemoteLayerTreeNode::initializeLayer()
{
    [layer() setValue:[NSValue valueWithPointer:this] forKey:WKRemoteLayerTreeNodePropertyKey];
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    m_interactionRegionsLayer = adoptNS([[CALayer alloc] init]);
    [m_interactionRegionsLayer setName:@"InteractionRegions Container"];
    [m_interactionRegionsLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];
#endif
}

WebCore::PlatformLayerIdentifier RemoteLayerTreeNode::layerID(CALayer *layer)
{
    auto* node = forCALayer(layer);
    return node ? node->layerID() : WebCore::PlatformLayerIdentifier { };
}

RemoteLayerTreeNode* RemoteLayerTreeNode::forCALayer(CALayer *layer)
{
    return static_cast<RemoteLayerTreeNode*>([[layer valueForKey:WKRemoteLayerTreeNodePropertyKey] pointerValue]);
}

NSString *RemoteLayerTreeNode::appendLayerDescription(NSString *description, CALayer *layer)
{
    NSString *layerDescription = [NSString stringWithFormat:@" layerID = %llu \"%@\"", WebKit::RemoteLayerTreeNode::layerID(layer).object().toUInt64(), layer.name ? layer.name : @""];
    return [description stringByAppendingString:layerDescription];
}

}
