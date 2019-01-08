/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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

#include "WebFrame.h"
#include <WebCore/FrameNetworkingContext.h>
#include <pal/SessionID.h>

namespace WebKit {

struct WebsiteDataStoreParameters;

class WebFrameNetworkingContext : public WebCore::FrameNetworkingContext {
public:
    static Ref<WebFrameNetworkingContext> create(WebFrame* frame)
    {
        return adoptRef(*new WebFrameNetworkingContext(frame));
    }

    // FIXME: remove platform-specific code and use SessionTracker
    static void ensureWebsiteDataStoreSession(WebsiteDataStoreParameters&&);

    WebFrameLoaderClient* webFrameLoaderClient() const;

private:
    WebFrameNetworkingContext(WebFrame* frame)
        : WebCore::FrameNetworkingContext(frame->coreFrame())
    {
    }

    bool localFileContentSniffingEnabled() const override;
    SchedulePairHashSet* scheduledRunLoopPairs() const override;
    RetainPtr<CFDataRef> sourceApplicationAuditData() const override;
    String sourceApplicationIdentifier() const override;
    WebCore::ResourceError blockedError(const WebCore::ResourceRequest&) const override;
    WebCore::NetworkStorageSession* storageSession() const override { return nullptr; }
};

}
