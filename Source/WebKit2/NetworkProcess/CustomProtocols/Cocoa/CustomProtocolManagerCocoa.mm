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

#import "config.h"
#import "CustomProtocolManager.h"

#import "ChildProcess.h"
#import "CustomProtocolManagerMessages.h"
#import "CustomProtocolManagerProxyMessages.h"
#import "DataReference.h"
#import "NetworkProcess.h"
#import "NetworkProcessCreationParameters.h"
#import "WebCoreArgumentCoders.h"
#import "WebProcessCreationParameters.h"
#import <Foundation/NSURLSession.h>
#import <WebCore/NSURLConnectionSPI.h>
#import <WebCore/URL.h>
#import <WebCore/ResourceError.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/ResourceResponse.h>
#import <WebCore/TextEncoding.h>

using namespace WebKit;

namespace WebKit {

static uint64_t generateCustomProtocolID()
{
    static uint64_t uniqueCustomProtocolID = 0;
    return ++uniqueCustomProtocolID;
}

} // namespace WebKit

@interface WKCustomProtocol : NSURLProtocol {
@private
    uint64_t _customProtocolID;
}
@property (nonatomic, readonly) uint64_t customProtocolID;
@end

@implementation WKCustomProtocol

@synthesize customProtocolID = _customProtocolID;

+ (BOOL)canInitWithRequest:(NSURLRequest *)request
{
    if (auto* customProtocolManager = NetworkProcess::singleton().supplement<CustomProtocolManager>())
        return customProtocolManager->supportsScheme([[[request URL] scheme] lowercaseString]);
    return NO;
}

+ (NSURLRequest *)canonicalRequestForRequest:(NSURLRequest *)request
{
    return request;
}

+ (BOOL)requestIsCacheEquivalent:(NSURLRequest *)a toRequest:(NSURLRequest *)b
{
    return NO;
}

- (id)initWithRequest:(NSURLRequest *)request cachedResponse:(NSCachedURLResponse *)cachedResponse client:(id<NSURLProtocolClient>)client
{
    self = [super initWithRequest:request cachedResponse:cachedResponse client:client];
    if (!self)
        return nil;
    
    _customProtocolID = generateCustomProtocolID();
    if (auto* customProtocolManager = NetworkProcess::singleton().supplement<CustomProtocolManager>())
        customProtocolManager->addCustomProtocol(self);
    return self;
}

- (void)startLoading
{
    if (auto* customProtocolManager = NetworkProcess::singleton().supplement<CustomProtocolManager>())
        customProtocolManager->childProcess()->send(Messages::CustomProtocolManagerProxy::StartLoading(self.customProtocolID, [self request]), 0);
}

- (void)stopLoading
{
    if (auto* customProtocolManager = NetworkProcess::singleton().supplement<CustomProtocolManager>()) {
        customProtocolManager->childProcess()->send(Messages::CustomProtocolManagerProxy::StopLoading(self.customProtocolID), 0);
        customProtocolManager->removeCustomProtocol(self);
    }
}

@end

