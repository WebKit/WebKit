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
#import "VideoPresentationManagerProxy.h"
#import "WKVideoView.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import <UIKit/UIScrollView.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)
#import "WKModelView.h"
#endif

namespace WebKit {
using namespace WebCore;

RefPtr<RemoteLayerTreeNode> RemoteLayerTreeHost::makeNode(const RemoteLayerTreeTransaction::LayerCreationProperties& properties)
{
    auto makeWithView = [&] (RetainPtr<UIView>&& view) {
        return RemoteLayerTreeNode::create(*properties.layerID, properties.hostIdentifier(), WTFMove(view));
    };

    switch (properties.type) {
    case PlatformCALayer::LayerType::LayerTypeLayer:
    case PlatformCALayer::LayerType::LayerTypeWebLayer:
    case PlatformCALayer::LayerType::LayerTypeRootLayer:
    case PlatformCALayer::LayerType::LayerTypeSimpleLayer:
    case PlatformCALayer::LayerType::LayerTypeTiledBackingLayer:
    case PlatformCALayer::LayerType::LayerTypePageTiledBackingLayer:
    case PlatformCALayer::LayerType::LayerTypeContentsProvidedLayer:
    case PlatformCALayer::LayerType::LayerTypeHost:
        return makeWithView(adoptNS([[WKCompositingView alloc] init]));

    case PlatformCALayer::LayerType::LayerTypeTiledBackingTileLayer:
        return RemoteLayerTreeNode::createWithPlainLayer(*properties.layerID);

    case PlatformCALayer::LayerType::LayerTypeBackdropLayer:
        return makeWithView(adoptNS([[WKBackdropView alloc] init]));

    case PlatformCALayer::LayerType::LayerTypeTransformLayer:
        return makeWithView(adoptNS([[WKTransformView alloc] init]));

    case PlatformCALayer::LayerType::LayerTypeCustom:
    case PlatformCALayer::LayerType::LayerTypeAVPlayerLayer: {
        if (m_isDebugLayerTreeHost)
            return makeWithView(adoptNS([[WKCompositingView alloc] init]));

#if HAVE(AVKIT)
        if (properties.videoElementData) {
            if (auto videoManager = m_drawingArea->page().videoPresentationManager()) {
                m_videoLayers.add(*properties.layerID, properties.videoElementData->playerIdentifier);
                return makeWithView(videoManager->createViewWithID(properties.videoElementData->playerIdentifier, properties.hostingContextID(), properties.videoElementData->initialSize, properties.videoElementData->naturalSize, properties.hostingDeviceScaleFactor()));
            }
        }
#endif

        auto view = adoptNS([[WKUIRemoteView alloc] initWithFrame:CGRectZero pid:m_drawingArea->page().legacyMainFrameProcessID() contextID:properties.hostingContextID()]);
        return makeWithView(WTFMove(view));
    }
    case PlatformCALayer::LayerType::LayerTypeShapeLayer:
        return makeWithView(adoptNS([[WKShapeView alloc] init]));

    case PlatformCALayer::LayerType::LayerTypeScrollContainerLayer:
        if (!m_isDebugLayerTreeHost)
            return makeWithView(adoptNS([[WKChildScrollView alloc] init]));
        // The debug indicator parents views under layers, which can cause crashes with UIScrollView.
        return makeWithView(adoptNS([[UIView alloc] init]));

#if ENABLE(MODEL_ELEMENT)
    case PlatformCALayer::LayerType::LayerTypeModelLayer:
#if ENABLE(MODEL_PROCESS)
        bool modelHandledOutOfProcess = m_drawingArea->page().preferences().modelProcessEnabled();
#else
        bool modelHandledOutOfProcess = false;
#endif

        if (!modelHandledOutOfProcess && m_drawingArea->page().preferences().modelElementEnabled()) {
            if (auto* model = std::get_if<Ref<Model>>(&properties.additionalData)) {
#if ENABLE(SEPARATED_MODEL)
                return makeWithView(adoptNS([[WKSeparatedModelView alloc] initWithModel:*model]));
#elif ENABLE(ARKIT_INLINE_PREVIEW_IOS)
                return makeWithView(adoptNS([[WKModelView alloc] initWithModel:*model layerID:*properties.layerID page:m_drawingArea->page()]));
#endif
            }
        }
        return makeWithView(adoptNS([[WKCompositingView alloc] init]));
#endif // ENABLE(MODEL_ELEMENT)
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
