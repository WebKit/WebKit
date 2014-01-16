/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef DownloadProxy_h
#define DownloadProxy_h

#include "APIObject.h"
#include "Connection.h"
#include "SandboxExtension.h"
#include <WebCore/ResourceRequest.h>
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>

namespace API {
class Data;
}

namespace WebCore {
    class AuthenticationChallenge;
    class ResourceError;
    class ResourceResponse;
}

namespace WebKit {

class DownloadProxyMap;
class WebContext;
class WebPageProxy;

class DownloadProxy : public API::ObjectImpl<API::Object::Type::Download>, public IPC::MessageReceiver {
public:
    static PassRefPtr<DownloadProxy> create(DownloadProxyMap&, WebContext&);
    ~DownloadProxy();

    uint64_t downloadID() const { return m_downloadID; }
    const WebCore::ResourceRequest& request() const { return m_request; }
    API::Data* resumeData() const { return m_resumeData.get(); }

    void cancel();

    void invalidate();
    void processDidClose();

    void didReceiveDownloadProxyMessage(IPC::Connection*, IPC::MessageDecoder&);
    void didReceiveSyncDownloadProxyMessage(IPC::Connection*, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&);

private:
    explicit DownloadProxy(DownloadProxyMap&, WebContext&);

    // IPC::MessageReceiver
    virtual void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&) override;
    virtual void didReceiveSyncMessage(IPC::Connection*, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&) override;

    // Message handlers.
    void didStart(const WebCore::ResourceRequest&);
    void didReceiveAuthenticationChallenge(const WebCore::AuthenticationChallenge&, uint64_t challengeID);
    void didReceiveResponse(const WebCore::ResourceResponse&);
    void didReceiveData(uint64_t length);
    void shouldDecodeSourceDataOfMIMEType(const String& mimeType, bool& result);
    void decideDestinationWithSuggestedFilename(const String& filename, String& destination, bool& allowOverwrite, SandboxExtension::Handle& sandboxExtensionHandle);
    void didCreateDestination(const String& path);
    void didFinish();
    void didFail(const WebCore::ResourceError&, const IPC::DataReference& resumeData);
    void didCancel(const IPC::DataReference& resumeData);

    DownloadProxyMap& m_downloadProxyMap;
    RefPtr<WebContext> m_webContext;
    uint64_t m_downloadID;

    RefPtr<API::Data> m_resumeData;
    WebCore::ResourceRequest m_request;
};

} // namespace WebKit

#endif // DownloadProxy_h
