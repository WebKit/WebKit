/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "WKCACFContextFlusher.h"

#include <wtf/StdLibExtras.h>
#include <QuartzCore/CACFContext.h>

namespace WebCore {

WKCACFContextFlusher& WKCACFContextFlusher::shared()
{
    DEFINE_STATIC_LOCAL(WKCACFContextFlusher, flusher, ());
    return flusher;
}

WKCACFContextFlusher::WKCACFContextFlusher()
{
}

WKCACFContextFlusher::~WKCACFContextFlusher()
{
}

void WKCACFContextFlusher::addContext(CACFContextRef context)
{
    ASSERT(context);

    m_contexts.add(context);
    CFRetain(context);
}

void WKCACFContextFlusher::removeContext(CACFContextRef context)
{
    ASSERT(context);

    ContextSet::iterator found = m_contexts.find(context);
    if (found == m_contexts.end())
        return;

    CFRelease(*found);
    m_contexts.remove(found);
}

void WKCACFContextFlusher::flushAllContexts()
{
    // addContext might get called beneath CACFContextFlush, and we don't want m_contexts to change while
    // we're iterating over it, so we move the contexts into a local ContextSet and iterate over that instead.
    ContextSet contextsToFlush;
    contextsToFlush.swap(m_contexts);

    ContextSet::const_iterator end = contextsToFlush.end();
    for (ContextSet::const_iterator it = contextsToFlush.begin(); it != end; ++it) {
        CACFContextRef context = *it;
        CACFContextFlush(context);
        CFRelease(context);
    }
}

}

#endif // USE(ACCELERATED_COMPOSITING)
