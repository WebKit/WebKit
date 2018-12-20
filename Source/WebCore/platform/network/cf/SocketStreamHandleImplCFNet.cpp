/*
 * Copyright (C) 2009-2016 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "SocketStreamHandleImpl.h"

#include "Credential.h"
#include "CredentialStorage.h"
#include "DeprecatedGlobalSettings.h"
#include "Logging.h"
#include "NetworkStorageSession.h"
#include "ProtectionSpace.h"
#include "SocketStreamError.h"
#include "SocketStreamHandleClient.h"
#include <CFNetwork/CFNetwork.h>
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/SoftLinking.h>
#include <wtf/cf/TypeCastsCF.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(WIN)
#include "LoaderRunLoopCF.h"
#include <pal/spi/cf/CFNetworkSPI.h>
#endif

#if PLATFORM(IOS_FAMILY)
#include "WebCoreThreadInternal.h"
#endif

#if PLATFORM(IOS_FAMILY) || PLATFORM(MAC)
extern "C" const CFStringRef kCFStreamPropertySourceApplication;
extern "C" const CFStringRef _kCFStreamSocketSetNoDelay;
#endif

#if PLATFORM(COCOA)
#import <pal/spi/cf/CFNetworkSPI.h>
#endif

#if PLATFORM(WIN)
SOFT_LINK_LIBRARY(CFNetwork);
SOFT_LINK_OPTIONAL(CFNetwork, _CFHTTPMessageSetResponseProxyURL, void, __cdecl, (CFHTTPMessageRef, CFURLRef));
#endif

WTF_DECLARE_CF_TYPE_TRAIT(CFHTTPMessage);

namespace WebCore {

static inline CFRunLoopRef callbacksRunLoop()
{
#if PLATFORM(WIN)
    return loaderRunLoop();
#elif PLATFORM(IOS_FAMILY)
    return WebThreadRunLoop();
#else
    return CFRunLoopGetMain();
#endif
}

static inline auto callbacksRunLoopMode()
{
#if PLATFORM(WIN)
    return kCFRunLoopDefaultMode;
#else
    return kCFRunLoopCommonModes;
#endif
}

SocketStreamHandleImpl::SocketStreamHandleImpl(const URL& url, SocketStreamHandleClient& client, PAL::SessionID sessionID, const String& credentialPartition, SourceApplicationAuditToken&& auditData)
    : SocketStreamHandle(url, client)
    , m_connectingSubstate(New)
    , m_connectionType(Unknown)
    , m_sentStoredCredentials(false)
    , m_sessionID(sessionID)
    , m_credentialPartition(credentialPartition)
    , m_auditData(WTFMove(auditData))
{
    LOG(Network, "SocketStreamHandle %p new client %p", this, &m_client);

    ASSERT(url.protocolIs("ws") || url.protocolIs("wss"));

    URL httpsURL(URL(), "https://" + m_url.host());
    m_httpsURL = httpsURL.createCFURL();

#if PLATFORM(COCOA)
    // Don't check for HSTS violation for ephemeral sessions since
    // HSTS state should not transfer between regular and private browsing.
    if (url.protocolIs("ws")
        && !sessionID.isEphemeral()
        && _CFNetworkIsKnownHSTSHostWithSession(m_httpsURL.get(), nullptr)) {
        // Call this asynchronously because the socket stream is not fully constructed at this point.
        callOnMainThread([this, protectedThis = makeRef(*this)] {
            m_client.didFailSocketStream(*this, SocketStreamError(0, m_url.string(), "WebSocket connection failed because it violates HTTP Strict Transport Security."));
        });
        return;
    }
#endif

    createStreams();
    ASSERT(!m_readStream == !m_writeStream);
    if (!m_readStream) // Doing asynchronous PAC file processing, streams will be created later.
        return;

    scheduleStreams();
}

void SocketStreamHandleImpl::scheduleStreams()
{
    ASSERT(m_readStream);
    ASSERT(m_writeStream);

    CFStreamClientContext clientContext = { 0, this, retainSocketStreamHandle, releaseSocketStreamHandle, copyCFStreamDescription };
    // FIXME: Pass specific events we're interested in instead of -1.
    CFReadStreamSetClient(m_readStream.get(), static_cast<CFOptionFlags>(-1), readStreamCallback, &clientContext);
    CFWriteStreamSetClient(m_writeStream.get(), static_cast<CFOptionFlags>(-1), writeStreamCallback, &clientContext);

    CFReadStreamScheduleWithRunLoop(m_readStream.get(), callbacksRunLoop(), callbacksRunLoopMode());
    CFWriteStreamScheduleWithRunLoop(m_writeStream.get(), callbacksRunLoop(), callbacksRunLoopMode());

    CFReadStreamOpen(m_readStream.get());
    CFWriteStreamOpen(m_writeStream.get());

    if (m_pacRunLoopSource)
        removePACRunLoopSource();

    m_connectingSubstate = WaitingForConnect;
}

void* SocketStreamHandleImpl::retainSocketStreamHandle(void* info)
{
    SocketStreamHandle* handle = static_cast<SocketStreamHandle*>(info);
    handle->ref();
    return handle;
}

void SocketStreamHandleImpl::releaseSocketStreamHandle(void* info)
{
    SocketStreamHandle* handle = static_cast<SocketStreamHandle*>(info);
    handle->deref();
}

CFStringRef SocketStreamHandleImpl::copyPACExecutionDescription(void*)
{
    return CFSTR("WebSocket proxy PAC file execution");
}

struct MainThreadPACCallbackInfo {
    MainThreadPACCallbackInfo(SocketStreamHandle* handle, CFArrayRef proxyList)
        : handle(handle), proxyList(proxyList)
    { }
    RefPtr<SocketStreamHandle> handle;
    CFArrayRef proxyList;
};

void SocketStreamHandleImpl::pacExecutionCallback(void* client, CFArrayRef proxyList, CFErrorRef)
{
    SocketStreamHandleImpl* handle = static_cast<SocketStreamHandleImpl*>(client);

    RefPtr<SocketStreamHandle> protector(handle);
    callOnMainThreadAndWait([&] {
        ASSERT(handle->m_connectingSubstate == ExecutingPACFile);
        // This time, the array won't have PAC as a first entry.
        if (handle->m_state != Connecting)
            return;
        handle->chooseProxyFromArray(proxyList);
        handle->createStreams();
        handle->scheduleStreams();
    });
}

void SocketStreamHandleImpl::executePACFileURL(CFURLRef pacFileURL)
{
    // CFNetwork returns an empty proxy array for WebSocket schemes, so use m_httpsURL.
    CFStreamClientContext clientContext = { 0, this, retainSocketStreamHandle, releaseSocketStreamHandle, copyPACExecutionDescription };
    m_pacRunLoopSource = adoptCF(CFNetworkExecuteProxyAutoConfigurationURL(pacFileURL, m_httpsURL.get(), pacExecutionCallback, &clientContext));
    CFRunLoopAddSource(callbacksRunLoop(), m_pacRunLoopSource.get(), callbacksRunLoopMode());
    m_connectingSubstate = ExecutingPACFile;
}

void SocketStreamHandleImpl::removePACRunLoopSource()
{
    ASSERT(m_pacRunLoopSource);

    CFRunLoopSourceInvalidate(m_pacRunLoopSource.get());
    CFRunLoopRemoveSource(callbacksRunLoop(), m_pacRunLoopSource.get(), callbacksRunLoopMode());
    m_pacRunLoopSource = 0;
}

void SocketStreamHandleImpl::chooseProxy()
{
    RetainPtr<CFDictionaryRef> proxyDictionary = adoptCF(CFNetworkCopySystemProxySettings());

    // SOCKS or HTTPS (AKA CONNECT) proxies are supported.
    // WebSocket protocol relies on handshake being transferred unchanged, so we need a proxy that will not modify headers.
    // Since HTTP proxies must add Via headers, they are highly unlikely to work.
    // Many CONNECT proxies limit connectivity to port 443, so we prefer SOCKS, if configured.

    if (!proxyDictionary) {
        m_connectionType = Direct;
        return;
    }

    // CFNetworkCopyProxiesForURL doesn't know about WebSocket schemes, so pretend to use http.
    // Always use "https" to get HTTPS proxies in result - we'll try to use those for ws:// even though many are configured to reject connections to ports other than 443.
    RetainPtr<CFArrayRef> proxyArray = adoptCF(CFNetworkCopyProxiesForURL(m_httpsURL.get(), proxyDictionary.get()));

    chooseProxyFromArray(proxyArray.get());
}

void SocketStreamHandleImpl::chooseProxyFromArray(CFArrayRef proxyArray)
{
    if (!proxyArray) {
        m_connectionType = Direct;
        return;
    }

    CFIndex proxyArrayCount = CFArrayGetCount(proxyArray);

    // PAC is always the first entry, if present.
    if (proxyArrayCount) {
        CFDictionaryRef proxyInfo = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(proxyArray, 0));
        CFTypeRef proxyType = CFDictionaryGetValue(proxyInfo, kCFProxyTypeKey);
        if (proxyType && CFGetTypeID(proxyType) == CFStringGetTypeID()) {
            if (CFEqual(proxyType, kCFProxyTypeAutoConfigurationURL)) {
                CFTypeRef pacFileURL = CFDictionaryGetValue(proxyInfo, kCFProxyAutoConfigurationURLKey);
                if (pacFileURL && CFGetTypeID(pacFileURL) == CFURLGetTypeID()) {
                    executePACFileURL(static_cast<CFURLRef>(pacFileURL));
                    return;
                }
            }
        }
    }

    CFDictionaryRef chosenProxy = 0;
    for (CFIndex i = 0; i < proxyArrayCount; ++i) {
        CFDictionaryRef proxyInfo = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(proxyArray, i));
        CFTypeRef proxyType = CFDictionaryGetValue(proxyInfo, kCFProxyTypeKey);
        if (proxyType && CFGetTypeID(proxyType) == CFStringGetTypeID()) {
            if (CFEqual(proxyType, kCFProxyTypeSOCKS)) {
                m_connectionType = SOCKSProxy;
                chosenProxy = proxyInfo;
                break;
            }
            if (CFEqual(proxyType, kCFProxyTypeHTTPS)) {
                m_connectionType = CONNECTProxy;
                chosenProxy = proxyInfo;
                // Keep looking for proxies, as a SOCKS one is preferable.
            }
        }
    }

    if (chosenProxy) {
        ASSERT(m_connectionType != Unknown);
        ASSERT(m_connectionType != Direct);

        CFTypeRef proxyHost = CFDictionaryGetValue(chosenProxy, kCFProxyHostNameKey);
        CFTypeRef proxyPort = CFDictionaryGetValue(chosenProxy, kCFProxyPortNumberKey);

        if (proxyHost && CFGetTypeID(proxyHost) == CFStringGetTypeID() && proxyPort && CFGetTypeID(proxyPort) == CFNumberGetTypeID()) {
            m_proxyHost = static_cast<CFStringRef>(proxyHost);
            m_proxyPort = static_cast<CFNumberRef>(proxyPort);
            return;
        }
    }

    m_connectionType = Direct;
}

static void setCONNECTProxyForStream(CFReadStreamRef stream, CFStringRef proxyHost, CFNumberRef proxyPort)
{
    const void* proxyKeys[] = { kCFStreamPropertyCONNECTProxyHost, kCFStreamPropertyCONNECTProxyPort };
    const void* proxyValues[] = { proxyHost, proxyPort };
    auto connectDictionary = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, proxyKeys, proxyValues, sizeof(proxyKeys) / sizeof(*proxyKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFReadStreamSetProperty(stream, kCFStreamPropertyCONNECTProxy, connectDictionary.get());
}

void SocketStreamHandleImpl::createStreams()
{
    if (m_connectionType == Unknown)
        chooseProxy();

    // If it's still unknown, then we're resolving a PAC file asynchronously.
    if (m_connectionType == Unknown)
        return;

    RetainPtr<CFStringRef> host = m_url.host().createCFString();

    // Creating streams to final destination, not to proxy.
    CFReadStreamRef readStream = 0;
    CFWriteStreamRef writeStream = 0;
    CFStreamCreatePairWithSocketToHost(0, host.get(), port(), &readStream, &writeStream);
#if PLATFORM(COCOA)
    // <rdar://problem/12855587> _kCFStreamSocketSetNoDelay is not exported on Windows
    CFWriteStreamSetProperty(writeStream, _kCFStreamSocketSetNoDelay, kCFBooleanTrue);
    if (m_auditData.sourceApplicationAuditData && m_auditData.sourceApplicationAuditData.get()) {
        CFReadStreamSetProperty(readStream, kCFStreamPropertySourceApplication, m_auditData.sourceApplicationAuditData.get());
        CFWriteStreamSetProperty(writeStream, kCFStreamPropertySourceApplication, m_auditData.sourceApplicationAuditData.get());
    }
    
#endif

    m_readStream = adoptCF(readStream);
    m_writeStream = adoptCF(writeStream);

    switch (m_connectionType) {
    case Unknown:
        ASSERT_NOT_REACHED();
        break;
    case Direct:
        break;
    case SOCKSProxy: {
        // FIXME: SOCKS5 doesn't do challenge-response, should we try to apply credentials from Keychain right away?
        // But SOCKS5 credentials don't work at the time of this writing anyway, see <rdar://6776698>.
        const void* proxyKeys[] = { kCFStreamPropertySOCKSProxyHost, kCFStreamPropertySOCKSProxyPort };
        const void* proxyValues[] = { m_proxyHost.get(), m_proxyPort.get() };
        RetainPtr<CFDictionaryRef> connectDictionary = adoptCF(CFDictionaryCreate(0, proxyKeys, proxyValues, WTF_ARRAY_LENGTH(proxyKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        CFReadStreamSetProperty(m_readStream.get(), kCFStreamPropertySOCKSProxy, connectDictionary.get());
        break;
        }
    case CONNECTProxy:
        setCONNECTProxyForStream(m_readStream.get(), m_proxyHost.get(), m_proxyPort.get());
        break;
    }

    if (shouldUseSSL()) {
        CFBooleanRef validateCertificateChain = DeprecatedGlobalSettings::allowsAnySSLCertificate() ? kCFBooleanFalse : kCFBooleanTrue;
        const void* keys[] = { kCFStreamSSLPeerName, kCFStreamSSLLevel, kCFStreamSSLValidatesCertificateChain };
        const void* values[] = { host.get(), kCFStreamSocketSecurityLevelNegotiatedSSL, validateCertificateChain };
        RetainPtr<CFDictionaryRef> settings = adoptCF(CFDictionaryCreate(0, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        CFReadStreamSetProperty(m_readStream.get(), kCFStreamPropertySSLSettings, settings.get());
        CFWriteStreamSetProperty(m_writeStream.get(), kCFStreamPropertySSLSettings, settings.get());
    }
}

bool SocketStreamHandleImpl::getStoredCONNECTProxyCredentials(const ProtectionSpace& protectionSpace, String& login, String& password)
{
    // FIXME (<rdar://problem/10416495>): Proxy credentials should be retrieved from AuthBrokerAgent.

    // Try system credential storage first, matching HTTP behavior (CFNetwork only asks the client for password if it couldn't find it in Keychain).
    Credential storedCredential;
    if (auto* storageSession = NetworkStorageSession::storageSession(m_sessionID)) {
        storedCredential = storageSession->credentialStorage().getFromPersistentStorage(protectionSpace);
        if (storedCredential.isEmpty())
            storedCredential = storageSession->credentialStorage().get(m_credentialPartition, protectionSpace);
    }

    if (storedCredential.isEmpty())
        return false;

    login = storedCredential.user();
    password = storedCredential.password();

    return true;
}

static ProtectionSpaceAuthenticationScheme authenticationSchemeFromAuthenticationMethod(CFStringRef method)
{
    if (CFEqual(method, kCFHTTPAuthenticationSchemeBasic))
        return ProtectionSpaceAuthenticationSchemeHTTPBasic;
    if (CFEqual(method, kCFHTTPAuthenticationSchemeDigest))
        return ProtectionSpaceAuthenticationSchemeHTTPDigest;
    if (CFEqual(method, kCFHTTPAuthenticationSchemeNTLM))
        return ProtectionSpaceAuthenticationSchemeNTLM;
    if (CFEqual(method, kCFHTTPAuthenticationSchemeNegotiate))
        return ProtectionSpaceAuthenticationSchemeNegotiate;
    ASSERT_NOT_REACHED();
    return ProtectionSpaceAuthenticationSchemeUnknown;
}
    
static void setCONNECTProxyAuthorizationForStream(CFReadStreamRef stream, CFStringRef proxyAuthorizationString)
{
    auto originalCONNECTDictionary = adoptCF((CFDictionaryRef)CFReadStreamCopyProperty(stream, kCFStreamPropertyCONNECTProxy));
    auto connectDictionary = adoptCF(CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, originalCONNECTDictionary.get()));

    const void* headerFieldNames[] = { CFSTR("Proxy-Authorization") };
    const void* headerFieldValues[] = { proxyAuthorizationString };
    auto additionalHeaderFields = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, headerFieldNames, headerFieldValues, sizeof(headerFieldNames) / sizeof(*headerFieldValues), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    CFDictionarySetValue(connectDictionary.get(), kCFStreamPropertyCONNECTAdditionalHeaders, additionalHeaderFields.get());
    CFReadStreamSetProperty(stream, kCFStreamPropertyCONNECTProxy, connectDictionary.get());
}

void SocketStreamHandleImpl::addCONNECTCredentials(CFHTTPMessageRef proxyResponse)
{
    RetainPtr<CFHTTPAuthenticationRef> authentication = adoptCF(CFHTTPAuthenticationCreateFromResponse(0, proxyResponse));

    if (!CFHTTPAuthenticationRequiresUserNameAndPassword(authentication.get())) {
        // That's all we can offer...
        m_client.didFailSocketStream(*this, SocketStreamError(0, m_url.string(), "Proxy authentication scheme is not supported for WebSockets"));
        return;
    }

    int port = 0;
    CFNumberGetValue(m_proxyPort.get(), kCFNumberIntType, &port);
    RetainPtr<CFStringRef> methodCF = adoptCF(CFHTTPAuthenticationCopyMethod(authentication.get()));
    RetainPtr<CFStringRef> realmCF = adoptCF(CFHTTPAuthenticationCopyRealm(authentication.get()));

    if (!methodCF || !realmCF) {
        // This shouldn't happen, but on some OS versions we get incomplete authentication data, see <rdar://problem/10416316>.
        m_client.didFailSocketStream(*this, SocketStreamError(0, m_url.string(), "WebSocket proxy authentication couldn't be handled"));
        return;
    }

    ProtectionSpace protectionSpace(String(m_proxyHost.get()), port, ProtectionSpaceProxyHTTPS, String(realmCF.get()), authenticationSchemeFromAuthenticationMethod(methodCF.get()));
    String login;
    String password;
    if (!m_sentStoredCredentials && getStoredCONNECTProxyCredentials(protectionSpace, login, password)) {
        // Try to apply stored credentials, if we haven't tried those already.
        // Create a temporary request to make CFNetwork apply credentials to it. Unfortunately, this cannot work with NTLM authentication.
        RetainPtr<CFHTTPMessageRef> dummyRequest = adoptCF(CFHTTPMessageCreateRequest(0, CFSTR("GET"), m_httpsURL.get(), kCFHTTPVersion1_1));

        Boolean appliedCredentials = CFHTTPMessageApplyCredentials(dummyRequest.get(), authentication.get(), login.createCFString().get(), password.createCFString().get(), 0);
        ASSERT_UNUSED(appliedCredentials, appliedCredentials);

        RetainPtr<CFStringRef> proxyAuthorizationString = adoptCF(CFHTTPMessageCopyHeaderFieldValue(dummyRequest.get(), CFSTR("Proxy-Authorization")));

        if (!proxyAuthorizationString) {
            // Fails e.g. for NTLM auth.
            m_client.didFailSocketStream(*this, SocketStreamError(0, m_url.string(), "Proxy authentication scheme is not supported for WebSockets"));
            return;
        }

        // Setting the authorization results in a new connection attempt.
        setCONNECTProxyAuthorizationForStream(m_readStream.get(), proxyAuthorizationString.get());
        m_sentStoredCredentials = true;
        return;
    }

    // FIXME: On platforms where AuthBrokerAgent is not available, ask the client if credentials could not be found.

    m_client.didFailSocketStream(*this, SocketStreamError(0, m_url.string(), "Proxy credentials are not available"));
}

CFStringRef SocketStreamHandleImpl::copyCFStreamDescription(void* info)
{
    SocketStreamHandleImpl* handle = static_cast<SocketStreamHandleImpl*>(info);
    return String("WebKit socket stream, " + handle->m_url.string()).createCFString().leakRef();
}

void SocketStreamHandleImpl::readStreamCallback(CFReadStreamRef stream, CFStreamEventType type, void* clientCallBackInfo)
{
    SocketStreamHandleImpl* handle = static_cast<SocketStreamHandleImpl*>(clientCallBackInfo);
    ASSERT_UNUSED(stream, stream == handle->m_readStream.get());
    // Workaround for <rdar://problem/17727073>. Keeping this below the assertion as we'd like better steps to reproduce this.
    if (!handle->m_readStream)
        return;

    RefPtr<SocketStreamHandle> protector(handle);
    callOnMainThreadAndWait([&] {
        if (handle->m_readStream)
            handle->readStreamCallback(type);
    });
}

void SocketStreamHandleImpl::writeStreamCallback(CFWriteStreamRef stream, CFStreamEventType type, void* clientCallBackInfo)
{
    SocketStreamHandleImpl* handle = static_cast<SocketStreamHandleImpl*>(clientCallBackInfo);
    ASSERT_UNUSED(stream, stream == handle->m_writeStream.get());
    // This wasn't seen happening in practice, yet it seems like it could, due to symmetry with read stream callback.
    if (!handle->m_writeStream)
        return;

    RefPtr<SocketStreamHandle> protector(handle);
    callOnMainThreadAndWait([&] {
        if (handle->m_writeStream)
            handle->writeStreamCallback(type);
    });
}

#if !PLATFORM(IOS_FAMILY)
static void setResponseProxyURL(CFHTTPMessageRef message, CFURLRef proxyURL)
{
#if PLATFORM(WIN)
    if (_CFHTTPMessageSetResponseProxyURLPtr())
        _CFHTTPMessageSetResponseProxyURLPtr()(message, proxyURL);
#else
    _CFHTTPMessageSetResponseProxyURL(message, proxyURL);
#endif
}
#endif

static RetainPtr<CFHTTPMessageRef> copyCONNECTProxyResponse(CFReadStreamRef stream, CFURLRef responseURL, CFStringRef proxyHost, CFNumberRef proxyPort)
{
    auto message = adoptCF(checked_cf_cast<CFHTTPMessageRef>(CFReadStreamCopyProperty(stream, kCFStreamPropertyCONNECTResponse)));
    // CFNetwork needs URL to be set on response in order to handle authentication - even though it doesn't seem to make sense to provide ultimate target URL when authenticating to a proxy.
    // This is set by CFNetwork internally for normal HTTP responses, but not for proxies.
    _CFHTTPMessageSetResponseURL(message.get(), responseURL);

#if !PLATFORM(IOS_FAMILY)
    // Ditto for proxy URL.
    auto proxyURLString = adoptCF(CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("https://%@:%@"), proxyHost, proxyPort));
    auto proxyURL = adoptCF(CFURLCreateWithString(kCFAllocatorDefault, proxyURLString.get(), nullptr));
    setResponseProxyURL(message.get(), proxyURL.get());
#else
    UNUSED_PARAM(proxyHost);
    UNUSED_PARAM(proxyPort);
#endif

    return message;
}

void SocketStreamHandleImpl::readStreamCallback(CFStreamEventType type)
{
    switch (type) {
    case kCFStreamEventNone:
        return;
    case kCFStreamEventOpenCompleted:
        return;
    case kCFStreamEventHasBytesAvailable: {
        if (m_connectingSubstate == WaitingForCredentials)
            return;

        if (m_connectingSubstate == WaitingForConnect) {
            if (m_connectionType == CONNECTProxy) {
                RetainPtr<CFHTTPMessageRef> proxyResponse = copyCONNECTProxyResponse(m_readStream.get(), m_httpsURL.get(), m_proxyHost.get(), m_proxyPort.get());
                if (!proxyResponse)
                    return;

                CFIndex proxyResponseCode = CFHTTPMessageGetResponseStatusCode(proxyResponse.get());
                switch (proxyResponseCode) {
                case 200:
                    // Successful connection.
                    break;
                case 407:
                    addCONNECTCredentials(proxyResponse.get());
                    return;
                default:
                    m_client.didFailSocketStream(*this, SocketStreamError(static_cast<int>(proxyResponseCode), m_url.string(), "Proxy connection could not be established, unexpected response code"));
                    platformClose();
                    return;
                }
            }
            m_connectingSubstate = Connected;
            m_state = Open;
            m_client.didOpenSocketStream(*this);
        }

        // Not an "else if", we could have made a client call above, and it could close the connection.
        if (m_state == Closed)
            return;

        ASSERT(m_state == Open);
        ASSERT(m_connectingSubstate == Connected);

        CFIndex length;
        UInt8 localBuffer[1024]; // Used if CFReadStreamGetBuffer couldn't return anything.
        const UInt8* ptr = CFReadStreamGetBuffer(m_readStream.get(), 0, &length);
        if (!ptr) {
            length = CFReadStreamRead(m_readStream.get(), localBuffer, sizeof(localBuffer));
            ptr = localBuffer;
        }

        if (!length)
            return;

        if (length == -1)
            m_client.didFailToReceiveSocketStreamData(*this);
        else
            m_client.didReceiveSocketStreamData(*this, reinterpret_cast<const char*>(ptr), length);

        return;
    }
    case kCFStreamEventCanAcceptBytes:
        ASSERT_NOT_REACHED();
        return;
    case kCFStreamEventErrorOccurred: {
        RetainPtr<CFErrorRef> error = adoptCF(CFReadStreamCopyError(m_readStream.get()));
        reportErrorToClient(error.get());
        return;
    }
    case kCFStreamEventEndEncountered:
        platformClose();
        return;
    }
}

void SocketStreamHandleImpl::writeStreamCallback(CFStreamEventType type)
{
    switch (type) {
    case kCFStreamEventNone:
        return;
    case kCFStreamEventOpenCompleted:
        return;
    case kCFStreamEventHasBytesAvailable:
        ASSERT_NOT_REACHED();
        return;
    case kCFStreamEventCanAcceptBytes: {
        // Can be false if read stream callback just decided to retry a CONNECT with credentials.
        if (!CFWriteStreamCanAcceptBytes(m_writeStream.get()))
            return;

        if (m_connectingSubstate == WaitingForCredentials)
            return;

        if (m_connectingSubstate == WaitingForConnect) {
            if (m_connectionType == CONNECTProxy) {
                RetainPtr<CFHTTPMessageRef> proxyResponse = copyCONNECTProxyResponse(m_readStream.get(), m_httpsURL.get(), m_proxyHost.get(), m_proxyPort.get());
                if (!proxyResponse)
                    return;

                // Don't write anything until read stream callback has dealt with CONNECT credentials.
                // The order of callbacks is not defined, so this can be called before readStreamCallback's kCFStreamEventHasBytesAvailable.
                CFIndex proxyResponseCode = CFHTTPMessageGetResponseStatusCode(proxyResponse.get());
                if (proxyResponseCode != 200)
                    return;
            }
            m_connectingSubstate = Connected;
            m_state = Open;
            m_client.didOpenSocketStream(*this);
        }

        // Not an "else if", we could have made a client call above, and it could close the connection.
        if (m_state == Closed)
            return;

        ASSERT(m_state == Open);
        ASSERT(m_connectingSubstate == Connected);

        sendPendingData();
        return;
    }
    case kCFStreamEventErrorOccurred: {
        RetainPtr<CFErrorRef> error = adoptCF(CFWriteStreamCopyError(m_writeStream.get()));
        reportErrorToClient(error.get());
        return;
    }
    case kCFStreamEventEndEncountered:
        // FIXME: Currently, we handle closing in read callback, but these can come independently (e.g. a server can stop listening, but keep sending data).
        return;
    }
}

void SocketStreamHandleImpl::reportErrorToClient(CFErrorRef error)
{
    CFIndex errorCode = CFErrorGetCode(error);
    String description;

#if PLATFORM(MAC)

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN

    if (CFEqual(CFErrorGetDomain(error), kCFErrorDomainOSStatus)) {
        const char* descriptionOSStatus = GetMacOSStatusCommentString(static_cast<OSStatus>(errorCode));
        if (descriptionOSStatus && descriptionOSStatus[0] != '\0')
            description = "OSStatus Error " + String::number(errorCode) + ": " + descriptionOSStatus;
    }

    ALLOW_DEPRECATED_DECLARATIONS_END

#endif

    if (description.isNull()) {
        RetainPtr<CFStringRef> descriptionCF = adoptCF(CFErrorCopyDescription(error));
        description = String(descriptionCF.get());
    }

    m_client.didFailSocketStream(*this, SocketStreamError(static_cast<int>(errorCode), m_url.string(), description));
}

SocketStreamHandleImpl::~SocketStreamHandleImpl()
{
    LOG(Network, "SocketStreamHandle %p dtor", this);

    ASSERT(!m_pacRunLoopSource);
}

Optional<size_t> SocketStreamHandleImpl::platformSendInternal(const uint8_t* data, size_t length)
{
    if (!m_writeStream)
        return 0;

    if (!CFWriteStreamCanAcceptBytes(m_writeStream.get()))
        return 0;

    CFIndex result = CFWriteStreamWrite(m_writeStream.get(), reinterpret_cast<const UInt8*>(data), length);
    if (result == -1)
        return WTF::nullopt;

    ASSERT(result >= 0);
    return static_cast<size_t>(result);
}

void SocketStreamHandleImpl::platformClose()
{
    LOG(Network, "SocketStreamHandle %p platformClose", this);

    if (m_pacRunLoopSource) 
        removePACRunLoopSource();

    ASSERT(!m_readStream == !m_writeStream);
    if (!m_readStream) {
        if (m_connectingSubstate == New || m_connectingSubstate == ExecutingPACFile)
            m_client.didCloseSocketStream(*this);
        return;
    }

    CFReadStreamUnscheduleFromRunLoop(m_readStream.get(), callbacksRunLoop(), callbacksRunLoopMode());
    CFWriteStreamUnscheduleFromRunLoop(m_writeStream.get(), callbacksRunLoop(), callbacksRunLoopMode());

    CFReadStreamClose(m_readStream.get());
    CFWriteStreamClose(m_writeStream.get());
    
    m_readStream = nullptr;
    m_writeStream = nullptr;

    m_client.didCloseSocketStream(*this);
}

unsigned short SocketStreamHandleImpl::port() const
{
    if (auto urlPort = m_url.port())
        return urlPort.value();
    if (shouldUseSSL())
        return 443;
    return 80;
}

} // namespace WebCore
