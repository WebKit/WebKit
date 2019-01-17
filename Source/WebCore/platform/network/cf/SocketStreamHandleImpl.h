/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "SocketStreamHandle.h"
#include <pal/SessionID.h>
#include <wtf/RetainPtr.h>
#include <wtf/StreamBuffer.h>

typedef struct __CFHTTPMessage* CFHTTPMessageRef;

namespace WebCore {

class Credential;
class StorageSessionProvider;
class ProtectionSpace;
class SocketStreamHandleClient;

class SocketStreamHandleImpl : public SocketStreamHandle {
public:
    static Ref<SocketStreamHandleImpl> create(const URL& url, SocketStreamHandleClient& client, PAL::SessionID sessionID, const String& credentialPartition, SourceApplicationAuditToken&& auditData, const StorageSessionProvider* provider) { return adoptRef(*new SocketStreamHandleImpl(url, client, sessionID, credentialPartition, WTFMove(auditData), provider)); }

    virtual ~SocketStreamHandleImpl();

    WEBCORE_EXPORT void platformSend(const uint8_t* data, size_t length, Function<void(bool)>&&) final;
    WEBCORE_EXPORT void platformSendHandshake(const uint8_t* data, size_t length, const Optional<CookieRequestHeaderFieldProxy>&, Function<void(bool, bool)>&&) final;
    WEBCORE_EXPORT void platformClose() final;
private:
    size_t bufferedAmount() final;
    Optional<size_t> platformSendInternal(const uint8_t*, size_t);
    bool sendPendingData();

    WEBCORE_EXPORT SocketStreamHandleImpl(const URL&, SocketStreamHandleClient&, PAL::SessionID, const String& credentialPartition, SourceApplicationAuditToken&&, const StorageSessionProvider*);
    void createStreams();
    void scheduleStreams();
    void chooseProxy();
    void chooseProxyFromArray(CFArrayRef);
    void executePACFileURL(CFURLRef);
    void removePACRunLoopSource();
    RetainPtr<CFRunLoopSourceRef> m_pacRunLoopSource;
    static void pacExecutionCallback(void* client, CFArrayRef proxyList, CFErrorRef);
    static CFStringRef copyPACExecutionDescription(void*);

    bool shouldUseSSL() const { return m_url.protocolIs("wss"); }
    unsigned short port() const;

    void addCONNECTCredentials(CFHTTPMessageRef response);

    static void* retainSocketStreamHandle(void*);
    static void releaseSocketStreamHandle(void*);
    static CFStringRef copyCFStreamDescription(void*);
    static void readStreamCallback(CFReadStreamRef, CFStreamEventType, void*);
    static void writeStreamCallback(CFWriteStreamRef, CFStreamEventType, void*);
    void readStreamCallback(CFStreamEventType);
    void writeStreamCallback(CFStreamEventType);

    void reportErrorToClient(CFErrorRef);

    bool getStoredCONNECTProxyCredentials(const ProtectionSpace&, String& login, String& password);

    enum ConnectingSubstate { New, ExecutingPACFile, WaitingForCredentials, WaitingForConnect, Connected };
    ConnectingSubstate m_connectingSubstate;

    enum ConnectionType { Unknown, Direct, SOCKSProxy, CONNECTProxy };
    ConnectionType m_connectionType;
    RetainPtr<CFStringRef> m_proxyHost;
    RetainPtr<CFNumberRef> m_proxyPort;

    RetainPtr<CFHTTPMessageRef> m_proxyResponseMessage;
    bool m_sentStoredCredentials;
    RetainPtr<CFReadStreamRef> m_readStream;
    RetainPtr<CFWriteStreamRef> m_writeStream;

    RetainPtr<CFURLRef> m_httpsURL; // ws(s): replaced with https:
    PAL::SessionID m_sessionID;
    String m_credentialPartition;
    SourceApplicationAuditToken m_auditData;
    RefPtr<const StorageSessionProvider> m_storageSessionProvider;

    StreamBuffer<uint8_t, 1024 * 1024> m_buffer;
    static const unsigned maxBufferSize = 100 * 1024 * 1024;
};

} // namespace WebCore
