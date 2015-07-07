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

#ifndef WebFrameNetworkingContext_h
#define WebFrameNetworkingContext_h

#include <WebCore/FrameNetworkingContext.h>

class WebFrameNetworkingContext : public WebCore::FrameNetworkingContext {
public:
    static PassRefPtr<WebFrameNetworkingContext> create(WebCore::Frame* frame)
    {
        return adoptRef(new WebFrameNetworkingContext(frame));
    }

    static void ensurePrivateBrowsingSession();
    static void destroyPrivateBrowsingSession();

#if PLATFORM(IOS)
    // FIXME: If MobileSafari ever switches to per-tab or non-shared private storage, then this can be removed.
    // <rdar://problem/10075665> Sub-TLF: Per-tab private browsing
    static void clearPrivateBrowsingSessionCookieStorage();
#endif

private:

    WebFrameNetworkingContext(WebCore::Frame* frame)
        : WebCore::FrameNetworkingContext(frame)
    {
    }

    virtual bool needsSiteSpecificQuirks() const override;
    virtual bool localFileContentSniffingEnabled() const override;
    virtual SchedulePairHashSet* scheduledRunLoopPairs() const override;
    virtual RetainPtr<CFDataRef> sourceApplicationAuditData() const override;
    virtual String sourceApplicationIdentifier() const override;
    virtual WebCore::ResourceError blockedError(const WebCore::ResourceRequest&) const override;
    virtual WebCore::NetworkStorageSession& storageSession() const override;

};

#endif
