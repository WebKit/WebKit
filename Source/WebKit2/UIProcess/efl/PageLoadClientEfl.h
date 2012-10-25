/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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

#ifndef PageLoadClientEfl_h
#define PageLoadClientEfl_h

#include "WKPageLoadTypes.h"
#include <WebKit2/WKBase.h>
#include <wtf/PassOwnPtr.h>

class EwkViewImpl;

namespace WebKit {

class PageLoadClientEfl {
public:
    static PassOwnPtr<PageLoadClientEfl> create(EwkViewImpl* viewImpl)
    {
        return adoptPtr(new PageLoadClientEfl(viewImpl));
    }

private:
    explicit PageLoadClientEfl(EwkViewImpl*);

    inline EwkViewImpl* viewImpl() const { return m_viewImpl; }

    static void didReceiveTitleForFrame(WKPageRef, WKStringRef title, WKFrameRef, WKTypeRef, const void* clientInfo);
#if ENABLE(WEB_INTENTS)
    static void didReceiveIntentForFrame(WKPageRef, WKFrameRef, WKIntentDataRef, WKTypeRef, const void* clientInfo);
#endif
#if ENABLE(WEB_INTENTS_TAG)
    static void registerIntentServiceForFrame(WKPageRef, WKFrameRef, WKIntentServiceInfoRef, WKTypeRef, const void* clientInfo);
#endif
    static void didChangeProgress(WKPageRef, const void* clientInfo);
    static void didFinishLoadForFrame(WKPageRef, WKFrameRef, WKTypeRef userData, const void* clientInfo);
    static void didFailLoadWithErrorForFrame(WKPageRef, WKFrameRef, WKErrorRef, WKTypeRef userData, const void* clientInfo);
    static void didStartProvisionalLoadForFrame(WKPageRef, WKFrameRef, WKTypeRef userData, const void* clientInfo);
    static void didReceiveServerRedirectForProvisionalLoadForFrame(WKPageRef, WKFrameRef, WKTypeRef userData, const void* clientInfo);
    static void didFailProvisionalLoadWithErrorForFrame(WKPageRef, WKFrameRef, WKErrorRef, WKTypeRef userData, const void* clientInfo);
#if USE(TILED_BACKING_STORE)
    static void didCommitLoadForFrame(WKPageRef, WKFrameRef, WKTypeRef userData, const void* clientInfo);
#endif
    static void didChangeBackForwardList(WKPageRef, WKBackForwardListItemRef addedItem, WKArrayRef removedItems, const void* clientInfo);
    static void didSameDocumentNavigationForFrame(WKPageRef, WKFrameRef, WKSameDocumentNavigationType, WKTypeRef, const void* clientInfo);

    EwkViewImpl* m_viewImpl;
};

} // namespace WebKit

#endif // PageLoadClientEfl_h
