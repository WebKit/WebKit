/*
 * Copyright (C) 2008-2017 Apple Inc. All Rights Reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "DOMApplicationCache.h"

#include "ApplicationCacheHost.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"

namespace WebCore {

DOMApplicationCache::DOMApplicationCache(DOMWindow& window)
    : DOMWindowProperty(&window)
{
    if (auto* host = applicationCacheHost())
        host->setDOMApplicationCache(this);
}

ApplicationCacheHost* DOMApplicationCache::applicationCacheHost() const
{
    auto* frame = this->frame();
    if (!frame)
        return nullptr;
    auto* documentLoader = frame->loader().documentLoader();
    if (!documentLoader)
        return nullptr;
    return &documentLoader->applicationCacheHost();
}

unsigned short DOMApplicationCache::status() const
{
    auto* host = applicationCacheHost();
    if (!host)
        return ApplicationCacheHost::UNCACHED;
    return host->status();
}

ExceptionOr<void> DOMApplicationCache::update()
{
    auto* host = applicationCacheHost();
    if (!host || !host->update())
        return Exception { InvalidStateError };
    return { };
}

ExceptionOr<void> DOMApplicationCache::swapCache()
{
    auto* host = applicationCacheHost();
    if (!host || !host->swapCache())
        return Exception { InvalidStateError };
    return { };
}

void DOMApplicationCache::abort()
{
    if (auto* host = applicationCacheHost())
        host->abort();
}

ScriptExecutionContext* DOMApplicationCache::scriptExecutionContext() const
{
    auto* frame = this->frame();
    if (!frame)
        return nullptr;
    return frame->document();
}

} // namespace WebCore
