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

#import "HostedNetscapePluginStream.h"

#import "NetscapePluginHostProxy.h"
#import "NetscapePluginInstanceProxy.h"
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import "WebHostedNetscapePluginView.h"
#import "WebFrameInternal.h"
#import "WebNSURLRequestExtras.h"
#import "WebKitPluginHost.h"
#import "WebKitSystemInterface.h"

using namespace WebCore;

namespace WebKit {

HostedNetscapePluginStream::HostedNetscapePluginStream(NetscapePluginInstanceProxy* instance, uint32_t streamID, NSURLRequest *request)
    : m_instance(instance)
    , m_streamID(streamID)
    , m_isTerminated(false)
    , m_request(AdoptNS, [request mutableCopy])
{
    if (core([instance->pluginView() webFrame])->loader()->shouldHideReferrer([request URL], core([instance->pluginView() webFrame])->loader()->outgoingReferrer()))
        [m_request.get() _web_setHTTPReferrer:nil];
}

void HostedNetscapePluginStream::startStreamWithResponse(NSURLResponse *response)
{
    didReceiveResponse(0, response);
}
    
void HostedNetscapePluginStream::startStream(NSURL *, long long expectedContentLength, NSDate *lastModifiedDate, NSString *mimeType, NSData *headers)
{
    char* mimeTypeUTF8 = const_cast<char*>([mimeType UTF8String]);
    int mimeTypeUTF8Length = mimeTypeUTF8 ? strlen (mimeTypeUTF8) + 1 : 0;
    
    _WKPHStartStream(m_instance->hostProxy()->port(),
                     m_instance->pluginID(),
                     m_streamID,
                     expectedContentLength,
                     [lastModifiedDate timeIntervalSince1970],
                     mimeTypeUTF8, mimeTypeUTF8Length,
                     const_cast<char*>(reinterpret_cast<const char*>([headers bytes])), [headers length]);
}
                     
void HostedNetscapePluginStream::didReceiveData(WebCore::NetscapePlugInStreamLoader*, const char* bytes, int length)
{
    _WKPHStreamDidReceiveData(m_instance->hostProxy()->port(),
                              m_instance->pluginID(),
                              m_streamID,
                              const_cast<char*>(bytes), length);
}
    
void HostedNetscapePluginStream::didFinishLoading(WebCore::NetscapePlugInStreamLoader*)
{
    _WKPHStreamDidFinishLoading(m_instance->hostProxy()->port(),
                                m_instance->pluginID(),
                                m_streamID);
}
    
    
void HostedNetscapePluginStream::didReceiveResponse(NetscapePlugInStreamLoader*, const ResourceResponse& response)
{
    NSURLResponse *r = response.nsURLResponse();
    
    NSMutableData *theHeaders = nil;
    long long expectedContentLength = [r expectedContentLength];
    
    if ([r isKindOfClass:[NSHTTPURLResponse class]]) {
        NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)r;
        theHeaders = [NSMutableData dataWithCapacity:1024];
        
        // FIXME: it would be nice to be able to get the raw HTTP header block.
        // This includes the HTTP version, the real status text,
        // all headers in their original order and including duplicates,
        // and all original bytes verbatim, rather than sent through Unicode translation.
        // Unfortunately NSHTTPURLResponse doesn't provide access at that low a level.
        
        [theHeaders appendBytes:"HTTP " length:5];
        char statusStr[10];
        long statusCode = [httpResponse statusCode];
        snprintf(statusStr, sizeof(statusStr), "%ld", statusCode);
        [theHeaders appendBytes:statusStr length:strlen(statusStr)];
        [theHeaders appendBytes:" OK\n" length:4];
        
        // HACK: pass the headers through as UTF-8.
        // This is not the intended behavior; we're supposed to pass original bytes verbatim.
        // But we don't have the original bytes, we have NSStrings built by the URL loading system.
        // It hopefully shouldn't matter, since RFC2616/RFC822 require ASCII-only headers,
        // but surely someone out there is using non-ASCII characters, and hopefully UTF-8 is adequate here.
        // It seems better than NSASCIIStringEncoding, which will lose information if non-ASCII is used.
        
        NSDictionary *headerDict = [httpResponse allHeaderFields];
        NSArray *keys = [[headerDict allKeys] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
        NSEnumerator *i = [keys objectEnumerator];
        NSString *k;
        while ((k = [i nextObject]) != nil) {
            NSString *v = [headerDict objectForKey:k];
            [theHeaders appendData:[k dataUsingEncoding:NSUTF8StringEncoding]];
            [theHeaders appendBytes:": " length:2];
            [theHeaders appendData:[v dataUsingEncoding:NSUTF8StringEncoding]];
            [theHeaders appendBytes:"\n" length:1];
        }
        
        // If the content is encoded (most likely compressed), then don't send its length to the plugin,
        // which is only interested in the decoded length, not yet known at the moment.
        // <rdar://problem/4470599> tracks a request for -[NSURLResponse expectedContentLength] to incorporate this logic.
        NSString *contentEncoding = (NSString *)[[(NSHTTPURLResponse *)r allHeaderFields] objectForKey:@"Content-Encoding"];
        if (contentEncoding && ![contentEncoding isEqualToString:@"identity"])
            expectedContentLength = -1;
        
        // startStreamResponseURL:... will null-terminate.
    }
    
    startStream([r URL], expectedContentLength, WKGetNSURLResponseLastModifiedDate(r), [r MIMEType], theHeaders);
}

} // namespace WebKit

#endif // USE(PLUGIN_HOST_PROCESS)

