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

#pragma once

#include "LayerHostingContext.h"
#include <WebCore/FloatSize.h>
#include <WebCore/PlatformLayer.h>
#include <wtf/CompletionHandler.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebKit {

class LayerHostingContext;

class LayerHostingContextManager {
    WTF_MAKE_TZONE_ALLOCATED(LayerHostingContextManager);
    WTF_MAKE_NONCOPYABLE(LayerHostingContextManager);
public:
    LayerHostingContextManager();
    LayerHostingContextManager(LayerHostingContextManager&&);
    LayerHostingContextManager& operator=(LayerHostingContextManager&&);
    ~LayerHostingContextManager();

    using LayerHostingContextCallback = CompletionHandler<void(WebCore::HostingContext)>;

    void requestHostingContext(LayerHostingContextCallback&&);
    std::optional<WebCore::HostingContext> createHostingContextIfNeeded(const PlatformLayerContainer&, bool canShowWhileLocked);
    void setVideoLayerSize(const WebCore::FloatSize&);
    void setVideoLayerSizeFenced(const WebCore::FloatSize&, WTF::MachSendRightAnnotated&&, NOESCAPE CompletionHandler<void()>&& postCommitAction);
    WebCore::FloatSize videoLayerSize() const { return m_videoLayerSize; }
    void setVideoLayerSizeIfPossible();
    void NODELETE setInitialVideoLayerSize(const WebCore::FloatSize&);

private:
    Vector<LayerHostingContextCallback> m_layerHostingContextRequests;
    std::unique_ptr<LayerHostingContext> m_inlineLayerHostingContext;
    WebCore::FloatSize m_videoLayerSize;
};

} // namespace WebKit
