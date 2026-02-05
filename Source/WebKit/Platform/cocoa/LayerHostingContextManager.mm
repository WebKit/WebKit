/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "LayerHostingContextManager.h"

#include <wtf/MachSendRightAnnotated.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LayerHostingContextManager);

LayerHostingContextManager::LayerHostingContextManager() = default;
LayerHostingContextManager::LayerHostingContextManager(LayerHostingContextManager&&) = default;
LayerHostingContextManager& LayerHostingContextManager::operator=(LayerHostingContextManager&&) = default;

LayerHostingContextManager::~LayerHostingContextManager()
{
    for (auto& request : std::exchange(m_layerHostingContextRequests, { }))
        request({ });
}

void LayerHostingContextManager::requestHostingContext(LayerHostingContextCallback&& completionHandler)
{
    if (m_inlineLayerHostingContext) {
        completionHandler(m_inlineLayerHostingContext->hostingContext());
        return;
    }

    m_layerHostingContextRequests.append(WTF::move(completionHandler));
}

void LayerHostingContextManager::setInitialVideoLayerSize(const WebCore::FloatSize& size)
{
    if (!size.isEmpty() && m_videoLayerSize.isEmpty())
        m_videoLayerSize = size;
}

std::optional<WebCore::HostingContext> LayerHostingContextManager::createHostingContextIfNeeded(const PlatformLayerContainer& layer, bool canShowWhileLocked)
{
    bool hadLayer = false;
    if (layer && !m_inlineLayerHostingContext) {
        LayerHostingContextOptions contextOptions;
#if USE(EXTENSIONKIT)
        contextOptions.useHostable = true;
#endif
#if PLATFORM(IOS_FAMILY)
        contextOptions.canShowWhileLocked = canShowWhileLocked;
#else
        UNUSED_PARAM(canShowWhileLocked);
#endif
        m_inlineLayerHostingContext = LayerHostingContext::create(contextOptions);
        if (m_videoLayerSize.isEmpty())
            m_videoLayerSize = enclosingIntRect(WebCore::FloatRect([layer frame])).size();
        auto& size = m_videoLayerSize;
        [layer setFrame:CGRectMake(0, 0, size.width(), size.height())];
        for (auto& request : std::exchange(m_layerHostingContextRequests, { }))
            request(m_inlineLayerHostingContext->hostingContext());
        hadLayer = true;
    } else if (!layer && m_inlineLayerHostingContext) {
        m_inlineLayerHostingContext = nullptr;
        hadLayer = true;
    }

    if (m_inlineLayerHostingContext)
        m_inlineLayerHostingContext->setRootLayer(layer.get());

    if (!hadLayer)
        return std::nullopt;
    return m_inlineLayerHostingContext ? m_inlineLayerHostingContext->hostingContext() : WebCore::HostingContext { };
}

void LayerHostingContextManager::setVideoLayerSizeFenced(const WebCore::FloatSize& size, WTF::MachSendRightAnnotated&& sendRightAnnotated, NOESCAPE CompletionHandler<void()>&& postCommitAction)
{
#if USE(EXTENSIONKIT)
    RetainPtr<BELayerHierarchyHostingTransactionCoordinator> hostingUpdateCoordinator;
#endif

    if (m_inlineLayerHostingContext) {
#if USE(EXTENSIONKIT)
#if ENABLE(MACH_PORT_LAYER_HOSTING)
        auto sendRightAnnotatedCopy = sendRightAnnotated;
        hostingUpdateCoordinator = LayerHostingContext::createHostingUpdateCoordinator(WTF::move(sendRightAnnotatedCopy));
#else
        hostingUpdateCoordinator = LayerHostingContext::createHostingUpdateCoordinator(sendRightAnnotated.sendRight.sendRight());
#endif // ENABLE(MACH_PORT_LAYER_HOSTING)
        [hostingUpdateCoordinator addLayerHierarchy:m_inlineLayerHostingContext->hostable().get()];
#else
        m_inlineLayerHostingContext->setFencePort(sendRightAnnotated.sendRight.sendRight());
#endif // USE(EXTENSIONKIT)
    }

    m_videoLayerSize = size;
    setVideoLayerSizeIfPossible();

    postCommitAction();

#if USE(EXTENSIONKIT)
    [hostingUpdateCoordinator commit];
#endif
}

void LayerHostingContextManager::setVideoLayerSizeIfPossible()
{
    if (!m_inlineLayerHostingContext || !m_inlineLayerHostingContext->rootLayer() || m_videoLayerSize.isEmpty())
        return;

    // We do not want animations here.
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [protect(m_inlineLayerHostingContext->rootLayer()) setFrame:CGRectMake(0, 0, m_videoLayerSize.width(), m_videoLayerSize.height())];
    [CATransaction commit];
}

}
