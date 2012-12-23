/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#if ENABLE(CUSTOM_PROTOCOLS)

#import "CustomProtocolManagerMessages.h"
#import "CustomProtocolManagerProxyMessages.h"
#import "DataReference.h"
#import "WebCoreArgumentCoders.h"
#import "WebProcess.h"
#import <WebCore/KURL.h>
#import <WebCore/ResourceError.h>

using namespace WebKit;

static uint64_t generateCustomProtocolID()
{
    static uint64_t uniqueCustomProtocolID = 0;
    return ++uniqueCustomProtocolID;
}

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
    return CustomProtocolManager::shared().supportsScheme([[[request URL] scheme] lowercaseString]);
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
    CustomProtocolManager::shared().addCustomProtocol(self);
    return self;
}

- (void)startLoading
{
    CustomProtocolManager::shared().childProcess()->send(Messages::CustomProtocolManagerProxy::StartLoading(self.customProtocolID, [self request]), 0);
}

- (void)stopLoading
{
    CustomProtocolManager::shared().childProcess()->send(Messages::CustomProtocolManagerProxy::StopLoading(self.customProtocolID), 0);
    CustomProtocolManager::shared().removeCustomProtocol(self);
}

@end

namespace WebKit {

CustomProtocolManager& CustomProtocolManager::shared()
{
    DEFINE_STATIC_LOCAL(CustomProtocolManager, customProtocolManager, ());
    return customProtocolManager;
}

CustomProtocolManager::CustomProtocolManager()
    : m_childProcess(0)
{
}

void CustomProtocolManager::initialize(ChildProcess* childProcess)
{
    ASSERT(!m_childProcess);
    ASSERT(childProcess);

    m_childProcess = childProcess;
    m_childProcess->addMessageReceiver(Messages::CustomProtocolManager::messageReceiverName(), this);
}

void CustomProtocolManager::connectionEstablished()
{
    [NSURLProtocol registerClass:[WKCustomProtocol class]];
}

void CustomProtocolManager::addCustomProtocol(WKCustomProtocol *customProtocol)
{
    ASSERT(customProtocol);
    m_customProtocolMap.add(customProtocol.customProtocolID, customProtocol);
}

void CustomProtocolManager::removeCustomProtocol(WKCustomProtocol *customProtocol)
{
    ASSERT(customProtocol);
    m_customProtocolMap.remove(customProtocol.customProtocolID);
}
    
void CustomProtocolManager::registerScheme(const String& scheme)
{
    m_registeredSchemes.add(scheme);
}
    
void CustomProtocolManager::unregisterScheme(const String& scheme)
{
    m_registeredSchemes.remove(scheme);
}

bool CustomProtocolManager::supportsScheme(const String& scheme)
{
    return m_registeredSchemes.contains(scheme);
}

void CustomProtocolManager::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder)
{
    didReceiveCustomProtocolManagerMessage(connection, messageID, decoder);
}

void CustomProtocolManager::didFailWithError(uint64_t customProtocolID, const WebCore::ResourceError& error)
{
    WKCustomProtocol *protocol = protocolForID(customProtocolID);
    if (!protocol)
        return;
    
    [[protocol client] URLProtocol:protocol didFailWithError:error.nsError()];
    removeCustomProtocol(protocol);
}

void CustomProtocolManager::didLoadData(uint64_t customProtocolID, const CoreIPC::DataReference& data)
{
    WKCustomProtocol *protocol = protocolForID(customProtocolID);
    if (!protocol)
        return;
    
    [[protocol client] URLProtocol:protocol didLoadData:[NSData dataWithBytes:(void*)data.data() length:data.size()]];
}

void CustomProtocolManager::didReceiveResponse(uint64_t customProtocolID, const WebCore::ResourceResponse& response, uint32_t cacheStoragePolicy)
{
    WKCustomProtocol *protocol = protocolForID(customProtocolID);
    if (!protocol)
        return;
    
    [[protocol client] URLProtocol:protocol didReceiveResponse:response.nsURLResponse() cacheStoragePolicy:static_cast<NSURLCacheStoragePolicy>(cacheStoragePolicy)];
}

void CustomProtocolManager::didFinishLoading(uint64_t customProtocolID)
{
    WKCustomProtocol *protocol = protocolForID(customProtocolID);
    if (!protocol)
        return;
    
    [[protocol client] URLProtocolDidFinishLoading:protocol];
    removeCustomProtocol(protocol);
}

WKCustomProtocol *CustomProtocolManager::protocolForID(uint64_t customProtocolID)
{
    CustomProtocolMap::const_iterator it = m_customProtocolMap.find(customProtocolID);
    if (it == m_customProtocolMap.end())
        return nil;
    
    ASSERT(it->value);
    return it->value.get();
}

} // namespace WebKit

#endif // ENABLE(CUSTOM_PROTOCOLS)
