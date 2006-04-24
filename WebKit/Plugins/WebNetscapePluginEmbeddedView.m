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

#import <WebKit/WebNetscapePluginEmbeddedView.h>

#import <WebKit/WebBaseNetscapePluginViewPrivate.h>
#import <WebKit/WebFrameBridge.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebNSURLRequestExtras.h>
#import <WebKit/WebView.h>

@implementation WebNetscapePluginEmbeddedView

- (id)initWithFrame:(NSRect)frame
             plugin:(WebNetscapePluginPackage *)thePlugin
                URL:(NSURL *)theURL
            baseURL:(NSURL *)theBaseURL
           MIMEType:(NSString *)MIME
      attributeKeys:(NSArray *)keys
    attributeValues:(NSArray *)values
{
    [super initWithFrame:frame];

    // load the plug-in if it is not already loaded
    if (![thePlugin load]) {
        [self release];
        return nil;
    }
    [self setPlugin:thePlugin];    
    
    URL = [theURL retain];
    
    [self setMIMEType:MIME];
    [self setBaseURL:theBaseURL];
    [self setAttributeKeys:keys andValues:values];
    [self setMode:NP_EMBED];

    return self;
}

- (void)dealloc
{
    [URL release];
    [super dealloc];
}

- (void)didStart
{
    // If the OBJECT/EMBED tag has no SRC, the URL is passed to us as "".
    // Check for this and don't start a load in this case.
    if (URL != nil && ![URL _web_isEmpty]) {
        NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:URL];
        [request _web_setHTTPReferrer:[[[self webFrame] _bridge] referrer]];
        [self loadRequest:request inTarget:nil withNotifyData:nil sendNotification:NO];
    }
}

- (void)setWebFrame:(WebFrame *)webFrame
{
    _webFrame = webFrame;
}

- (WebDataSource *)dataSource
{
    return [_webFrame dataSource];
}

@end
