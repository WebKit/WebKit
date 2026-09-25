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

#pragma once

#if PLATFORM(IOS_FAMILY) && ENABLE(MODEL_PROCESS)

#import <WebCore/ModelContext.h>
#import <WebCore/PlatformLayerIdentifier.h>
#import <wtf/RefCounted.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/UniqueRef.h>
#import <wtf/WeakPtr.h>

#if ENABLE(CONNECTED_VOLUMETRIC_SCENE)
#import "VolumetricSceneContentContext.h"
#import <WebCore/NodeIdentifier.h>
#import <wtf/CompletionHandler.h>
#endif

OBJC_CLASS WKPageHostedPortalView;
OBJC_CLASS WKPortalVolumetricSceneController;
OBJC_CLASS UIView;
OBJC_CLASS _UIRemoteView;

namespace WebKit {

class WebPageProxy;

class PortalPresentationManagerProxy : public RefCounted<PortalPresentationManagerProxy>, public CanMakeWeakPtr<PortalPresentationManagerProxy> {
    WTF_MAKE_TZONE_ALLOCATED(PortalPresentationManagerProxy);
public:
    static Ref<PortalPresentationManagerProxy> create(WebPageProxy& page)
    {
        return adoptRef(*new PortalPresentationManagerProxy(page));
    }

    virtual ~PortalPresentationManagerProxy();

    RetainPtr<WKPageHostedPortalView> setUpModelView(Ref<WebCore::ModelContext>);
    RetainPtr<UIView> startDragForModel(const WebCore::PlatformLayerIdentifier&);
    void doneWithCurrentDragSession();
    void invalidateModel(const WebCore::PlatformLayerIdentifier&);
    void invalidateAllModels();
    void pageScaleDidChange(CGFloat);

#if ENABLE(CONNECTED_VOLUMETRIC_SCENE)
    void showVolumetricScene(WebCore::NodeIdentifier, const VolumetricSceneContentContext&, CompletionHandler<void(bool)>&&);
    void reconnectVolumetricSceneToContentContext(WebCore::NodeIdentifier, const VolumetricSceneContentContext&);
    void hideVolumetricScene(WebCore::NodeIdentifier);
    void hideAllVolumetricScenes();
#endif

private:
    explicit PortalPresentationManagerProxy(WebPageProxy&);

    struct PortalPresentation {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(PortalPresentation);

    public:
        Ref<WebCore::ModelContext> modelContext;
        RetainPtr<_UIRemoteView> remoteModelView;
        RetainPtr<WKPageHostedPortalView> pageHostedPortalView;
    };

    PortalPresentation& ensurePortalPresentation(Ref<WebCore::ModelContext>, const WebPageProxy&);

    HashMap<WebCore::PlatformLayerIdentifier, UniqueRef<PortalPresentation>> m_portalPresentations;
    HashSet<WebCore::PlatformLayerIdentifier> m_activelyDraggedModelLayerIDs;
    WeakPtr<WebPageProxy> m_page;

#if ENABLE(CONNECTED_VOLUMETRIC_SCENE)
    struct VolumetricScenePresentation {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(VolumetricScenePresentation);

    public:
        WebCore::LayerHostingContextIdentifier contentContext;
        RetainPtr<WKPortalVolumetricSceneController> sceneController;
    };

    HashMap<WebCore::NodeIdentifier, UniqueRef<VolumetricScenePresentation>> m_volumetricScenes;
#endif
};

}

#endif // PLATFORM(IOS_FAMILY) && ENABLE(MODEL_PROCESS)
