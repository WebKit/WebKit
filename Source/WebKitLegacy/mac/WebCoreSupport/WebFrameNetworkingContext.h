/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include <WebCore/FrameNetworkingContext.h>

class WebFrameNetworkingContext : public WebCore::FrameNetworkingContext {
public:
    static Ref<WebFrameNetworkingContext> create(WebCore::Frame* frame)
    {
        return adoptRef(*new WebFrameNetworkingContext(frame));
    }

    static WebCore::NetworkStorageSession& ensurePrivateBrowsingSession();
    static void destroyPrivateBrowsingSession();

private:

    WebFrameNetworkingContext(WebCore::Frame* frame)
        : WebCore::FrameNetworkingContext(frame)
    {
    }

    bool localFileContentSniffingEnabled() const override;
    SchedulePairHashSet* scheduledRunLoopPairs() const override;
    RetainPtr<CFDataRef> sourceApplicationAuditData() const override;
    String sourceApplicationIdentifier() const override;
    WebCore::ResourceError blockedError(const WebCore::ResourceRequest&) const override;
    WebCore::NetworkStorageSession& storageSession() const override;
};
