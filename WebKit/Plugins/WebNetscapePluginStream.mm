/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKit/WebNetscapePluginStream.h>

#import <Foundation/NSURLConnection.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/NetscapePlugInStreamLoader.h>
#import <WebKit/WebDataSourceInternal.h>
#import <WebKit/WebFrameBridge.h>
#import <WebKit/WebFrameInternal.h>
#import <WebKit/WebKitErrorsPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNSURLRequestExtras.h>
#import <WebKit/WebNetscapePluginEmbeddedView.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebViewInternal.h>
#import <WebCore/ResourceError.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/PassRefPtr.h>

using namespace WebCore;

@implementation WebNetscapePluginStream

#ifndef BUILDING_ON_TIGER
+ (void)initialize
{
    WebCoreObjCFinalizeOnMainThread(self);
}
#endif

- initWithRequest:(NSURLRequest *)theRequest
           plugin:(NPP)thePlugin
       notifyData:(void *)theNotifyData 
 sendNotification:(BOOL)flag
{   
    WebBaseNetscapePluginView *view = (WebBaseNetscapePluginView *)thePlugin->ndata;

    if (!core([view webFrame])->loader()->canLoad([theRequest URL], core([view webFrame])->document()))
        return nil;

    if ([self initWithRequestURL:[theRequest URL]
                          plugin:thePlugin
                       notifyData:theNotifyData
                 sendNotification:flag] == nil) {
        return nil;
    }
    
    // Temporarily set isTerminated to YES to avoid assertion failure in dealloc in case we are released in this method.
    isTerminated = YES;
    
    request = [theRequest mutableCopy];
    if (core([view webFrame])->loader()->shouldHideReferrer([theRequest URL], core([view webFrame])->loader()->outgoingReferrer()))
        [(NSMutableURLRequest *)request _web_setHTTPReferrer:nil];

    _loader = NetscapePlugInStreamLoader::create(core([view webFrame]), self).releaseRef();
    
    isTerminated = NO;

    return self;
}

- (void)dealloc
{
    if (_loader)
        _loader->deref();
    [request release];
    [super dealloc];
}

- (void)finalize
{
    ASSERT_MAIN_THREAD();
    if (_loader)
        _loader->deref();
    [super finalize];
}

- (void)start
{
    ASSERT(request);

    _loader->documentLoader()->addPlugInStreamLoader(_loader);
    if (!_loader->load(request))
        _loader->documentLoader()->removePlugInStreamLoader(_loader);
}

- (void)cancelLoadWithError:(NSError *)error
{
    if (!_loader->isDone())
        _loader->cancel(error);
}

- (void)stop
{
    if (!_loader->isDone())
        [self cancelLoadAndDestroyStreamWithError:_loader->cancelledError()];
}

@end

