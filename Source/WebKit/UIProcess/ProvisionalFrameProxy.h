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
class ProvisionalFrameProxy;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebKit::ProvisionalFrameProxy> : std::true_type { };
}

namespace WebKit {

class FrameProcess;
class VisitedLinkStore;
class WebFrameProxy;
class WebProcessProxy;

class ProvisionalFrameProxy : public CanMakeWeakPtr<ProvisionalFrameProxy> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ProvisionalFrameProxy(WebFrameProxy&, Ref<FrameProcess>&&, bool isCrossSiteRedirect);
    ~ProvisionalFrameProxy();

    WebProcessProxy& process() const;
    Ref<WebProcessProxy> protectedProcess() const;

    WebCore::LayerHostingContextIdentifier layerHostingContextIdentifier() const { return m_layerHostingContextIdentifier; }

    Ref<FrameProcess> takeFrameProcess();

    bool isCrossSiteRedirect() const { return m_isCrossSiteRedirect; }

private:
    WeakRef<WebFrameProxy> m_frame;
    Ref<FrameProcess> m_frameProcess;
    Ref<VisitedLinkStore> m_visitedLinkStore;
    bool m_isCrossSiteRedirect;
    WebCore::LayerHostingContextIdentifier m_layerHostingContextIdentifier;
};

} // namespace WebKit
