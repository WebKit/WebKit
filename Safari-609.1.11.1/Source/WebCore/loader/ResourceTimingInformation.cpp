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
#include "ResourceTimingInformation.h"

#include "CachedResource.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLFrameOwnerElement.h"
#include "LoadTiming.h"
#include "Performance.h"
#include "ResourceTiming.h"
#include "RuntimeEnabledFeatures.h"

namespace WebCore {

bool ResourceTimingInformation::shouldAddResourceTiming(CachedResource& resource)
{
    // FIXME: We can be less restrictive here.
    // <https://github.com/w3c/resource-timing/issues/100>
    if (!resource.resourceRequest().url().protocolIsInHTTPFamily())
        return false;
    if (resource.errorOccurred())
        return false;
    if (resource.wasCanceled())
        return false;

    if (resource.options().loadedFromOpaqueSource == LoadedFromOpaqueSource::Yes)
        return false;

    return true;
}

void ResourceTimingInformation::addResourceTiming(CachedResource& resource, Document& document, ResourceTiming&& resourceTiming)
{
    ASSERT(RuntimeEnabledFeatures::sharedFeatures().resourceTimingEnabled());
    if (!ResourceTimingInformation::shouldAddResourceTiming(resource))
        return;

    auto iterator = m_initiatorMap.find(&resource);
    if (iterator == m_initiatorMap.end())
        return;

    InitiatorInfo& info = iterator->value;
    if (info.added == Added)
        return;

    Document* initiatorDocument = &document;
    if (resource.type() == CachedResource::Type::MainResource && document.frame() && document.frame()->loader().shouldReportResourceTimingToParentFrame())
        initiatorDocument = document.parentDocument();
    if (!initiatorDocument)
        return;

    auto* initiatorWindow = initiatorDocument->domWindow();
    if (!initiatorWindow)
        return;

    resourceTiming.overrideInitiatorName(info.name);

    initiatorWindow->performance().addResourceTiming(WTFMove(resourceTiming));

    info.added = Added;
}

void ResourceTimingInformation::storeResourceTimingInitiatorInformation(const CachedResourceHandle<CachedResource>& resource, const AtomString& initiatorName, Frame* frame)
{
    ASSERT(RuntimeEnabledFeatures::sharedFeatures().resourceTimingEnabled());
    ASSERT(resource.get());

    if (resource->type() == CachedResource::Type::MainResource) {
        // <iframe>s should report the initial navigation requested by the parent document, but not subsequent navigations.
        ASSERT(frame);
        if (frame->ownerElement()) {
            InitiatorInfo info = { frame->ownerElement()->localName(), NotYetAdded };
            m_initiatorMap.add(resource.get(), info);
        }
    } else {
        InitiatorInfo info = { initiatorName, NotYetAdded };
        m_initiatorMap.add(resource.get(), info);
    }
}

}
