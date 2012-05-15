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
#include "Prerender.h"

#include <public/Platform.h>
#include <public/WebPrerender.h>
#include <public/WebPrerenderingSupport.h>
#include <public/WebReferrerPolicy.h>
#include <public/WebString.h>
#include <public/WebURL.h>

#if ENABLE(LINK_PRERENDER)

namespace WebCore {

Prerender::Prerender(const KURL& url, const String& referrer, ReferrerPolicy policy)
    : m_url(url)
    , m_referrer(referrer)
    , m_referrerPolicy(policy)
#ifndef NDEBUG
    , m_state(Inactive)
#endif
{
}

Prerender::~Prerender()
{
}

void Prerender::setState(State state)
{
#ifdef NDEBUG
    UNUSED_PARAM(state);
#else
    m_state = state;
#endif
}

void Prerender::add()
{
    ASSERT(m_state == Inactive);
    WebKit::WebPrerenderingSupport* platform = WebKit::WebPrerenderingSupport::current();
    if (!platform)
        return;
    WebKit::WebPrerender webPrerender(this);
    platform->add(webPrerender);
    setState(Active);
}

void Prerender::cancel()
{
    ASSERT(WebKit::WebPrerenderingSupport::current());
    WebKit::WebPrerenderingSupport* platform = WebKit::WebPrerenderingSupport::current();
    if (!platform)
        return;
    ASSERT(m_state == Active);
    WebKit::WebPrerender webPrerender(this);
    platform->cancel(webPrerender);
    setState(Inactive);
}

void Prerender::abandon()
{
    WebKit::WebPrerenderingSupport* platform = WebKit::WebPrerenderingSupport::current();
    if (!platform)
        return;
    // FIXME: Assert on the state as Inactive here. It is currently common to call abandon() on an Inactive
    // prerender, as the Prerenderer doesn't keep track of which prerenders are active, and so any page that
    // ever had a now-canceled Prerender will get this bogus stop() call.
    WebKit::WebPrerender webPrerender(this);
    platform->abandon(webPrerender);
    setState(Inactive);
}

void Prerender::suspend()
{
    abandon();
}

void Prerender::resume()
{
    add();
}

}

#endif // ENABLE(LINK_PRERENDER)
