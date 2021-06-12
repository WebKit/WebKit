/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#if USE(PLUGIN_HOST_PROCESS)

#ifndef HostedNetscapePluginStream_h
#define HostedNetscapePluginStream_h

#include <WebCore/NetscapePlugInStreamLoader.h>
#include <WebKitLegacy/npapi.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

namespace WebCore {
    class FrameLoader;
    class NetscapePlugInStreamLoader;
}

namespace WebKit {

class NetscapePluginInstanceProxy;

class HostedNetscapePluginStream : public RefCounted<HostedNetscapePluginStream>
                                 , private WebCore::NetscapePlugInStreamLoaderClient {
public:
    static Ref<HostedNetscapePluginStream> create(NetscapePluginInstanceProxy* instance, uint32_t streamID, NSURLRequest *request)
    {
        return adoptRef(*new HostedNetscapePluginStream(instance, streamID, request));
    }
    static Ref<HostedNetscapePluginStream> create(NetscapePluginInstanceProxy* instance, WebCore::FrameLoader* frameLoader)
    {
        return adoptRef(*new HostedNetscapePluginStream(instance, frameLoader));
    }

    ~HostedNetscapePluginStream();

    uint32_t streamID() const { return m_streamID; }

    void startStreamWithResponse(NSURLResponse *response);

    // FIXME: Can these be made private?
    void didReceiveData(WebCore::NetscapePlugInStreamLoader*, const uint8_t* bytes, int length) override;
    void didFinishLoading(WebCore::NetscapePlugInStreamLoader*) override;
    void didFail(WebCore::NetscapePlugInStreamLoader*, const WebCore::ResourceError&) override;

    void start();
    void stop();

    void cancelLoad(NPReason reason);

    static NPReason reasonForError(NSError* error);

private:
    NSError *errorForReason(NPReason) const;
    void cancelLoad(NSError *);

    HostedNetscapePluginStream(NetscapePluginInstanceProxy*, uint32_t streamID, NSURLRequest *);
    HostedNetscapePluginStream(NetscapePluginInstanceProxy*, WebCore::FrameLoader*);
    
    void startStream(NSURL *, long long expectedContentLength, NSDate *lastModifiedDate, NSString *mimeType, NSData *headers);

    NSError *pluginCancelledConnectionError() const;

    // NetscapePlugInStreamLoaderClient methods.
    void willSendRequest(WebCore::NetscapePlugInStreamLoader*, WebCore::ResourceRequest&&, const WebCore::ResourceResponse& redirectResponse, CompletionHandler<void(WebCore::ResourceRequest&&)>&&) override;
    void didReceiveResponse(WebCore::NetscapePlugInStreamLoader*, const WebCore::ResourceResponse&) override;
    bool wantsAllStreams() const override;
    
    RefPtr<NetscapePluginInstanceProxy> m_instance;
    uint32_t m_streamID;
    RetainPtr<NSMutableURLRequest> m_request;

    RetainPtr<NSURL> m_requestURL;
    RetainPtr<NSURL> m_responseURL;
    RetainPtr<NSString> m_mimeType;

    WebCore::FrameLoader* m_frameLoader;
    RefPtr<WebCore::NetscapePlugInStreamLoader> m_loader;
};

}

#endif // HostedNetscapePluginStream_h
#endif // USE(PLUGIN_HOST_PROCESS)
