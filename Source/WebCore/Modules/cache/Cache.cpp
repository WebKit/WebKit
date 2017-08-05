/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Cache.h"

namespace WebCore {

void Cache::match(RequestInfo&&, std::optional<CacheQueryOptions>&&, Ref<DeferredPromise>&& promise)
{
    promise->reject(Exception { TypeError, ASCIILiteral("Not implemented")});
}

void Cache::matchAll(std::optional<RequestInfo>&&, std::optional<CacheQueryOptions>&&, MatchAllPromise&& promise)
{
    promise.reject(Exception { TypeError, ASCIILiteral("Not implemented")});
}

void Cache::add(RequestInfo&&, DOMPromiseDeferred<void>&& promise)
{
    promise.reject(Exception { TypeError, ASCIILiteral("Not implemented")});
}

void Cache::addAll(Vector<RequestInfo>&&, DOMPromiseDeferred<void>&& promise)
{
    promise.reject(Exception { TypeError, ASCIILiteral("Not implemented")});
}

void Cache::put(RequestInfo&&, Ref<FetchResponse>&&, DOMPromiseDeferred<void>&& promise)
{
    promise.reject(Exception { TypeError, ASCIILiteral("Not implemented")});
}

void Cache::remove(RequestInfo&&, std::optional<CacheQueryOptions>&&, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    promise.reject(Exception { TypeError, ASCIILiteral("Not implemented")});
}

void Cache::keys(std::optional<RequestInfo>&&, std::optional<CacheQueryOptions>&&, KeysPromise&& promise)
{
    promise.reject(Exception { TypeError, ASCIILiteral("Not implemented")});
}


} // namespace WebCore
