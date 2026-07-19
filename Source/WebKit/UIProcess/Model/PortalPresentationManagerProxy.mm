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
#import "PortalPresentationManagerProxy.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(MODEL_PROCESS)

#if HAVE(CORE_RE)

#import "UIKitSPI.h"
#import "WKPageHostedPortalView.h"
#import "WKWebViewIOS.h"
#import "WebPageProxy.h"
#import <wtf/RefPtr.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PortalPresentationManagerProxy);

PortalPresentationManagerProxy::PortalPresentationManagerProxy(WebPageProxy& page)
    : m_page(page)
{
}

PortalPresentationManagerProxy::~PortalPresentationManagerProxy() = default;

RetainPtr<WKPageHostedPortalView> PortalPresentationManagerProxy::setUpModelView(Ref<WebCore::ModelContext> modelContext)
{
    RefPtr webPageProxy = m_page.get();
    if (!webPageProxy)
        return nil;

    auto& modelPresentation = ensurePortalPresentation(modelContext, *webPageProxy);
    auto view = modelPresentation.pageHostedPortalView;
    CGRect frame = [view frame];
    frame.size.width = modelContext->modelLayoutSize().width().toFloat();
    frame.size.height = modelContext->modelLayoutSize().height().toFloat();
    [view setFrame:frame];
    [view setShouldDisablePortal:modelContext->disablePortal() == WebCore::ModelContextDisablePortal::Yes];
    [view applyBackgroundColor:modelContext->backgroundColor()];

    pageScaleDidChange(webPageProxy->pageScaleFactor());
    return view;
}

RetainPtr<UIView> PortalPresentationManagerProxy::startDragForModel(const WebCore::PlatformLayerIdentifier& layerIdentifier)
{
    auto iterator = m_portalPresentations.find(layerIdentifier);
    if (iterator == m_portalPresentations.end())
        return nil;

    auto& modelPresentation = iterator->value;
    RetainPtr modelView = modelPresentation->remoteModelView;
    if (!modelView)
        return nil;

#if PLATFORM(VISION)
    CGRect frame = [modelView frame];
    [modelView _setAssumedNoncoplanarHostedContentSize:SPSize3DMake(CGRectGetWidth(frame), CGRectGetHeight(frame), 100)];

    auto hostedView = modelPresentation->pageHostedPortalView;
    [hostedView setPortalCrossing:YES];
#endif

    m_activelyDraggedModelLayerIDs.add(layerIdentifier);

    return modelView;
}

void PortalPresentationManagerProxy::doneWithCurrentDragSession()
{
    for (WebCore::PlatformLayerIdentifier layerIdentifier : m_activelyDraggedModelLayerIDs) {
        auto iterator = m_portalPresentations.find(layerIdentifier);
        if (iterator == m_portalPresentations.end())
            continue;

        auto& modelPresentation = iterator->value;
        if (auto pageHostedPortalView = modelPresentation->pageHostedPortalView)
            [modelPresentation->pageHostedPortalView setPortalCrossing:NO];
    }

    m_activelyDraggedModelLayerIDs.clear();
}

void PortalPresentationManagerProxy::pageScaleDidChange(CGFloat newScale)
{
    for (auto& modelPresentation : m_portalPresentations.values()) {
        // This is safe because only the pageHostedView is part of the RemoteLayerTree
        if (RetainPtr modelView = modelPresentation->remoteModelView) {
            CATransform3D newTransform = [modelView transform3D];
            newTransform.m33 = newScale;
            modelView.get().transform3D = newTransform;
        }
    }
}

void PortalPresentationManagerProxy::invalidateModel(const WebCore::PlatformLayerIdentifier& layerIdentifier)
{
    auto iterator = m_portalPresentations.find(layerIdentifier);
    if (iterator == m_portalPresentations.end())
        return;

    auto& modelPresentation = iterator->value;

    // If the model being removed is currently being dragged, we have to make sure the _UIRemoteView
    // stays in some window by adding it to the WKContentView's _dragPreviewContainerView.
    if (RefPtr webPageProxy = m_page.get(); m_activelyDraggedModelLayerIDs.contains(layerIdentifier)) {
        RELEASE_LOG(ModelElement, "%p - PortalPresentationManagerProxy dragged model with layerID: %" PRIu64 " is being removed", this, layerIdentifier.object().toRawValue());
        if (RetainPtr pageHostedPortalView = modelPresentation->pageHostedPortalView)
            [webPageProxy->cocoaView() _willInvalidateDraggedModelWithContainerView:pageHostedPortalView.get()];
    }

    m_portalPresentations.remove(layerIdentifier);
    RELEASE_LOG_INFO(ModelElement, "%p - PortalPresentationManagerProxy removed model presentation for layer ID: %" PRIu64, this, layerIdentifier.object().toRawValue());
}

