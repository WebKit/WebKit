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

#import "config.h"
#import "ARKitInlinePreviewModelPlayerIOS.h"

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)

#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import <WebCore/GraphicsLayer.h>

namespace WebKit {

Ref<ARKitInlinePreviewModelPlayerIOS> ARKitInlinePreviewModelPlayerIOS::create(WebPage& page, WebCore::ModelPlayerClient& client)
{
    return adoptRef(*new ARKitInlinePreviewModelPlayerIOS(page, client));
}

static HashSet<ARKitInlinePreviewModelPlayerIOS*>& instances()
{
    static NeverDestroyed<HashSet<ARKitInlinePreviewModelPlayerIOS*>> instances;
    return instances;
}

ARKitInlinePreviewModelPlayerIOS::ARKitInlinePreviewModelPlayerIOS(WebPage& page, WebCore::ModelPlayerClient& client)
    : ARKitInlinePreviewModelPlayer(page, client)
{
    instances().add(this);
}

ARKitInlinePreviewModelPlayerIOS::~ARKitInlinePreviewModelPlayerIOS()
{
    instances().remove(this);
}

ARKitInlinePreviewModelPlayerIOS* ARKitInlinePreviewModelPlayerIOS::modelPlayerForPageAndLayerID(WebPage& page, GraphicsLayer::PlatformLayerID layerID)
{
    for (auto* modelPlayer : instances()) {
        if (!modelPlayer || !modelPlayer->client())
            continue;

        if (&page != modelPlayer->page())
            continue;

        if (modelPlayer->client()->platformLayerID() != layerID)
            continue;

        return modelPlayer;
    }

    return nullptr;
}

void ARKitInlinePreviewModelPlayerIOS::pageLoadedModelInlinePreview(WebPage& page, GraphicsLayer::PlatformLayerID layerID)
{
    if (auto* modelPlayer = modelPlayerForPageAndLayerID(page, layerID))
        modelPlayer->client()->didFinishLoading(*modelPlayer);
}

void ARKitInlinePreviewModelPlayerIOS::pageFailedToLoadModelInlinePreview(WebPage& page, WebCore::GraphicsLayer::PlatformLayerID layerID, const WebCore::ResourceError& error)
{
    if (auto* modelPlayer = modelPlayerForPageAndLayerID(page, layerID))
        modelPlayer->client()->didFailLoading(*modelPlayer, error);
}

std::optional<ModelIdentifier> ARKitInlinePreviewModelPlayerIOS::modelIdentifier()
{
    if (!client())
        return { };

    if (auto layerId = client()->platformLayerID())
        return { { layerId } };

    return { };
}

// MARK: - WebCore::ModelPlayer overrides.

void ARKitInlinePreviewModelPlayerIOS::enterFullscreen()
{
    RefPtr strongPage = page();
    if (!strongPage)
        return;

    if (auto modelIdentifier = this->modelIdentifier())
        strongPage->send(Messages::WebPageProxy::TakeModelElementFullscreen(*modelIdentifier));
}

void ARKitInlinePreviewModelPlayerIOS::setInteractionEnabled(bool isInteractionEnabled)
{
    RefPtr strongPage = page();
    if (!strongPage)
        return;

    if (auto modelIdentifier = this->modelIdentifier())
        strongPage->send(Messages::WebPageProxy::ModelElementSetInteractionEnabled(*modelIdentifier, isInteractionEnabled));
}

void ARKitInlinePreviewModelPlayerIOS::handleMouseDown(const WebCore::LayoutPoint&, MonotonicTime)
{
    ASSERT_NOT_REACHED();
}

void ARKitInlinePreviewModelPlayerIOS::handleMouseMove(const WebCore::LayoutPoint&, MonotonicTime)
{
    ASSERT_NOT_REACHED();
}

void ARKitInlinePreviewModelPlayerIOS::handleMouseUp(const WebCore::LayoutPoint&, MonotonicTime)
{
    ASSERT_NOT_REACHED();
}

}
#endif
