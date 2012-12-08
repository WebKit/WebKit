/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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
 *
 */

#include "config.h"
#include "Prerenderer.h"

#if ENABLE(LINK_PRERENDER)

#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "PrerenderHandle.h"
#include "PrerendererClient.h"
#include "ReferrerPolicy.h"
#include "SecurityPolicy.h"
#include "WebCoreMemoryInstrumentation.h"

#include <wtf/MemoryInstrumentationVector.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// static
PassOwnPtr<Prerenderer> Prerenderer::create(Document* document)
{
    Prerenderer* prerenderer = new Prerenderer(document);
    prerenderer->suspendIfNeeded();
    return adoptPtr(prerenderer);
}

Prerenderer::Prerenderer(Document* document)
    : ActiveDOMObject(document, this)
    , m_initializedClient(false)
    , m_client(0)
{
}

Prerenderer::~Prerenderer()
{
}

PassRefPtr<PrerenderHandle> Prerenderer::render(PrerenderClient* prerenderClient, const KURL& url)
{
    // Prerenders are unlike requests in most ways (for instance, they pass down fragments, and they don't return data),
    // but they do have referrers.
    const ReferrerPolicy referrerPolicy = document()->referrerPolicy();
    
    if (!document()->frame())
        return 0;

    const String referrer = SecurityPolicy::generateReferrerHeader(referrerPolicy, url, document()->frame()->loader()->outgoingReferrer());

    RefPtr<PrerenderHandle> prerenderHandle = PrerenderHandle::create(prerenderClient, url, referrer, referrerPolicy);

    if (client())
        client()->willAddPrerender(prerenderHandle.get());
    prerenderHandle->add();

    m_activeHandles.append(prerenderHandle);
    return prerenderHandle;
}

void Prerenderer::stop()
{
    while (!m_activeHandles.isEmpty()) {
        RefPtr<PrerenderHandle> handle = m_activeHandles[0].release();
        m_activeHandles.remove(0);
        handle->abandon();
    }
    while (!m_suspendedHandles.isEmpty()) {
        RefPtr<PrerenderHandle> handle = m_suspendedHandles[0].release();
        m_suspendedHandles.remove(0);
        handle->abandon();
    }
}

void Prerenderer::suspend(ReasonForSuspension reason)
{
    if (reason == DocumentWillBecomeInactive || reason == PageWillBeSuspended) {
        while (!m_activeHandles.isEmpty()) {
            RefPtr<PrerenderHandle> handle = m_activeHandles[0].release();
            m_activeHandles.remove(0);
            handle->suspend();
            m_suspendedHandles.append(handle);
        }
    }
}

void Prerenderer::resume()
{
    while (!m_suspendedHandles.isEmpty()) {
        RefPtr<PrerenderHandle> handle = m_suspendedHandles[0].release();
        m_suspendedHandles.remove(0);
        handle->resume();
        m_activeHandles.append(handle);
    }
}

Document* Prerenderer::document()
{
    ASSERT(scriptExecutionContext()->isDocument());
    return static_cast<Document*>(scriptExecutionContext());
}

PrerendererClient* Prerenderer::client()
{
    if (!m_initializedClient) {
        // We can't initialize the client in our contructor, because the platform might not have
        // provided our supplement by then.
        m_initializedClient = true;
        m_client = PrerendererClient::from(document()->page());
    }
    return m_client;
}

void Prerenderer::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::DOM);
    ActiveDOMObject::reportMemoryUsage(memoryObjectInfo);
    info.addMember(m_activeHandles);
    info.addMember(m_suspendedHandles);
}

}

#endif // ENABLE(LINK_PRERENDER)