namespace WebKit {

const char* CustomProtocolManager::supplementName()
{
    return "CustomProtocolManager";
}

CustomProtocolManager::CustomProtocolManager(ChildProcess* childProcess)
    : m_childProcess(childProcess)
    , m_messageQueue(WorkQueue::create("com.apple.WebKit.CustomProtocolManager"))
{
    WebCore::UTF8Encoding();
}

void CustomProtocolManager::initializeConnection(IPC::Connection* connection)
{
    connection->addWorkQueueMessageReceiver(Messages::CustomProtocolManager::messageReceiverName(), m_messageQueue.get(), this);
}

void CustomProtocolManager::initialize(const NetworkProcessCreationParameters& parameters)
{
#if !USE(NETWORK_SESSION)
    [NSURLProtocol registerClass:[WKCustomProtocol class]];
#endif

    for (size_t i = 0; i < parameters.urlSchemesRegisteredForCustomProtocols.size(); ++i)
        registerScheme(parameters.urlSchemesRegisteredForCustomProtocols[i]);
}

void CustomProtocolManager::addCustomProtocol(WKCustomProtocol *customProtocol)
{
    ASSERT(customProtocol);
    LockHolder locker(m_customProtocolMapMutex);
    m_customProtocolMap.add(customProtocol.customProtocolID, customProtocol);
}

void CustomProtocolManager::removeCustomProtocol(WKCustomProtocol *customProtocol)
{
    ASSERT(customProtocol);
    LockHolder locker(m_customProtocolMapMutex);
    m_customProtocolMap.remove(customProtocol.customProtocolID);
}

#if USE(NETWORK_SESSION)
void CustomProtocolManager::registerProtocolClass(NSURLSessionConfiguration *configuration)
{
    configuration.protocolClasses = @[[WKCustomProtocol class]];
}
#endif

void CustomProtocolManager::registerScheme(const String& scheme)
{
    ASSERT(!scheme.isNull());
    LockHolder locker(m_registeredSchemesMutex);
    m_registeredSchemes.add(scheme);
}
    
void CustomProtocolManager::unregisterScheme(const String& scheme)
{
    ASSERT(!scheme.isNull());
    LockHolder locker(m_registeredSchemesMutex);
    m_registeredSchemes.remove(scheme);
}

bool CustomProtocolManager::supportsScheme(const String& scheme)
{
    if (scheme.isNull())
        return false;

    LockHolder locker(m_registeredSchemesMutex);
    return m_registeredSchemes.contains(scheme);
}

static inline void dispatchOnResourceLoaderRunLoop(void (^block)())
{
    CFRunLoopPerformBlock([NSURLConnection resourceLoaderRunLoop], kCFRunLoopDefaultMode, block);
    CFRunLoopWakeUp([NSURLConnection resourceLoaderRunLoop]);
}

void CustomProtocolManager::didFailWithError(uint64_t customProtocolID, const WebCore::ResourceError& error)
{
    RetainPtr<WKCustomProtocol> protocol = protocolForID(customProtocolID);
    if (!protocol)
        return;

    RetainPtr<NSError> nsError = error.nsError();

    dispatchOnResourceLoaderRunLoop(^ {
        [[protocol client] URLProtocol:protocol.get() didFailWithError:nsError.get()];
    });

    removeCustomProtocol(protocol.get());
}

void CustomProtocolManager::didLoadData(uint64_t customProtocolID, const IPC::DataReference& data)
{
    RetainPtr<WKCustomProtocol> protocol = protocolForID(customProtocolID);
    if (!protocol)
        return;

    RetainPtr<NSData> nsData = adoptNS([[NSData alloc] initWithBytes:data.data() length:data.size()]);

    dispatchOnResourceLoaderRunLoop(^ {
        [[protocol client] URLProtocol:protocol.get() didLoadData:nsData.get()];
    });
}

void CustomProtocolManager::didReceiveResponse(uint64_t customProtocolID, const WebCore::ResourceResponse& response, uint32_t cacheStoragePolicy)
{
    RetainPtr<WKCustomProtocol> protocol = protocolForID(customProtocolID);
    if (!protocol)
        return;

    RetainPtr<NSURLResponse> nsResponse = response.nsURLResponse();

    dispatchOnResourceLoaderRunLoop(^ {
        [[protocol client] URLProtocol:protocol.get() didReceiveResponse:nsResponse.get() cacheStoragePolicy:static_cast<NSURLCacheStoragePolicy>(cacheStoragePolicy)];
    });
}

void CustomProtocolManager::didFinishLoading(uint64_t customProtocolID)
{
    RetainPtr<WKCustomProtocol> protocol = protocolForID(customProtocolID);
    if (!protocol)
        return;

    dispatchOnResourceLoaderRunLoop(^ {
        [[protocol client] URLProtocolDidFinishLoading:protocol.get()];
    });

    removeCustomProtocol(protocol.get());
}

void CustomProtocolManager::wasRedirectedToRequest(uint64_t customProtocolID, const WebCore::ResourceRequest& request, const WebCore::ResourceResponse& redirectResponse)
{
    RetainPtr<WKCustomProtocol> protocol = protocolForID(customProtocolID);
    if (!protocol)
        return;

    RetainPtr<NSURLRequest> nsRequest = request.nsURLRequest(WebCore::DoNotUpdateHTTPBody);
    RetainPtr<NSURLResponse> nsRedirectResponse = redirectResponse.nsURLResponse();

    dispatchOnResourceLoaderRunLoop([protocol, nsRequest, nsRedirectResponse]() {
        [[protocol client] URLProtocol:protocol.get() wasRedirectedToRequest:nsRequest.get() redirectResponse:nsRedirectResponse.get()];
    });
}

RetainPtr<WKCustomProtocol> CustomProtocolManager::protocolForID(uint64_t customProtocolID)
{
    LockHolder locker(m_customProtocolMapMutex);

    CustomProtocolMap::const_iterator it = m_customProtocolMap.find(customProtocolID);
    if (it == m_customProtocolMap.end())
        return nil;
    
    ASSERT(it->value);
    return it->value;
}

} // namespace WebKit
