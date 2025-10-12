/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "ModelPresentationManagerProxy.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(MODEL_PROCESS)

#import "UIKitSPI.h"
#import "WKPageHostedModelView.h"
#import "WKWebViewIOS.h"
#import "WebPageProxy.h"
#import <wtf/RefPtr.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ModelPresentationManagerProxy);

ModelPresentationManagerProxy::ModelPresentationManagerProxy(WebPageProxy& page)
    : m_page(page)
{
}

ModelPresentationManagerProxy::~ModelPresentationManagerProxy() = default;

RetainPtr<WKPageHostedModelView> ModelPresentationManagerProxy::setUpModelView(Ref<WebCore::ModelContext> modelContext)
{
    RefPtr webPageProxy = m_page.get();
    if (!webPageProxy)
        return nil;

    auto& modelPresentation = ensureModelPresentation(modelContext, *webPageProxy);
    auto view = modelPresentation.pageHostedModelView;
    CGRect frame = [view frame];
    frame.size.width = modelContext->modelLayoutSize().width().toFloat();
    frame.size.height = modelContext->modelLayoutSize().height().toFloat();
    [view setFrame:frame];
    [view setShouldDisablePortal:modelContext->disablePortal() == WebCore::ModelContextDisablePortal::Yes];
    [view applyBackgroundColor:modelContext->backgroundColor()];

    pageScaleDidChange(webPageProxy->pageScaleFactor());
    return view;
}

RetainPtr<UIView> ModelPresentationManagerProxy::startDragForModel(const WebCore::PlatformLayerIdentifier& layerIdentifier)
{
    auto iterator = m_modelPresentations.find(layerIdentifier);
    if (iterator == m_modelPresentations.end())
        return nil;

    auto& modelPresentation = iterator->value;
    RetainPtr modelView = modelPresentation->remoteModelView;
    if (!modelView)
        return nil;

#if PLATFORM(VISION)
    CGRect frame = [modelView frame];
    [modelView _setAssumedNoncoplanarHostedContentSize:SPSize3DMake(CGRectGetWidth(frame), CGRectGetHeight(frame), 100)];

    auto hostedView = modelPresentation->pageHostedModelView;
    [hostedView setPortalCrossing:YES];
#endif

    m_activelyDraggedModelLayerIDs.add(layerIdentifier);

    return modelView;
}

void ModelPresentationManagerProxy::doneWithCurrentDragSession()
{
    for (WebCore::PlatformLayerIdentifier layerIdentifier : m_activelyDraggedModelLayerIDs) {
        auto iterator = m_modelPresentations.find(layerIdentifier);
        if (iterator == m_modelPresentations.end())
            continue;

        auto& modelPresentation = iterator->value;
        if (auto pageHostedModelView = modelPresentation->pageHostedModelView)
            [modelPresentation->pageHostedModelView setPortalCrossing:NO];
    }

    m_activelyDraggedModelLayerIDs.clear();
}

void ModelPresentationManagerProxy::pageScaleDidChange(CGFloat newScale)
{
    for (auto& modelPresentation : m_modelPresentations.values()) {
        // This is safe because only the pageHostedView is part of the RemoteLayerTree
        if (RetainPtr modelView = modelPresentation->remoteModelView) {
            CATransform3D newTransform = [modelView transform3D];
            newTransform.m33 = newScale;
            modelView.get().transform3D = newTransform;
        }
    }
}

void ModelPresentationManagerProxy::invalidateModel(const WebCore::PlatformLayerIdentifier& layerIdentifier)
{
    auto iterator = m_modelPresentations.find(layerIdentifier);
    if (iterator == m_modelPresentations.end())
        return;

    auto& modelPresentation = iterator->value;

    // If the model being removed is currently being dragged, we have to make sure the _UIRemoteView
    // stays in some window by adding it to the WKContentView's _dragPreviewContainerView.
    if (RefPtr webPageProxy = m_page.get(); m_activelyDraggedModelLayerIDs.contains(layerIdentifier)) {
        RELEASE_LOG(ModelElement, "%p - ModelPresentationManagerProxy dragged model with layerID: %" PRIu64 " is being removed", this, layerIdentifier.object().toRawValue());
        if (RetainPtr pageHostedModelView = modelPresentation->pageHostedModelView)
            [webPageProxy->cocoaView() _willInvalidateDraggedModelWithContainerView:pageHostedModelView.get()];
    }

    m_modelPresentations.remove(layerIdentifier);
    RELEASE_LOG_INFO(ModelElement, "%p - ModelPresentationManagerProxy removed model presentation for layer ID: %" PRIu64, this, layerIdentifier.object().toRawValue());
}

void ModelPresentationManagerProxy::invalidateAllModels()
{
    m_modelPresentations.clear();
    RELEASE_LOG_INFO(ModelElement, "%p - ModelPresentationManagerProxy removed all model presentations", this);
}

ModelPresentationManagerProxy::ModelPresentation& ModelPresentationManagerProxy::ensureModelPresentation(Ref<WebCore::ModelContext> modelContext, const WebPageProxy& webPageProxy)
{
    auto layerIdentifier = modelContext->modelLayerIdentifier();
    if (m_modelPresentations.contains(layerIdentifier)) {
        // Update the existing ModelPresentation
        ModelPresentation& modelPresentation = *(m_modelPresentations.get(layerIdentifier));
        if (modelPresentation.modelContext->modelContentsLayerHostingContextIdentifier() != modelContext->modelContentsLayerHostingContextIdentifier()) {
            modelPresentation.remoteModelView = adoptNS([[_UIRemoteView alloc] initWithFrame:CGRectZero pid:webPageProxy.legacyMainFrameProcessID() contextID:modelContext->modelContentsLayerHostingContextIdentifier().toRawValue()]);
            [modelPresentation.pageHostedModelView setRemoteModelView:modelPresentation.remoteModelView.get()];
            RELEASE_LOG_INFO(ModelElement, "%p - ModelPresentationManagerProxy updated model view for element: %" PRIu64, this, layerIdentifier.object().toRawValue());
        }
        modelPresentation.modelContext = modelContext;
    } else {
        auto pageHostedModelView = adoptNS([[WKPageHostedModelView alloc] init]);
        auto remoteModelView = adoptNS([[_UIRemoteView alloc] initWithFrame:CGRectZero pid:webPageProxy.legacyMainFrameProcessID() contextID:modelContext->modelContentsLayerHostingContextIdentifier().toRawValue()]);
        [pageHostedModelView setRemoteModelView:remoteModelView.get()];
        auto modelPresentation = ModelPresentation {
            .modelContext = modelContext,
            .remoteModelView = remoteModelView,
            .pageHostedModelView = pageHostedModelView,
        };
        m_modelPresentations.add(layerIdentifier, makeUniqueRef<ModelPresentationManagerProxy::ModelPresentation>(WTFMove(modelPresentation)));
        RELEASE_LOG_INFO(ModelElement, "%p - ModelPresentationManagerProxy created new model presentation for element: %" PRIu64, this, layerIdentifier.object().toRawValue());
    }

    return *(m_modelPresentations.get(layerIdentifier));
}

}

#endif // PLATFORM(IOS_FAMILY) && ENABLE(MODEL_PROCESS)
