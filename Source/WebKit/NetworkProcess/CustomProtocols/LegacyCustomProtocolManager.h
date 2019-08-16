/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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

#include "MessageReceiver.h"
#include "NetworkProcessSupplement.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
OBJC_CLASS NSURLSessionConfiguration;
OBJC_CLASS WKCustomProtocol;
#endif

#if USE(SOUP)
#include <wtf/glib/GRefPtr.h>

typedef struct _GCancellable GCancellable;
typedef struct _GInputStream GInputStream;
typedef struct _GTask GTask;
typedef struct _WebKitSoupRequestGeneric WebKitSoupRequestGeneric;
#endif

namespace IPC {
class DataReference;
} // namespace IPC

namespace WebCore {
class ResourceError;
class ResourceRequest;
class ResourceResponse;
} // namespace WebCore

namespace WebKit {

class NetworkProcess;
struct NetworkProcessCreationParameters;

class LegacyCustomProtocolManager : public NetworkProcessSupplement, public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(LegacyCustomProtocolManager);
public:
    explicit LegacyCustomProtocolManager(NetworkProcess&);

    static const char* supplementName();

    void registerScheme(const String&);
    void unregisterScheme(const String&);
    bool supportsScheme(const String&);

#if PLATFORM(COCOA)
    typedef RetainPtr<WKCustomProtocol> CustomProtocol;
#endif
#if USE(SOUP)
    struct WebSoupRequestAsyncData {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        WebSoupRequestAsyncData(GRefPtr<GTask>&&, WebKitSoupRequestGeneric*);
        ~WebSoupRequestAsyncData();

        GRefPtr<GTask> task;
        WebKitSoupRequestGeneric* request;
        GRefPtr<GCancellable> cancellable;
        GRefPtr<GInputStream> stream;
    };
    typedef std::unique_ptr<WebSoupRequestAsyncData> CustomProtocol;
#endif

    uint64_t addCustomProtocol(CustomProtocol&&);
    void removeCustomProtocol(uint64_t customProtocolID);
    void startLoading(uint64_t customProtocolID, const WebCore::ResourceRequest&);
    void stopLoading(uint64_t customProtocolID);

#if PLATFORM(COCOA)
    void registerProtocolClass(NSURLSessionConfiguration*);
#endif
#if PLATFORM(COCOA) || USE(SOUP)
    static void networkProcessCreated(NetworkProcess&);
#endif

private:
    // NetworkProcessSupplement
    void initialize(const NetworkProcessCreationParameters&) override;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void didFailWithError(uint64_t customProtocolID, const WebCore::ResourceError&);
    void didLoadData(uint64_t customProtocolID, const IPC::DataReference&);
    void didReceiveResponse(uint64_t customProtocolID, const WebCore::ResourceResponse&, uint32_t cacheStoragePolicy);
    void didFinishLoading(uint64_t customProtocolID);
    void wasRedirectedToRequest(uint64_t customProtocolID, const WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse);

    void registerProtocolClass();

    NetworkProcess& m_networkProcess;

    typedef HashMap<uint64_t, CustomProtocol> CustomProtocolMap;
    CustomProtocolMap m_customProtocolMap;
    Lock m_customProtocolMapMutex;

#if PLATFORM(COCOA)
    HashSet<String, ASCIICaseInsensitiveHash> m_registeredSchemes;
    Lock m_registeredSchemesMutex;

    // WKCustomProtocol objects can be removed from the m_customProtocolMap from multiple threads.
    // We return a RetainPtr here because it is unsafe to return a raw pointer since the object might immediately be destroyed from a different thread.
    RetainPtr<WKCustomProtocol> protocolForID(uint64_t customProtocolID);
#endif

#if USE(SOUP)
    GRefPtr<GPtrArray> m_registeredSchemes;
#endif
};

} // namespace WebKit

