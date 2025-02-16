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

OBJC_CLASS WKPageHostedModelView;
OBJC_CLASS UIView;
OBJC_CLASS _UIRemoteView;

namespace WebKit {

class WebPageProxy;

class ModelPresentationManagerProxy : public RefCounted<ModelPresentationManagerProxy> {
    WTF_MAKE_TZONE_ALLOCATED(ModelPresentationManagerProxy);
public:
    static Ref<ModelPresentationManagerProxy> create(WebPageProxy& page)
    {
        return adoptRef(*new ModelPresentationManagerProxy(page));
    }

    virtual ~ModelPresentationManagerProxy();

    RetainPtr<WKPageHostedModelView> setUpModelView(Ref<WebCore::ModelContext>);
    RetainPtr<UIView> startDragForModel(const WebCore::PlatformLayerIdentifier&);
    void doneWithCurrentDragSession();
    void invalidateModel(const WebCore::PlatformLayerIdentifier&);
    void invalidateAllModels();

private:
    explicit ModelPresentationManagerProxy(WebPageProxy&);

    struct ModelPresentation {
        WTF_MAKE_FAST_ALLOCATED;

    public:
        Ref<WebCore::ModelContext> modelContext;
        RetainPtr<_UIRemoteView> remoteModelView;
        RetainPtr<WKPageHostedModelView> pageHostedModelView;
    };

    ModelPresentation& ensureModelPresentation(Ref<WebCore::ModelContext>, const WebPageProxy&);

    HashMap<WebCore::PlatformLayerIdentifier, UniqueRef<ModelPresentation>> m_modelPresentations;
    HashSet<WebCore::PlatformLayerIdentifier> m_activelyDraggedModelLayerIDs;
    WeakPtr<WebPageProxy> m_page;
};

}

#endif // PLATFORM(IOS_FAMILY) && ENABLE(MODEL_PROCESS)
