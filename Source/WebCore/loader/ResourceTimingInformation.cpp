/*
 * Copyright (C) 2016 Akamai Technologies Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#if ENABLE(WEB_TIMING)
#include "ResourceTimingInformation.h"

#include "CachedResource.h"
#include "CachedResourceRequest.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Frame.h"
#include "HTMLFrameOwnerElement.h"
#include "LoadTiming.h"
#include "Performance.h"
#include "RuntimeEnabledFeatures.h"

namespace WebCore {

void ResourceTimingInformation::addResourceTiming(CachedResource* resource, Document& document, const LoadTiming& loadTiming)
{
    ASSERT(RuntimeEnabledFeatures::sharedFeatures().resourceTimingEnabled());
    if (resource && resource->resourceRequest().url().protocolIsInHTTPFamily()
        && ((!resource->errorOccurred() && !resource->wasCanceled()) || resource->response().httpStatusCode() == 304)) {
        auto initiatorIt = m_initiatorMap.find(resource);
        if (initiatorIt != m_initiatorMap.end() && initiatorIt->value.added == NotYetAdded) {
            Document* initiatorDocument = &document;
            if (resource->type() == CachedResource::MainResource)
                initiatorDocument = document.parentDocument();
            ASSERT(initiatorDocument);
            ASSERT(initiatorDocument->domWindow());
            ASSERT(initiatorDocument->domWindow()->performance());
            const InitiatorInfo& info = initiatorIt->value;
            initiatorDocument->domWindow()->performance()->addResourceTiming(info.name, initiatorDocument, resource->resourceRequest().url(), resource->response(), loadTiming);
            initiatorIt->value.added = Added;
        }
    }
}

void ResourceTimingInformation::storeResourceTimingInitiatorInformation(const CachedResourceHandle<CachedResource>& resource, const CachedResourceRequest& request, Frame* frame)
{
    ASSERT(RuntimeEnabledFeatures::sharedFeatures().resourceTimingEnabled());
    ASSERT(resource.get());
    if (resource->type() == CachedResource::MainResource) {
        // <iframe>s should report the initial navigation requested by the parent document, but not subsequent navigations.
        ASSERT(frame);
        if (frame->ownerElement()) {
            InitiatorInfo info = { frame->ownerElement()->localName(), monotonicallyIncreasingTime(), NotYetAdded };
            m_initiatorMap.add(resource.get(), info);
        }
    } else {
        InitiatorInfo info = { request.initiatorName(), monotonicallyIncreasingTime(), NotYetAdded };
        m_initiatorMap.add(resource.get(), info);
    }
}

}

#endif // ENABLE(WEB_TIMING)
