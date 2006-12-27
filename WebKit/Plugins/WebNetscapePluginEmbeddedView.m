/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#import "WebNetscapePluginEmbeddedView.h"

#import "WebBaseNetscapePluginViewPrivate.h"
#import "WebDataSource.h"
#import "WebFrame.h"
#import "WebFrameBridge.h"
#import "WebFrameInternal.h"
#import "WebFrameView.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebNSViewExtras.h"
#import "WebNetscapePluginPackage.h"
#import "WebNetscapePluginStream.h"
#import "WebView.h"
#import <JavaScriptCore/Assertions.h>
#import <WebCore/Document.h>
#import <WebCore/Element.h>
#import <WebCore/FrameMac.h>
#import <WebCore/FrameLoader.h>

@implementation WebNetscapePluginEmbeddedView

- (id)initWithFrame:(NSRect)frame
      pluginPackage:(WebNetscapePluginPackage *)thePluginPackage
                URL:(NSURL *)theURL
            baseURL:(NSURL *)theBaseURL
           MIMEType:(NSString *)MIME
      attributeKeys:(NSArray *)keys
    attributeValues:(NSArray *)values
       loadManually:(BOOL)loadManually
         DOMElement:(DOMElement *)anElement
{
    [super initWithFrame:frame];

    // load the plug-in if it is not already loaded
    if (![thePluginPackage load]) {
        [self release];
        return nil;
    }
    [self setPluginPackage:thePluginPackage];

    element = [anElement retain];
    
    URL = [theURL retain];
    
    [self setMIMEType:MIME];
    [self setBaseURL:theBaseURL];
    [self setAttributeKeys:keys andValues:values];
    if (loadManually)
        [self setMode:NP_FULL];
    else
        [self setMode:NP_EMBED];

    _loadManually = loadManually;
    
    return self;
}

- (void)dealloc
{
    [URL release];
    [_manualStream release];
    [_error release];
    [super dealloc];
}

- (void)didStart
{
    if (_loadManually) {
        [self redeliverStream];
        return;
    }
    
    // If the OBJECT/EMBED tag has no SRC, the URL is passed to us as "".
    // Check for this and don't start a load in this case.
    if (URL != nil && ![URL _web_isEmpty]) {
        NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:URL];
        [request _web_setHTTPReferrer:core([self webFrame])->loader()->outgoingReferrer()];
        [self loadRequest:request inTarget:nil withNotifyData:nil sendNotification:NO];
    } 
}

- (WebDataSource *)dataSource
{
    WebFrame *webFrame = kit(core(element)->document()->frame());
    return [webFrame dataSource];
}

-(void)pluginView:(NSView *)pluginView receivedResponse:(NSURLResponse *)response
{
    ASSERT(_loadManually);
    ASSERT(!_manualStream);
    
    _manualStream = [[WebNetscapePluginStream alloc] init];
}

- (void)pluginView:(NSView *)pluginView receivedData:(NSData *)data
{
    ASSERT(_loadManually);
    ASSERT(_manualStream);
    
    _dataLengthReceived += [data length];
    
    if (![self isStarted])
        return;
    
    if ([_manualStream plugin] == NULL) {
        [_manualStream setRequestURL:[[[self dataSource] request] URL]];
        [_manualStream setPlugin:[self plugin]];
        ASSERT([_manualStream plugin]);
        [_manualStream startStreamWithResponse:[[self dataSource] response]];
    }
    
    if ([_manualStream plugin])
        [_manualStream receivedData:data];
}

- (void)pluginView:(NSView *)pluginView receivedError:(NSError *)error
{
    ASSERT(_loadManually);
    
    [error retain];
    [_error release];
    _error = error;
    
    if (![self isStarted]) {
        return;
    }
    
    [_manualStream destroyStreamWithError:error];
}

- (void)pluginViewFinishedLoading:(NSView *)pluginView 
{
    ASSERT(_loadManually);
    ASSERT(_manualStream);
    
    if ([self isStarted])
        [_manualStream finishedLoadingWithData:[[self dataSource] data]];    
}

- (void)redeliverStream
{
    if ([self dataSource] && [self isStarted]) {
        // Deliver what has not been passed to the plug-in up to this point.
        if (_dataLengthReceived > 0) {
            NSData *data = [[[self dataSource] data] subdataWithRange:NSMakeRange(0, _dataLengthReceived)];
            _dataLengthReceived = 0;
            [self pluginView:self receivedData:data];
            if (![[self dataSource] isLoading]) {
                if (_error)
                    [self pluginView:self receivedError:_error];
                else
                    [self pluginViewFinishedLoading:self];
            }
        }
    }
}

@end
