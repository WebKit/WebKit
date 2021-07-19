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

#if HAVE(ARKIT_INLINE_PREVIEW)

#include <WebCore/ElementContext.h>
#include <WebCore/GraphicsLayer.h>
#include <wtf/RetainPtr.h>
#include <wtf/URL.h>
#include <wtf/WeakPtr.h>

#if HAVE(ARKIT_INLINE_PREVIEW_MAC)
OBJC_CLASS ASVInlinePreview;
#endif

namespace WebKit {

class WebPageProxy;

class ModelElementController : public CanMakeWeakPtr<ModelElementController> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit ModelElementController(WebPageProxy&);

    WebPageProxy& page() { return m_webPageProxy; }

#if HAVE(ARKIT_INLINE_PREVIEW_IOS)
    void takeModelElementFullscreen(WebCore::GraphicsLayer::PlatformLayerID contentLayerId);
#endif
#if HAVE(ARKIT_INLINE_PREVIEW_MAC)
    void modelElementDidCreatePreview(const WebCore::ElementContext&, const URL&, const String&, const WebCore::FloatSize&);
#endif

private:
    WebPageProxy& m_webPageProxy;
#if HAVE(ARKIT_INLINE_PREVIEW_MAC)
    HashMap<String, RetainPtr<ASVInlinePreview>> m_inlinePreviews;
#endif
};

}

#endif
