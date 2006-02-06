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

#import <WebKit/WebDataSource.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebKitErrorsPrivate.h>
#import <WebKit/WebNetscapePluginDocumentView.h>
#import <WebKit/WebNetscapePluginRepresentation.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebViewPrivate.h>

#import <WebKit/WebAssertions.h>
#import <Foundation/NSURLResponse.h>

@implementation WebNetscapePluginDocumentView

- initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];
    [self setFrame:NSZeroRect];
    [self setMode:NP_FULL];
    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    needsLayout = YES;
    return self;
}

- (void)dealloc
{
    [dataSource release];
    [super dealloc];
}

- (void)drawRect:(NSRect)rect
{
    if(needsLayout){
        [self layout];
    }

    [super drawRect:rect];
}

- (BOOL)canStart
{
    return (dataSource != nil);
}

- (void)didStart
{
    // Deliver what has not been passed to the plug-in up to this point.
    // Do this in case the plug-in was started after the load started.
    WebNetscapePluginRepresentation *representation = (WebNetscapePluginRepresentation *)[dataSource representation];
    ASSERT([representation isKindOfClass:[WebNetscapePluginRepresentation class]]);
    [representation redeliverStream];
}

- (WebDataSource *)dataSource
{
    return dataSource;
}

- (void)setDataSource:(WebDataSource *)theDataSource
{    
    [dataSource release];
    dataSource = [theDataSource retain];

    NSString *MIME = [[dataSource response] MIMEType];
    
    [self setMIMEType:MIME];
    [self setBaseURL:[[dataSource request] URL]];

    WebNetscapePluginPackage *thePlugin;
    thePlugin = (WebNetscapePluginPackage *)[[WebPluginDatabase installedPlugins] pluginForMIMEType:MIME];
    ASSERT([thePlugin isKindOfClass:[WebNetscapePluginPackage class]]);

    if (![thePlugin load]) {
        NSError *error = [[NSError alloc] _initWithPluginErrorCode:WebKitErrorCannotLoadPlugIn
                                                        contentURL:[[theDataSource request] URL]
                                                     pluginPageURL:nil
                                                        pluginName:[thePlugin name]
                                                          MIMEType:MIME];
        WebView *webView = [[theDataSource webFrame] webView];
        [[webView _resourceLoadDelegateForwarder] webView:webView
                                    plugInFailedWithError:error
                                               dataSource:theDataSource];
        [error release];
        return;
    }

    [self setPlugin:thePlugin];

    if ([self currentWindow]) {
        [self start];
    }
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
}

- (void)setNeedsLayout:(BOOL)flag
{
    needsLayout = flag;
}

- (void)layout
{
    NSRect superFrame = [[self _web_superviewOfClass:[WebFrameView class]] frame];
    [self setFrame:NSMakeRect(0, 0, NSWidth(superFrame), NSHeight(superFrame))];
    needsLayout = NO;
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
    [super viewWillMoveToHostWindow:hostWindow];
}

- (void)viewDidMoveToHostWindow
{
    [super viewDidMoveToHostWindow];
}

@end
