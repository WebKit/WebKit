/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "AuthenticationChallengeDisposition.h"
#include "AuthenticationChallengeProxy.h"
#include "DataReference.h"
#include <wtf/CompletionHandler.h>

namespace WebCore {
class Credential;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
}

namespace API {

class Data;

class DataTaskClient : public RefCounted<DataTaskClient> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<DataTaskClient> create() { return adoptRef(*new DataTaskClient); }
    virtual ~DataTaskClient() { }

    virtual void didReceiveChallenge(DataTask&, WebCore::AuthenticationChallenge&&, CompletionHandler<void(WebKit::AuthenticationChallengeDisposition, WebCore::Credential&&)>&& completionHandler) const { completionHandler(WebKit::AuthenticationChallengeDisposition::RejectProtectionSpaceAndContinue, { }); }
    virtual void willPerformHTTPRedirection(DataTask&, WebCore::ResourceResponse&&, WebCore::ResourceRequest&&, CompletionHandler<void(bool)>&& completionHandler) const { completionHandler(true); }
    virtual void didReceiveResponse(DataTask&, WebCore::ResourceResponse&&, CompletionHandler<void(bool)>&& completionHandler) const { completionHandler(true); }
    virtual void didReceiveData(DataTask&, const IPC::DataReference&) const { }
    virtual void didCompleteWithError(DataTask&, WebCore::ResourceError&&) const { }
};

} // namespace API
