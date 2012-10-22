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

#ifndef ResourceLoadClientEfl_h
#define ResourceLoadClientEfl_h

#include "ewk_resource_private.h"
#include "ewk_view_private.h"
#include <wtf/HashMap.h>
#include <wtf/PassOwnPtr.h>

namespace WebKit {

class ResourceLoadClientEfl {
public:
    ~ResourceLoadClientEfl();

    static PassOwnPtr<ResourceLoadClientEfl> create(Evas_Object* view)
    {
        return adoptPtr(new ResourceLoadClientEfl(view));
    }

private:
    explicit ResourceLoadClientEfl(Evas_Object* view);

    static void didInitiateLoadForResource(WKPageRef, WKFrameRef, uint64_t resourceIdentifier, WKURLRequestRef, bool pageIsProvisionallyLoading, const void* clientInfo);
    static void didSendRequestForResource(WKPageRef, WKFrameRef, uint64_t resourceIdentifier, WKURLRequestRef, WKURLResponseRef, const void* clientInfo);
    static void didReceiveResponseForResource(WKPageRef, WKFrameRef, uint64_t resourceIdentifier, WKURLResponseRef, const void* clientInfo);
    static void didFinishLoadForResource(WKPageRef, WKFrameRef, uint64_t resourceIdentifier, const void* clientInfo);
    static void didFailLoadForResource(WKPageRef, WKFrameRef, uint64_t resourceIdentifier, WKErrorRef, const void* clientInfo);

    static void onViewProvisionalLoadStarted(void* userData, Evas_Object* view, void* clientInfo);

    Evas_Object* m_view;
    HashMap< uint64_t, RefPtr<Ewk_Resource> > m_loadingResourcesMap;
};

} // namespace WebKit

#endif // ResourceLoadClientEfl_h
