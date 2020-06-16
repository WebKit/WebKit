/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#pragma once

#include "InspectorInstrumentationPublic.h"
#include <wtf/CompletionHandler.h>

namespace WebCore {

class Frame;
class ResourceResponse;
class SharedBuffer;

class WEBCORE_EXPORT InspectorInstrumentationWebKit {
public:
    static bool shouldInterceptRequest(const Frame*, const ResourceRequest&);
    static bool shouldInterceptResponse(const Frame*, const ResourceResponse&);
    static void interceptRequest(ResourceLoader&, CompletionHandler<void(const ResourceRequest&)>&&);
    static void interceptResponse(const Frame*, const ResourceResponse&, unsigned long identifier, CompletionHandler<void(const ResourceResponse&, RefPtr<SharedBuffer>)>&&);

private:
    static bool shouldInterceptRequestInternal(const Frame&, const ResourceRequest&);
    static bool shouldInterceptResponseInternal(const Frame&, const ResourceResponse&);
    static void interceptRequestInternal(ResourceLoader&, CompletionHandler<void(const ResourceRequest&)>&&);
    static void interceptResponseInternal(const Frame&, const ResourceResponse&, unsigned long identifier, CompletionHandler<void(const ResourceResponse&, RefPtr<SharedBuffer>)>&&);
};

inline bool InspectorInstrumentationWebKit::shouldInterceptRequest(const Frame* frame, const ResourceRequest& request)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (!frame)
        return false;

    return shouldInterceptRequestInternal(*frame, request);
}

inline bool InspectorInstrumentationWebKit::shouldInterceptResponse(const Frame* frame, const ResourceResponse& response)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (!frame)
        return false;

    return shouldInterceptResponseInternal(*frame, response);
}

inline void InspectorInstrumentationWebKit::interceptRequest(ResourceLoader& loader, CompletionHandler<void(const ResourceRequest&)>&& handler)
{
    ASSERT(InspectorInstrumentationWebKit::shouldInterceptRequest(loader.frame(), loader.request()));
    interceptRequestInternal(loader, WTFMove(handler));
}

inline void InspectorInstrumentationWebKit::interceptResponse(const Frame* frame, const ResourceResponse& response, unsigned long identifier, CompletionHandler<void(const ResourceResponse&, RefPtr<SharedBuffer>)>&& handler)
{
    ASSERT(InspectorInstrumentationWebKit::shouldInterceptResponse(frame, response));
    interceptResponseInternal(*frame, response, identifier, WTFMove(handler));
}

}
