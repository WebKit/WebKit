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

#if USE(ACCELERATED_COMPOSITING)
#include "RateLimiter.h"

#include "CCProxy.h"
#include "CCThread.h"
#include "TraceEvent.h"
#include <public/WebGraphicsContext3D.h>

namespace WebCore {

class RateLimiter::Task : public CCThread::Task {
public:
    static PassOwnPtr<Task> create(RateLimiter* rateLimiter)
    {
        return adoptPtr(new Task(rateLimiter));
    }
    virtual ~Task() { }

private:
    explicit Task(RateLimiter* rateLimiter)
        : CCThread::Task(this)
        , m_rateLimiter(rateLimiter)
    {
    }

    virtual void performTask() OVERRIDE
    {
        m_rateLimiter->rateLimitContext();
    }

    RefPtr<RateLimiter> m_rateLimiter;
};

PassRefPtr<RateLimiter> RateLimiter::create(WebKit::WebGraphicsContext3D* context, RateLimiterClient *client)
{
    return adoptRef(new RateLimiter(context, client));
}

RateLimiter::RateLimiter(WebKit::WebGraphicsContext3D* context, RateLimiterClient *client)
    : m_context(context)
    , m_active(false)
    , m_client(client)
{
    ASSERT(context);
}

RateLimiter::~RateLimiter()
{
}

void RateLimiter::start()
{
    if (m_active)
        return;

    TRACE_EVENT0("cc", "RateLimiter::start");
    m_active = true;
    CCProxy::mainThread()->postTask(RateLimiter::Task::create(this));
}

void RateLimiter::stop()
{
    TRACE_EVENT0("cc", "RateLimiter::stop");
    m_client = 0;
}

void RateLimiter::rateLimitContext()
{
    if (!m_client)
        return;

    TRACE_EVENT0("cc", "RateLimiter::rateLimitContext");

    m_active = false;
    m_client->rateLimit();
    m_context->rateLimitOffscreenContextCHROMIUM();
}

}
#endif // USE(ACCELERATED_COMPOSITING)
