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

#include "Extensions3DChromium.h"
#include "GraphicsContext3D.h"
#include "TraceEvent.h"

namespace WebCore {

PassRefPtr<RateLimiter> RateLimiter::create(GraphicsContext3D* context, RateLimiterClient *client)
{
    return adoptRef(new RateLimiter(context, client));
}

RateLimiter::RateLimiter(GraphicsContext3D* context, RateLimiterClient *client)
    : m_context(context)
    , m_timer(this, &RateLimiter::rateLimitContext)
    , m_client(client)
{
    ASSERT(context);
    ASSERT(context->getExtensions());
    m_contextSupportsRateLimitingExtension = context->getExtensions()->supports("GL_CHROMIUM_rate_limit_offscreen_context");
}

RateLimiter::~RateLimiter()
{
}

void RateLimiter::start()
{
    if (m_contextSupportsRateLimitingExtension && !m_timer.isActive())
        m_timer.startOneShot(0);
}

void RateLimiter::stop()
{
    m_timer.stop();
}

void RateLimiter::rateLimitContext(Timer<RateLimiter>*)
{
    TRACE_EVENT("RateLimiter::rateLimitContext", this, 0);

    Extensions3DChromium* extensions = static_cast<Extensions3DChromium*>(m_context->getExtensions());

    m_client->rateLimit();
    extensions->rateLimitOffscreenContextCHROMIUM();
}

}
#endif // USE(ACCELERATED_COMPOSITING)
