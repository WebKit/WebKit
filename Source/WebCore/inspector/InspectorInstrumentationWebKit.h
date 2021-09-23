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
#include "ResourceError.h"
#include "ResourceLoader.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Function.h>

namespace WebCore {

class Frame;
class ResourceError;
class ResourceLoader;
class ResourceRequest;
class ResourceResponse;
class SharedBuffer;

class WEBCORE_EXPORT InspectorInstrumentationWebKit {
public:
    static bool shouldInterceptRequest(const Frame*, const ResourceRequest&);
    static bool shouldInterceptResponse(const Frame*, const ResourceResponse&);
    static void interceptRequest(ResourceLoader&, Function<void(const ResourceRequest&)>&&);
<<<<<<< ours
    static void interceptResponse(const Frame*, const ResourceResponse&, ResourceLoaderIdentifier, CompletionHandler<void(const ResourceResponse&, RefPtr<SharedBuffer>)>&&);
||||||| base
    static void interceptResponse(const Frame*, const ResourceResponse&, unsigned long identifier, CompletionHandler<void(const ResourceResponse&, RefPtr<SharedBuffer>)>&&);
=======
    static void interceptResponse(const Frame*, const ResourceResponse&, unsigned long identifier, CompletionHandler<void(std::optional<ResourceError>&& error, const ResourceResponse&, RefPtr<SharedBuffer>)>&&);
    static void interceptDidReceiveData(const Frame*, unsigned long identifier, const SharedBuffer&);
    static void interceptDidFinishResourceLoad(const Frame*, unsigned long identifier);
    static void interceptDidFailResourceLoad(const Frame*, unsigned long identifier, const ResourceError& error);
>>>>>>> theirs

private:
    static bool shouldInterceptRequestInternal(const Frame&, const ResourceRequest&);
    static bool shouldInterceptResponseInternal(const Frame&, const ResourceResponse&);
    static void interceptRequestInternal(ResourceLoader&, Function<void(const ResourceRequest&)>&&);
<<<<<<< ours
    static void interceptResponseInternal(const Frame&, const ResourceResponse&, ResourceLoaderIdentifier, CompletionHandler<void(const ResourceResponse&, RefPtr<SharedBuffer>)>&&);
||||||| base
    static void interceptResponseInternal(const Frame&, const ResourceResponse&, unsigned long identifier, CompletionHandler<void(const ResourceResponse&, RefPtr<SharedBuffer>)>&&);
=======
    static void interceptResponseInternal(const Frame&, const ResourceResponse&, unsigned long identifier, CompletionHandler<void(std::optional<ResourceError>&& error, const ResourceResponse&, RefPtr<SharedBuffer>)>&&);
    static void interceptDidReceiveDataInternal(const Frame&, unsigned long identifier, const SharedBuffer&);
    static void interceptDidFinishResourceLoadInternal(const Frame&, unsigned long identifier);
    static void interceptDidFailResourceLoadInternal(const Frame&, unsigned long identifier, const ResourceError& error);
>>>>>>> theirs
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

inline void InspectorInstrumentationWebKit::interceptRequest(ResourceLoader& loader, Function<void(const ResourceRequest&)>&& handler)
{
    ASSERT(InspectorInstrumentationWebKit::shouldInterceptRequest(loader.frame(), loader.request()));
    interceptRequestInternal(loader, WTFMove(handler));
}

<<<<<<< ours
inline void InspectorInstrumentationWebKit::interceptResponse(const Frame* frame, const ResourceResponse& response, ResourceLoaderIdentifier identifier, CompletionHandler<void(const ResourceResponse&, RefPtr<SharedBuffer>)>&& handler)
||||||| base
inline void InspectorInstrumentationWebKit::interceptResponse(const Frame* frame, const ResourceResponse& response, unsigned long identifier, CompletionHandler<void(const ResourceResponse&, RefPtr<SharedBuffer>)>&& handler)
=======
inline void InspectorInstrumentationWebKit::interceptResponse(const Frame* frame, const ResourceResponse& response, unsigned long identifier, CompletionHandler<void(std::optional<ResourceError>&& error, const ResourceResponse&, RefPtr<SharedBuffer>)>&& handler)
>>>>>>> theirs
{
    ASSERT(InspectorInstrumentationWebKit::shouldInterceptResponse(frame, response));
    interceptResponseInternal(*frame, response, identifier, WTFMove(handler));
}

inline void InspectorInstrumentationWebKit::interceptDidReceiveData(const Frame* frame, unsigned long identifier, const SharedBuffer& buffer)
{
    if (!frame)
        return;

    interceptDidReceiveDataInternal(*frame, identifier, buffer);
}

inline void InspectorInstrumentationWebKit::interceptDidFinishResourceLoad(const Frame* frame, unsigned long identifier)
{
    if (!frame)
        return;

    interceptDidFinishResourceLoadInternal(*frame, identifier);
}

inline void InspectorInstrumentationWebKit::interceptDidFailResourceLoad(const Frame* frame, unsigned long identifier, const ResourceError& error)
{
    if (!frame)
        return;

    interceptDidFailResourceLoadInternal(*frame, identifier, error);
}

}
