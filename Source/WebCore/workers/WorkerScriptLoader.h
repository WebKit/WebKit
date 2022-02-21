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

#include "CertificateInfo.h"
#include "ContentSecurityPolicyResponseHeaders.h"
#include "CrossOriginEmbedderPolicy.h"
#include "FetchOptions.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptBuffer.h"
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
class ScriptExecutionContext;
class TextResourceDecoder;
class WorkerScriptLoaderClient;
struct WorkerFetchResult;
enum class CertificateInfoPolicy : uint8_t;

class WorkerScriptLoader : public RefCounted<WorkerScriptLoader>, public ThreadableLoaderClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<WorkerScriptLoader> create()
    {
        return adoptRef(*new WorkerScriptLoader);
    }

    enum class Source : uint8_t { ClassicWorkerScript, ClassicWorkerImport, ModuleScript };

    std::optional<Exception> loadSynchronously(ScriptExecutionContext*, const URL&, Source, FetchOptions::Mode, FetchOptions::Cache, ContentSecurityPolicyEnforcement, const String& initiatorIdentifier);
    void loadAsynchronously(ScriptExecutionContext&, ResourceRequest&&, Source, FetchOptions&&, ContentSecurityPolicyEnforcement, ServiceWorkersMode, WorkerScriptLoaderClient&, String&& taskMode);

    void notifyError();

    const ScriptBuffer& script() const { return m_script; }
    const ContentSecurityPolicyResponseHeaders& contentSecurityPolicy() const { return m_contentSecurityPolicy; }
    const String& referrerPolicy() const { return m_referrerPolicy; }
    const CrossOriginEmbedderPolicy& crossOriginEmbedderPolicy() const { return m_crossOriginEmbedderPolicy; }
    const URL& url() const { return m_url; }
    const URL& lastRequestURL() const { return m_lastRequestURL; }
    const URL& responseURL() const;
    ResourceResponse::Source responseSource() const { return m_responseSource; }
    bool isRedirected() const { return m_isRedirected; }
    const CertificateInfo& certificateInfo() const { return m_certificateInfo; }
    const String& responseMIMEType() const { return m_responseMIMEType; }
    bool failed() const { return m_failed; }
    ResourceLoaderIdentifier identifier() const { return m_identifier; }
    const ResourceError& error() const { return m_error; }

    WorkerFetchResult fetchResult() const;

    void redirectReceived(const URL& redirectURL) override;
    void didReceiveResponse(ResourceLoaderIdentifier, const ResourceResponse&) override;
    void didReceiveData(const SharedBuffer&) override;
    void didFinishLoading(ResourceLoaderIdentifier, const NetworkLoadMetrics&) override;
    void didFail(const ResourceError&) override;

    void cancel();

    WEBCORE_EXPORT static ResourceError validateWorkerResponse(const ResourceResponse&, Source, FetchOptions::Destination);

private:
    friend class RefCounted<WorkerScriptLoader>;
    friend struct std::default_delete<WorkerScriptLoader>;

    WorkerScriptLoader();
    ~WorkerScriptLoader();

    std::unique_ptr<ResourceRequest> createResourceRequest(const String& initiatorIdentifier);
    void notifyFinished();

    WorkerScriptLoaderClient* m_client { nullptr };
    RefPtr<ThreadableLoader> m_threadableLoader;
    RefPtr<TextResourceDecoder> m_decoder;
    ScriptBuffer m_script;
    URL m_url;
    URL m_lastRequestURL;
    URL m_responseURL;
    CertificateInfo m_certificateInfo;
    String m_responseMIMEType;
    Source m_source;
    FetchOptions::Destination m_destination;
    ContentSecurityPolicyResponseHeaders m_contentSecurityPolicy;
    String m_referrerPolicy;
    CrossOriginEmbedderPolicy m_crossOriginEmbedderPolicy;
    ResourceLoaderIdentifier m_identifier;
    bool m_failed { false };
    bool m_finishing { false };
    bool m_isRedirected { false };
    bool m_isCOEPEnabled { false };
    ResourceResponse::Source m_responseSource { ResourceResponse::Source::Unknown };
    ResourceError m_error;
};

} // namespace WebCore
