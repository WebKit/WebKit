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

#include "config.h"
#include "InspectorInstrumentationWebKit.h"

#include "InspectorInstrumentation.h"

namespace WebCore {

bool InspectorInstrumentationWebKit::shouldInterceptRequestInternal(const Frame& frame, const ResourceRequest& request)
{
    return InspectorInstrumentation::shouldInterceptRequest(frame, request);
}

bool InspectorInstrumentationWebKit::shouldInterceptResponseInternal(const Frame& frame, const ResourceResponse& response)
{
    return InspectorInstrumentation::shouldInterceptResponse(frame, response);
}

void InspectorInstrumentationWebKit::interceptRequestInternal(ResourceLoader& loader, Function<void(const ResourceRequest&)>&& handler)
{
    InspectorInstrumentation::interceptRequest(loader, WTFMove(handler));
}

<<<<<<< ours
void InspectorInstrumentationWebKit::interceptResponseInternal(const Frame& frame, const ResourceResponse& response, ResourceLoaderIdentifier identifier, CompletionHandler<void(const ResourceResponse&, RefPtr<SharedBuffer>)>&& handler)
||||||| base
void InspectorInstrumentationWebKit::interceptResponseInternal(const Frame& frame, const ResourceResponse& response, unsigned long identifier, CompletionHandler<void(const ResourceResponse&, RefPtr<SharedBuffer>)>&& handler)
=======
void InspectorInstrumentationWebKit::interceptResponseInternal(const Frame& frame, const ResourceResponse& response, unsigned long identifier, CompletionHandler<void(std::optional<ResourceError>&& error, const ResourceResponse&, RefPtr<SharedBuffer>)>&& handler)
>>>>>>> theirs
{
    InspectorInstrumentation::interceptResponse(frame, response, identifier, WTFMove(handler));
}

void InspectorInstrumentationWebKit::interceptDidReceiveDataInternal(const Frame& frame, unsigned long identifier, const SharedBuffer& buffer)
{
    InspectorInstrumentation::interceptDidReceiveData(frame, identifier, buffer);
}

void InspectorInstrumentationWebKit::interceptDidFinishResourceLoadInternal(const Frame& frame, unsigned long identifier)
{
    InspectorInstrumentation::interceptDidFinishResourceLoad(frame, identifier);
}

void InspectorInstrumentationWebKit::interceptDidFailResourceLoadInternal(const Frame& frame, unsigned long identifier, const ResourceError& error)
{
    InspectorInstrumentation::interceptDidFailResourceLoad(frame, identifier, error);
}

} // namespace WebCore
