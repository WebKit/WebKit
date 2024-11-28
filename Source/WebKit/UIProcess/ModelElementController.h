/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(ARKIT_INLINE_PREVIEW)

#include "ModelIdentifier.h"
#include <WebCore/ElementContext.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/HTMLModelElementCamera.h>
#include <WebCore/ResourceError.h>
#include <wtf/MachSendRight.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/URL.h>

OBJC_CLASS ASVInlinePreview;

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)
OBJC_CLASS WKModelView;
#endif

namespace WebKit {

class WebPageProxy;

class ModelElementController : public RefCountedAndCanMakeWeakPtr<ModelElementController> {
    WTF_MAKE_TZONE_ALLOCATED(ModelElementController);
public:
    static Ref<ModelElementController> create(WebPageProxy&);

    WebPageProxy* page();

#if ENABLE(ARKIT_INLINE_PREVIEW)
    void getCameraForModelElement(ModelIdentifier, CompletionHandler<void(Expected<WebCore::HTMLModelElementCamera, WebCore::ResourceError>)>&&);
    void setCameraForModelElement(ModelIdentifier, WebCore::HTMLModelElementCamera, CompletionHandler<void(bool)>&&);
    void isPlayingAnimationForModelElement(ModelIdentifier, CompletionHandler<void(Expected<bool, WebCore::ResourceError>)>&&);
    void setAnimationIsPlayingForModelElement(ModelIdentifier, bool, CompletionHandler<void(bool)>&&);
    void isLoopingAnimationForModelElement(ModelIdentifier, CompletionHandler<void(Expected<bool, WebCore::ResourceError>)>&&);
    void setIsLoopingAnimationForModelElement(ModelIdentifier, bool, CompletionHandler<void(bool)>&&);
    void animationDurationForModelElement(ModelIdentifier, CompletionHandler<void(Expected<Seconds, WebCore::ResourceError>)>&&);
    void animationCurrentTimeForModelElement(ModelIdentifier, CompletionHandler<void(Expected<Seconds, WebCore::ResourceError>)>&&);
    void setAnimationCurrentTimeForModelElement(ModelIdentifier, Seconds, CompletionHandler<void(bool)>&&);
    void hasAudioForModelElement(ModelIdentifier, CompletionHandler<void(Expected<bool, WebCore::ResourceError>)>&&);
    void isMutedForModelElement(ModelIdentifier, CompletionHandler<void(Expected<bool, WebCore::ResourceError>)>&&);
    void setIsMutedForModelElement(ModelIdentifier, bool, CompletionHandler<void(bool)>&&);
#endif
#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)
    void takeModelElementFullscreen(ModelIdentifier, const URL&);
    void setInteractionEnabledForModelElement(ModelIdentifier, bool);
#endif
#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
    void modelElementCreateRemotePreview(String, WebCore::FloatSize, CompletionHandler<void(Expected<std::pair<String, uint32_t>, WebCore::ResourceError>)>&&);
    void modelElementLoadRemotePreview(String, URL, CompletionHandler<void(std::optional<WebCore::ResourceError>&&)>&&);
    void modelElementDestroyRemotePreview(String);
    void modelElementSizeDidChange(const String& uuid, WebCore::FloatSize, CompletionHandler<void(Expected<MachSendRight, WebCore::ResourceError>)>&&);
    void handleMouseDownForModelElement(const String&, const WebCore::LayoutPoint&, MonotonicTime);
    void handleMouseMoveForModelElement(const String&, const WebCore::LayoutPoint&, MonotonicTime);
    void handleMouseUpForModelElement(const String&, const WebCore::LayoutPoint&, MonotonicTime);
    void inlinePreviewUUIDs(CompletionHandler<void(Vector<String>&&)>&&);
#endif

private:
    explicit ModelElementController(WebPageProxy&);

#if ENABLE(ARKIT_INLINE_PREVIEW)
    ASVInlinePreview * previewForModelIdentifier(ModelIdentifier);
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)
    WKModelView * modelViewForModelIdentifier(ModelIdentifier);
#endif

    WeakPtr<WebPageProxy> m_webPageProxy;
#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
    RetainPtr<ASVInlinePreview> previewForUUID(const String&);
    HashMap<String, RetainPtr<ASVInlinePreview>> m_inlinePreviews;
#endif
};

} // namespace WebKit

#endif