void PortalPresentationManagerProxy::invalidateAllModels()
{
    m_portalPresentations.clear();
    RELEASE_LOG_INFO(ModelElement, "%p - PortalPresentationManagerProxy removed all model presentations", this);
}

PortalPresentationManagerProxy::PortalPresentation& PortalPresentationManagerProxy::ensurePortalPresentation(Ref<WebCore::ModelContext> modelContext, const WebPageProxy& webPageProxy)
{
    auto layerIdentifier = modelContext->modelLayerIdentifier();
    if (m_portalPresentations.contains(layerIdentifier)) {
        // Update the existing PortalPresentation
        PortalPresentation& modelPresentation = *(m_portalPresentations.get(layerIdentifier));
        if (modelPresentation.modelContext->modelContentsLayerHostingContextIdentifier() != modelContext->modelContentsLayerHostingContextIdentifier()) {
            modelPresentation.remoteModelView = adoptNS([[_UIRemoteView alloc] initWithFrame:CGRectZero pid:webPageProxy.legacyMainFrameProcessID() contextID:modelContext->modelContentsLayerHostingContextIdentifier().toRawValue()]);
            [modelPresentation.pageHostedPortalView setRemoteModelView:modelPresentation.remoteModelView.get()];
            RELEASE_LOG_INFO(ModelElement, "%p - PortalPresentationManagerProxy updated model view for element: %" PRIu64, this, layerIdentifier.object().toRawValue());
        }
        modelPresentation.modelContext = modelContext;
    } else {
        RetainPtr pageHostedPortalView = adoptNS([[WKPageHostedPortalView alloc] init]);
        RetainPtr remoteModelView = adoptNS([[_UIRemoteView alloc] initWithFrame:CGRectZero pid:webPageProxy.legacyMainFrameProcessID() contextID:modelContext->modelContentsLayerHostingContextIdentifier().toRawValue()]);
        [pageHostedPortalView setRemoteModelView:remoteModelView.get()];
        auto modelPresentation = PortalPresentation {
            .modelContext = modelContext,
            .remoteModelView = remoteModelView,
            .pageHostedPortalView = pageHostedPortalView,
        };
        m_portalPresentations.add(layerIdentifier, makeUniqueRef<PortalPresentationManagerProxy::PortalPresentation>(WTF::move(modelPresentation)));
        RELEASE_LOG_INFO(ModelElement, "%p - PortalPresentationManagerProxy created new model presentation for element: %" PRIu64, this, layerIdentifier.object().toRawValue());
    }

    return *(m_portalPresentations.get(layerIdentifier));
}

}

#else // !HAVE(CORE_RE)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PortalPresentationManagerProxy);

PortalPresentationManagerProxy::PortalPresentationManagerProxy(WebPageProxy& page)
    : m_page(page)
{
}

PortalPresentationManagerProxy::~PortalPresentationManagerProxy() = default;

RetainPtr<WKPageHostedPortalView> PortalPresentationManagerProxy::setUpModelView(Ref<WebCore::ModelContext>)
{
    return nil;
}

RetainPtr<UIView> PortalPresentationManagerProxy::startDragForModel(const WebCore::PlatformLayerIdentifier&)
{
    return nil;
}

void PortalPresentationManagerProxy::doneWithCurrentDragSession()
{
}

void PortalPresentationManagerProxy::pageScaleDidChange(CGFloat)
{
}

void PortalPresentationManagerProxy::invalidateModel(const WebCore::PlatformLayerIdentifier&)
{
}

void PortalPresentationManagerProxy::invalidateAllModels()
{
}

}

#endif // HAVE(CORE_RE)

#endif // PLATFORM(IOS_FAMILY) && ENABLE(MODEL_PROCESS)
