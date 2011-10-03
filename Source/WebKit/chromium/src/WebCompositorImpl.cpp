/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "WebCompositorImpl.h"

#include "CCThreadImpl.h"
#include "WebCompositorClient.h"
#include "WebInputEvent.h"
#include "cc/CCThreadProxy.h"
#include <wtf/ThreadingPrimitives.h>

using namespace WebCore;

namespace WebKit {

void WebCompositor::setThread(WebThread* compositorThread)
{
    ASSERT(compositorThread);
    CCThreadProxy::setThread(CCThreadImpl::create(compositorThread).leakPtr());
}

int WebCompositorImpl::s_nextAvailableIdentifier = 1;

// These data structures are always allocated from the main thread, but may
// be accessed and mutated on the main or compositor thread.
// s_compositors is deleted when it has no elements. s_compositorsLock is never
// deleted.
HashSet<WebCompositorImpl*>* WebCompositorImpl::s_compositors = 0;
Mutex* WebCompositorImpl::s_compositorsLock = 0;

WebCompositor* WebCompositor::fromIdentifier(int identifier)
{
    return WebCompositorImpl::fromIdentifier(identifier);
}

WebCompositor* WebCompositorImpl::fromIdentifier(int identifier)
{
    ASSERT(CCProxy::isImplThread());
    if (!s_compositorsLock)
        return 0;

    MutexLocker lock(*s_compositorsLock);
    if (!s_compositors)
        return 0;

    for (HashSet<WebCompositorImpl*>::iterator it = s_compositors->begin(); it != s_compositors->end(); ++it) {
        if ((*it)->identifier() == identifier)
            return *it;
    }
    return 0;
}

WebCompositorImpl::WebCompositorImpl()
    : m_client(0)
    , m_identifier(s_nextAvailableIdentifier++)
{
    ASSERT(CCProxy::isMainThread());
    if (!s_compositorsLock)
        s_compositorsLock = new Mutex;
    MutexLocker lock(*s_compositorsLock);
    if (!s_compositors)
        s_compositors = new HashSet<WebCompositorImpl*>;
    s_compositors->add(this);
}

WebCompositorImpl::~WebCompositorImpl()
{
    ASSERT(s_compositorsLock);
    MutexLocker lock(*s_compositorsLock);
    ASSERT(s_compositors);
    s_compositors->remove(this);
    if (!s_compositors->size()) {
        delete s_compositors;
        s_compositors = 0;
    }
}

void WebCompositorImpl::setClient(WebCompositorClient* client)
{
    ASSERT(CCProxy::isImplThread());
    ASSERT(client);
    m_client = client;
}

void WebCompositorImpl::handleInputEvent(const WebInputEvent& event)
{
    ASSERT(CCProxy::isImplThread());
    // FIXME: Do something interesting with the event here.
    m_client->didHandleInputEvent(false);
}

}

