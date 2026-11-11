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
#include "DocumentSecurityOrigin.h"
#include "DocumentView.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameInlines.h"
#include "FrameLoader.h"
#include "HTMLFrameOwnerElement.h"
#include "LocalDOMWindow.h"
#include "LocalFrameInlines.h"
#include "Performance.h"
#include "ResourceTiming.h"
#include "SecurityOrigin.h"

namespace WebCore {

bool ResourceTimingInformation::shouldAddResourceTiming(CachedResource& resource)
{
    return resource.resourceRequest().url().protocolIsInHTTPFamily()
        && !resource.loadFailedOrCanceled()
        && resource.options().loadedFromOpaqueSource == LoadedFromOpaqueSource::No;
}

void ResourceTimingInformation::addResourceTimingToDocument(Document& document, ResourceTiming&& resourceTiming)
{
    RefPtr window = document.window();
    if (!window)
        return;

    resourceTiming.updateExposure(protect(document.securityOrigin()));
    protect(window->performance())->addResourceTiming(WTF::move(resourceTiming));
}

void ResourceTimingInformation::addResourceTiming(CachedResource& resource, Document& document, ResourceTiming&& resourceTiming)
{
    if (!ResourceTimingInformation::shouldAddResourceTiming(resource))
        return;

    auto iterator = m_initiatorMap.find(resource);
    if (iterator == m_initiatorMap.end())
        return;

    InitiatorInfo& info = iterator->value;
    if (info.added == Added)
        return;

    RefPtr initiatorDocument = &document;
    if (resource.type() == CachedResource::Type::MainResource && document.frame() && protect(document.frame()->loader())->shouldReportResourceTimingToParentFrame()) {
        RefPtr parentFrame = dynamicDowncast<LocalFrame>(protect(document.frame()->loader())->parentFrameForResourceTiming());
        initiatorDocument = parentFrame ? parentFrame->document() : nullptr;
        if (initiatorDocument)
            resourceTiming.updateExposure(protect(initiatorDocument->securityOrigin()));
    }
    if (!initiatorDocument)
        return;

    RefPtr initiatorWindow = initiatorDocument->window();
    if (!initiatorWindow)
        return;

    resourceTiming.overrideInitiatorType(info.type);

    protect(initiatorWindow->performance())->addResourceTiming(WTF::move(resourceTiming));

    info.added = Added;
}

void ResourceTimingInformation::removeResourceTiming(CachedResource& resource)
{
    m_initiatorMap.remove(resource);
}

void ResourceTimingInformation::storeResourceTimingInitiatorInformation(CachedResource& resource, const AtomString& initiatorType, LocalFrame* frame)
{
    if (resource.type() == CachedResource::Type::MainResource) {
        ASSERT(frame);
        // A provisional frame is not attached to its owner element yet, so use the one of the frame it will replace.
        RefPtr ownerElement = frame->ownerElement();
        if (!ownerElement) {
            if (RefPtr frameBeingReplaced = frame->frameBeingReplaced())
                ownerElement = frameBeingReplaced->ownerElement();
        }
        if (ownerElement) {
            InitiatorInfo info = { ownerElement->localName(), NotYetAdded };
            m_initiatorMap.add(resource, info);
        }
    } else {
        InitiatorInfo info = { initiatorType, NotYetAdded };
        m_initiatorMap.add(resource, info);
    }
}

}
