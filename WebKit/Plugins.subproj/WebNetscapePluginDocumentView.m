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

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];

    // If viewDidMoveToWindow is called before setDataSource don't deliver the stream here because the loading process
    // of WebKit handles that for us. If viewDidMoveToWindow is called after setDataSource,
    // (this happens if plug-in content is loaded without a window), start the plug-in and redeliver the
    // stream because the view is now in a window.
    if ([self window] && dataSource && [self start]) {
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
        
        WebView *c = [[theDataSource webFrame] webView];
        [[c _resourceLoadDelegateForwarder] webView:c plugInFailedWithError:error dataSource:theDataSource];
        
        return;
    }

    [self setPlugin:thePlugin];

    if ([self window]) {
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

    [self setFrame:NSMakeRect(0, 0, superFrame.size.width, superFrame.size.height)];
    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [self setWindow];

    needsLayout = NO;
}

@end
