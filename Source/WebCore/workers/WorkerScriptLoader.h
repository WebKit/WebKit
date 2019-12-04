/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009, 2011 Google Inc. All Rights Reserved.
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

#pragma once

#include "ContentSecurityPolicyResponseHeaders.h"
#include "FetchOptions.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ThreadableLoader.h"
#include "ThreadableLoaderClient.h"
#include <memory>
#include <wtf/FastMalloc.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/URL.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

class Exception;
class ResourceResponse;
class ScriptExecutionContext;
class TextResourceDecoder;
class WorkerScriptLoaderClient;

class WorkerScriptLoader : public RefCounted<WorkerScriptLoader>, public ThreadableLoaderClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<WorkerScriptLoader> create()
    {
        return adoptRef(*new WorkerScriptLoader);
    }

    Optional<Exception> loadSynchronously(ScriptExecutionContext*, const URL&, FetchOptions::Mode, FetchOptions::Cache, ContentSecurityPolicyEnforcement, const String& initiatorIdentifier);
    void loadAsynchronously(ScriptExecutionContext&, ResourceRequest&&, FetchOptions&&, ContentSecurityPolicyEnforcement, ServiceWorkersMode, WorkerScriptLoaderClient&);

    void notifyError();

    String script();
    const ContentSecurityPolicyResponseHeaders& contentSecurityPolicy() const { return m_contentSecurityPolicy; }
    const String& referrerPolicy() const { return m_referrerPolicy; }
    const URL& url() const { return m_url; }
    const URL& responseURL() const;
    const String& responseMIMEType() const { return m_responseMIMEType; }
    bool failed() const { return m_failed; }
    unsigned long identifier() const { return m_identifier; }
    const ResourceError& error() const { return m_error; }

    void didReceiveResponse(unsigned long identifier, const ResourceResponse&) override;
    void didReceiveData(const char* data, int dataLength) override;
    void didFinishLoading(unsigned long identifier) override;
    void didFail(const ResourceError&) override;

    void cancel();

    WEBCORE_EXPORT static ResourceError validateWorkerResponse(const ResourceResponse&, FetchOptions::Destination);

private:
    friend class WTF::RefCounted<WorkerScriptLoader>;
    friend struct std::default_delete<WorkerScriptLoader>;

    WorkerScriptLoader();
    ~WorkerScriptLoader();

    std::unique_ptr<ResourceRequest> createResourceRequest(const String& initiatorIdentifier);
    void notifyFinished();

    WorkerScriptLoaderClient* m_client { nullptr };
    RefPtr<ThreadableLoader> m_threadableLoader;
    String m_responseEncoding;
    RefPtr<TextResourceDecoder> m_decoder;
    StringBuilder m_script;
    URL m_url;
    URL m_responseURL;
    String m_responseMIMEType;
    FetchOptions::Destination m_destination;
    ContentSecurityPolicyResponseHeaders m_contentSecurityPolicy;
    String m_referrerPolicy;
    unsigned long m_identifier { 0 };
    bool m_failed { false };
    bool m_finishing { false };
    ResourceError m_error;
};

} // namespace WebCore
