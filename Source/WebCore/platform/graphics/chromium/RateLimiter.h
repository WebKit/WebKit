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

#ifndef RateLimiter_h
#define RateLimiter_h

#if USE(ACCELERATED_COMPOSITING)

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebKit {
class WebGraphicsContext3D;
}

namespace WebCore {

class RateLimiterClient {
public:
    virtual void rateLimit() = 0;
};

// A RateLimiter can be used to make sure that a single context does not dominate all execution time.
// To use, construct a RateLimiter class around the context and call start() whenever calls are made on the
// context outside of normal flow control. RateLimiter will block if the context is too far ahead of the
// compositor.
class RateLimiter : public RefCounted<RateLimiter> {
public:
    static PassRefPtr<RateLimiter> create(WebKit::WebGraphicsContext3D*, RateLimiterClient*);
    ~RateLimiter();

    void start();

    // Context and client will not be accessed after stop().
    void stop();

private:
    RateLimiter(WebKit::WebGraphicsContext3D*, RateLimiterClient*);

    class Task;
    friend class Task;
    void rateLimitContext();

    WebKit::WebGraphicsContext3D* m_context;
    bool m_active;
    RateLimiterClient *m_client;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
