/*
        WebNetscapePluginDocumentView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDataSource.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebNetscapePluginDocumentView.h>
#import <WebKit/WebNetscapePluginRepresentation.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebPluginErrorPrivate.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebViewPrivate.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/NSURLResponse.h>

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
    if ([[dataSource data] length] > 0) {
        // Plug-in started after data was received. Redeliver what was already received.
        WebNetscapePluginRepresentation *representation = (WebNetscapePluginRepresentation *)[dataSource representation];
        ASSERT([representation isKindOfClass:[WebNetscapePluginRepresentation class]]);
        [representation redeliverStream];
    }
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

    if (![thePlugin load]){
        // FIXME: It would be nice to stop the load here.
        WebPlugInError *error = [WebPlugInError pluginErrorWithCode:WebKitErrorCannotLoadPlugin
                                                         contentURL:[[[theDataSource request] URL] absoluteString]
                                                      pluginPageURL:nil
                                                         pluginName:[thePlugin name]
                                                           MIMEType:MIME];
        WebView *webView = [[theDataSource webFrame] webView];
        [[webView _resourceLoadDelegateForwarder] webView:webView
                                    plugInFailedWithError:error
                                               dataSource:theDataSource];
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
