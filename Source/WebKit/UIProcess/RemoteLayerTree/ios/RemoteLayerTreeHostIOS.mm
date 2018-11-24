/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "RemoteLayerTreeHost.h"

#if PLATFORM(IOS_FAMILY)

#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteLayerTreeViews.h"
#import "UIKitSPI.h"
#import "WKDrawingView.h"
#import "WebPageProxy.h"
#import <UIKit/UIScrollView.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/SoftLinking.h>

using namespace WebCore;

namespace WebKit {

static RetainPtr<UIView> createRemoteView(pid_t pid, uint32_t contextID)
{
#if USE(UIREMOTEVIEW_CONTEXT_HOSTING)
    // FIXME: Remove this respondsToSelector check when possible.
    static BOOL canUseUIRemoteView;
    static std::once_flag initializeCanUseUIRemoteView;
    std::call_once(initializeCanUseUIRemoteView, [] {
        canUseUIRemoteView = [_UIRemoteView instancesRespondToSelector:@selector(initWithFrame:pid:contextID:)];
    });
    if (canUseUIRemoteView)
        return adoptNS([[WKUIRemoteView alloc] initWithFrame:CGRectZero pid:pid contextID:contextID]);
#else
    UNUSED_PARAM(pid);
#endif
    return adoptNS([[WKRemoteView alloc] initWithFrame:CGRectZero contextID:contextID]);
}

void RemoteLayerTreeHost::createLayer(const RemoteLayerTreeTransaction::LayerCreationProperties& properties, const RemoteLayerTreeTransaction::LayerProperties* layerProperties)
{
    ASSERT(!m_nodes.contains(properties.layerID));

    RetainPtr<UIView> view;

    switch (properties.type) {
    case PlatformCALayer::LayerTypeLayer:
    case PlatformCALayer::LayerTypeWebLayer:
    case PlatformCALayer::LayerTypeRootLayer:
    case PlatformCALayer::LayerTypeSimpleLayer:
    case PlatformCALayer::LayerTypeTiledBackingLayer:
    case PlatformCALayer::LayerTypePageTiledBackingLayer:
    case PlatformCALayer::LayerTypeTiledBackingTileLayer:
        view = adoptNS([[WKCompositingView alloc] init]);
        break;
    case PlatformCALayer::LayerTypeBackdropLayer:
        view = adoptNS([[WKSimpleBackdropView alloc] init]);
        break;
    case PlatformCALayer::LayerTypeLightSystemBackdropLayer:
        view = adoptNS([[WKBackdropView alloc] initWithFrame:CGRectZero privateStyle:_UIBackdropViewStyle_Light]);
        break;
    case PlatformCALayer::LayerTypeDarkSystemBackdropLayer:
        view = adoptNS([[WKBackdropView alloc] initWithFrame:CGRectZero privateStyle:_UIBackdropViewStyle_Dark]);
        break;
    case PlatformCALayer::LayerTypeTransformLayer:
        view = adoptNS([[WKTransformView alloc] init]);
        break;
    case PlatformCALayer::LayerTypeCustom:
    case PlatformCALayer::LayerTypeAVPlayerLayer:
    case PlatformCALayer::LayerTypeContentsProvidedLayer:
        if (!m_isDebugLayerTreeHost) {
            view = createRemoteView(m_drawingArea->page().processIdentifier(), properties.hostingContextID);
            if (properties.type == PlatformCALayer::LayerTypeAVPlayerLayer) {
                // Invert the scale transform added in the WebProcess to fix <rdar://problem/18316542>.
                float inverseScale = 1 / properties.hostingDeviceScaleFactor;
                [[view layer] setTransform:CATransform3DMakeScale(inverseScale, inverseScale, 1)];
            }
        } else
            view = adoptNS([[WKCompositingView alloc] init]);
        break;
    case PlatformCALayer::LayerTypeShapeLayer:
        view = adoptNS([[WKShapeView alloc] init]);
        break;
    case PlatformCALayer::LayerTypeScrollingLayer:
        if (!m_isDebugLayerTreeHost)
            view = adoptNS([[WKChildScrollView alloc] init]);
        else // The debug indicator parents views under layers, which can cause crashes with UIScrollView.
            view = adoptNS([[UIView alloc] init]);
        break;
    case PlatformCALayer::LayerTypeEditableImageLayer:
        view = createEmbeddedView(properties, layerProperties);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    setLayerID([view layer], properties.layerID);

    m_nodes.add(properties.layerID, std::make_unique<RemoteLayerTreeNode>(WTFMove(view)));
}

RetainPtr<WKEmbeddedView> RemoteLayerTreeHost::createEmbeddedView(const RemoteLayerTreeTransaction::LayerCreationProperties& properties, const RemoteLayerTreeTransaction::LayerProperties* layerProperties)
{
    Class embeddedViewClass = nil;
    switch (properties.type) {
    case PlatformCALayer::LayerTypeEditableImageLayer:
#if HAVE(PENCILKIT)
        embeddedViewClass = [WKDrawingView class];
#endif
        break;
    default:
        break;
    }

    if (!embeddedViewClass || m_isDebugLayerTreeHost)
        return adoptNS([[UIView alloc] init]);

    auto result = m_embeddedViews.ensure(properties.embeddedViewID, [&] {
        return adoptNS([[embeddedViewClass alloc] init]);
    });
    auto view = result.iterator->value;
    if (!result.isNewEntry)
        m_layerToEmbeddedViewMap.remove([view layerID]);
    [view setLayerID:properties.layerID];
    m_embeddedViews.set(properties.embeddedViewID, view);
    m_layerToEmbeddedViewMap.set(properties.layerID, properties.embeddedViewID);
    return view;
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
