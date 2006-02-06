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

#import <WebKit/WebPluginDocumentView.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebKitErrorsPrivate.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPluginController.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebPluginPackage.h>
#import <WebKit/WebPluginViewFactoryPrivate.h>
#import <WebKit/WebView.h>

@implementation WebPluginDocumentView

- initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];
    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    needsLayout = YES;
    pluginController = [[WebPluginController alloc] initWithDocumentView:self];
    return self;
}

- (void)dealloc
{
    [plugin release];
    [pluginController destroyAllPlugins];
    [pluginController release];
    [super dealloc];
}

- (void)drawRect:(NSRect)rect
{
    if (needsLayout) {
        [self layout];
    }
    [super drawRect:rect];
}

- (void)setDataSource:(WebDataSource *)dataSource
{
    // Since this class is a WebDocumentView and WebDocumentRepresentation, setDataSource: will be called twice. Do work only once.
    if (dataSourceHasBeenSet)
        return;
    
    // As noted above, -setDataSource: will be called twice -- once for the WebDocumentRepresentation, once for the WebDocumentView.
    // We don't want to make the plugin until we know we're being committed as the WebDocumentView for the WebFrame.  This check is
    // to ensure that we've been added to the view hierarchy before attempting to create the plugin, as some plugins currently work
    // under this assumption.
    if (![self superview])
        return;
        
    dataSourceHasBeenSet = YES;
    
    NSURLResponse *response = [dataSource response];
    NSString *MIMEType = [response MIMEType];    
    plugin = (WebPluginPackage *)[[[WebPluginDatabase installedPlugins] pluginForMIMEType:MIMEType] retain];
    ASSERT([plugin isKindOfClass:[WebPluginPackage class]]);
    
    NSURL *URL = [response URL];
    NSString *URLString = [URL _web_originalDataAsString];
    NSDictionary *attributes = [[NSDictionary alloc] initWithObjectsAndKeys:MIMEType, @"type", URLString, @"src", nil];
    NSDictionary *arguments = [[NSDictionary alloc] initWithObjectsAndKeys:
        URL,                WebPlugInBaseURLKey,
        attributes,         WebPlugInAttributesKey,
        pluginController,   WebPlugInContainerKey,
        [NSNumber numberWithInt:WebPlugInModeFull], WebPlugInModeKey,
        nil];
    [attributes release];
    NSView *view = [WebPluginController plugInViewWithArguments:arguments fromPluginPackage:plugin];
    [arguments release];

    ASSERT(view != nil);

    [self addSubview:view];
    [pluginController addPlugin:view];
    [view setFrame:[self frame]];
    [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource;
{
    // Cancel the load since WebKit plug-ins do their own loading.
    NSURLResponse *response = [dataSource response];
    // FIXME: See <rdar://problem/4258008>
    NSError *error = [[NSError alloc] _initWithPluginErrorCode:WebKitErrorPlugInWillHandleLoad
                                                    contentURL:[response URL]
                                                 pluginPageURL:nil
                                                    pluginName:[plugin name]
                                                      MIMEType:[response MIMEType]];
    [dataSource _stopLoadingWithError:error];
    [error release];    
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

- (NSWindow *)currentWindow
{
    NSWindow *window = [self window];
    return window != nil ? window : [[self _webView] hostWindow];
}

- (void)viewWillMoveToWindow:(NSWindow *)window
{
    if ([[self _webView] hostWindow] == nil) {
        [pluginController stopAllPlugins];
    }
}

- (void)viewDidMoveToWindow
{
    if ([self currentWindow] != nil) {
        [pluginController startAllPlugins];
    }
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
    if ([self window] == nil) {
        [pluginController stopAllPlugins];
    }
}

- (void)viewDidMoveToHostWindow
{
    if ([self currentWindow] != nil) {
        [pluginController startAllPlugins];
    }    
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource
{
    
}

- (void)receivedError:(NSError *)error withDataSource:(WebDataSource *)dataSource
{
    
}

- (void)finishedLoadingWithDataSource:(WebDataSource *)dataSource
{
    
}

- (BOOL)canProvideDocumentSource
{
    return NO;
}

- (NSString *)documentSource
{
    return nil;
}

- (NSString *)title
{
    return nil;
}

@end
