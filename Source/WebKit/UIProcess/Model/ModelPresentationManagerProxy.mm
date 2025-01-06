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
    return modelPresentation.pageHostedModelView;
}

void ModelPresentationManagerProxy::invalidateModel(const WebCore::PlatformLayerIdentifier& layerIdentifier)
{
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
