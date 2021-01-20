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
#import "WebPageProxy.h"
#import <UIKit/UIScrollView.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>

namespace WebKit {
using namespace WebCore;

std::unique_ptr<RemoteLayerTreeNode> RemoteLayerTreeHost::makeNode(const RemoteLayerTreeTransaction::LayerCreationProperties& properties)
{
    auto makeWithView = [&] (RetainPtr<UIView> view) {
        return makeUnique<RemoteLayerTreeNode>(properties.layerID, WTFMove(view));
    };
    auto makeAdoptingView = [&] (UIView* view) {
        return makeWithView(adoptNS(view));
    };

    switch (properties.type) {
    case PlatformCALayer::LayerTypeLayer:
    case PlatformCALayer::LayerTypeWebLayer:
    case PlatformCALayer::LayerTypeRootLayer:
    case PlatformCALayer::LayerTypeSimpleLayer:
    case PlatformCALayer::LayerTypeTiledBackingLayer:
    case PlatformCALayer::LayerTypePageTiledBackingLayer:
        return makeAdoptingView([[WKCompositingView alloc] init]);

    case PlatformCALayer::LayerTypeTiledBackingTileLayer:
        return RemoteLayerTreeNode::createWithPlainLayer(properties.layerID);

    case PlatformCALayer::LayerTypeBackdropLayer:
        return makeAdoptingView([[WKSimpleBackdropView alloc] init]);

    case PlatformCALayer::LayerTypeLightSystemBackdropLayer:
        return makeAdoptingView([[WKBackdropView alloc] initWithFrame:CGRectZero privateStyle:_UIBackdropViewStyle_Light]);

    case PlatformCALayer::LayerTypeDarkSystemBackdropLayer:
        return makeAdoptingView([[WKBackdropView alloc] initWithFrame:CGRectZero privateStyle:_UIBackdropViewStyle_Dark]);

    case PlatformCALayer::LayerTypeTransformLayer:
        return makeAdoptingView([[WKTransformView alloc] init]);

    case PlatformCALayer::LayerTypeCustom:
    case PlatformCALayer::LayerTypeAVPlayerLayer:
    case PlatformCALayer::LayerTypeContentsProvidedLayer:
        if (!m_isDebugLayerTreeHost) {
            auto view = adoptNS([[WKUIRemoteView alloc] initWithFrame:CGRectZero
                pid:m_drawingArea->page().processIdentifier() contextID:properties.hostingContextID]);
            if (properties.type == PlatformCALayer::LayerTypeAVPlayerLayer) {
                // Invert the scale transform added in the WebProcess to fix <rdar://problem/18316542>.
                float inverseScale = 1 / properties.hostingDeviceScaleFactor;
                [[view layer] setTransform:CATransform3DMakeScale(inverseScale, inverseScale, 1)];
            }
            return makeWithView(WTFMove(view));
        }
        return makeAdoptingView([[WKCompositingView alloc] init]);

    case PlatformCALayer::LayerTypeShapeLayer:
        return makeAdoptingView([[WKShapeView alloc] init]);

    case PlatformCALayer::LayerTypeScrollContainerLayer:
        if (!m_isDebugLayerTreeHost)
            return makeAdoptingView([[WKChildScrollView alloc] init]);
        // The debug indicator parents views under layers, which can cause crashes with UIScrollView.
        return makeAdoptingView([[UIView alloc] init]);

    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
