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
#import <wtf/MachSendRightAnnotated.h>

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)
#import "WKModelView.h"
#endif

#if ENABLE(MODEL_PROCESS)
#import "ModelPresentationManagerProxy.h"
#import "WKPageHostedModelView.h"
#endif

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
#import "WKSeparatedImageView.h"
#endif

#if HAVE(MATERIAL_HOSTING)
#import "WKMaterialHostingSupport.h"
#endif

namespace WebKit {
using namespace WebCore;

RefPtr<RemoteLayerTreeNode> RemoteLayerTreeHost::makeNode(const RemoteLayerTreeTransaction::LayerCreationProperties& properties)
{
    auto makeWithView = [&] (RetainPtr<UIView>&& view) {
        return RemoteLayerTreeNode::create(*properties.layerID, properties.hostIdentifier(), WTF::move(view));
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

#if HAVE(CORE_MATERIAL)
    case PlatformCALayer::LayerType::LayerTypeMaterialLayer:
        return makeWithView(adoptNS([[WKMaterialView alloc] init]));
#endif

#if HAVE(MATERIAL_HOSTING)
    case PlatformCALayer::LayerType::LayerTypeMaterialHostingLayer: {
        if (![WKMaterialHostingSupport isMaterialHostingAvailable])
            return makeWithView(adoptNS([[WKCompositingView alloc] init]));

        return makeWithView(adoptNS([[WKMaterialHostingView alloc] init]));
    }
#endif

    case PlatformCALayer::LayerType::LayerTypeTransformLayer:
        return makeWithView(adoptNS([[WKTransformView alloc] init]));

    case PlatformCALayer::LayerType::LayerTypeCustom:
    case PlatformCALayer::LayerType::LayerTypeAVPlayerLayer: {
        if (m_isDebugLayerTreeHost)
            return makeWithView(adoptNS([[WKCompositingView alloc] init]));

#if HAVE(AVKIT)
        if (properties.videoElementData) {
            if (RefPtr page = protect(m_drawingArea)->page()) {
                if (RefPtr videoManager = page->videoPresentationManager()) {
                    m_videoLayers.add(*properties.layerID, properties.videoElementData->playerIdentifier);
                WebCore::HostingContext hostingContext;
                hostingContext.contextID = properties.hostingContextID();
#if ENABLE(MACH_PORT_LAYER_HOSTING)
                if (auto sendRightAnnotated = properties.sendRightAnnotated())
                    hostingContext.sendRightAnnotated = WTF::move(*sendRightAnnotated);
#endif
                return makeWithView(videoManager->createViewWithID(properties.videoElementData->playerIdentifier, WTF::move(hostingContext), properties.videoElementData->initialSize, properties.videoElementData->naturalSize, properties.hostingDeviceScaleFactor()));
                }
            }
        }
#endif

        if (!protect(m_drawingArea)->page())
            return nullptr;

#if ENABLE(MODEL_PROCESS)
        if (auto modelContext = properties.modelContext()) {
            if (auto modelPresentationManager = m_drawingArea->page() ? m_drawingArea->page()->modelPresentationManagerProxy() : nullptr) {
                if (auto view = modelPresentationManager->setUpModelView(*modelContext)) {
                    m_modelLayers.add(modelContext->modelLayerIdentifier());
                    return makeWithView(WTF::move(view));
                }
            }
        }
#endif

        auto view = adoptNS([[WKUIRemoteView alloc] initWithFrame:CGRectZero pid:protect(m_drawingArea)->page()->legacyMainFrameProcessID() contextID:properties.hostingContextID()]);
        return makeWithView(WTF::move(view));
    }
    case PlatformCALayer::LayerType::LayerTypeShapeLayer:
        return makeWithView(adoptNS([[WKShapeView alloc] init]));

    case PlatformCALayer::LayerType::LayerTypeScrollContainerLayer:
        if (!m_isDebugLayerTreeHost)
            return makeWithView(adoptNS([[WKChildScrollView alloc] init]));
        // The debug indicator parents views under layers, which can cause crashes with UIScrollView.
        return makeWithView(adoptNS([[UIView alloc] init]));

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    case PlatformCALayer::LayerType::LayerTypeSeparatedImageLayer:
        return makeWithView(adoptNS([[WKSeparatedImageView alloc] init]));
#endif

#if ENABLE(MODEL_ELEMENT)
    case PlatformCALayer::LayerType::LayerTypeModelLayer:
#if ENABLE(MODEL_PROCESS)
        bool modelHandledOutOfProcess = m_drawingArea->page() && m_drawingArea->page()->preferences().modelProcessEnabled();
#else
        bool modelHandledOutOfProcess = false;
#endif

        if (!modelHandledOutOfProcess) {
            if (RefPtr page = m_drawingArea->page()) {
                if (page->preferences().modelElementEnabled()) {
                    if (auto* model = std::get_if<Ref<Model>>(&properties.additionalData)) {
#if ENABLE(SEPARATED_MODEL)
                return makeWithView(adoptNS([[WKSeparatedModelView alloc] initWithModel:*model]));
#elif ENABLE(ARKIT_INLINE_PREVIEW_IOS)
                return makeWithView(adoptNS([[WKModelView alloc] initWithModel:*model layerID:*properties.layerID page:*m_drawingArea->page()]));
#else
                UNUSED_PARAM(model);
#endif
                    }
                }
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
