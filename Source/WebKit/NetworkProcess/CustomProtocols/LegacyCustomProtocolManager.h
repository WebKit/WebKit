/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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

#include "DataReference.h"
#include "LegacyCustomProtocolID.h"
#include "MessageReceiver.h"
#include "NetworkProcessSupplement.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
OBJC_CLASS NSURLSessionConfiguration;
OBJC_CLASS WKCustomProtocol;
#endif

namespace WebCore {
class ResourceError;
class ResourceRequest;
class ResourceResponse;
} // namespace WebCore

namespace WebKit {

enum class CacheStoragePolicy : uint8_t;
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

    LegacyCustomProtocolID addCustomProtocol(CustomProtocol&&);
    void removeCustomProtocol(LegacyCustomProtocolID);
    void startLoading(LegacyCustomProtocolID, const WebCore::ResourceRequest&);
    void stopLoading(LegacyCustomProtocolID);

#if PLATFORM(COCOA)
    void registerProtocolClass(NSURLSessionConfiguration*);
    static void networkProcessCreated(NetworkProcess&);
#endif

private:
    // NetworkProcessSupplement
    void initialize(const NetworkProcessCreationParameters&) override;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void didFailWithError(LegacyCustomProtocolID, const WebCore::ResourceError&);
    void didLoadData(LegacyCustomProtocolID, const IPC::DataReference&);
    void didReceiveResponse(LegacyCustomProtocolID, const WebCore::ResourceResponse&, CacheStoragePolicy);
    void didFinishLoading(LegacyCustomProtocolID);
    void wasRedirectedToRequest(LegacyCustomProtocolID, const WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse);

    void registerProtocolClass();

    NetworkProcess& m_networkProcess;

    typedef HashMap<LegacyCustomProtocolID, CustomProtocol> CustomProtocolMap;
    CustomProtocolMap m_customProtocolMap WTF_GUARDED_BY_LOCK(m_customProtocolMapLock);
    Lock m_customProtocolMapLock;

#if PLATFORM(COCOA)
    HashSet<String, ASCIICaseInsensitiveHash> m_registeredSchemes WTF_GUARDED_BY_LOCK(m_registeredSchemesLock);
    Lock m_registeredSchemesLock;

    // WKCustomProtocol objects can be removed from the m_customProtocolMap from multiple threads.
    // We return a RetainPtr here because it is unsafe to return a raw pointer since the object might immediately be destroyed from a different thread.
    RetainPtr<WKCustomProtocol> protocolForID(LegacyCustomProtocolID);
#endif
};

} // namespace WebKit
