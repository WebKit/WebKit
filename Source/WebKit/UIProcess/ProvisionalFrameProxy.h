/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "WebPageProxyIdentifier.h"
#include <WebCore/LayerHostingContextIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class RemotePageProxy;
class VisitedLinkStore;
class WebFrameProxy;
class WebProcessProxy;

class ProvisionalFrameProxy : public CanMakeWeakPtr<ProvisionalFrameProxy> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ProvisionalFrameProxy(WebFrameProxy&, WebProcessProxy&, RefPtr<RemotePageProxy>&&);
    ~ProvisionalFrameProxy();

    WebProcessProxy& process() const { return m_process.get(); }
    Ref<WebProcessProxy> protectedProcess() const;
    RefPtr<RemotePageProxy> takeRemotePageProxy();

    WebCore::LayerHostingContextIdentifier layerHostingContextIdentifier() const { return m_layerHostingContextIdentifier; }

private:
    WeakRef<WebFrameProxy> m_frame;
    Ref<WebProcessProxy> m_process;
    RefPtr<RemotePageProxy> m_remotePageProxy;
    Ref<VisitedLinkStore> m_visitedLinkStore;
    WebCore::PageIdentifier m_pageID;
    WebPageProxyIdentifier m_webPageID;
    WebCore::LayerHostingContextIdentifier m_layerHostingContextIdentifier;
};

} // namespace WebKit
