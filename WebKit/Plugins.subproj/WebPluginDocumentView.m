/*	
    WebPluginDocumentView.m
    Copyright 2004, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebPluginDocumentView.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPluginController.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebPluginPackage.h>
#import <WebKit/WebPluginViewFactory.h>
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
    NSURLResponse *response = [dataSource response];
    
    NSString *MIMEType = [response MIMEType];    
    WebPluginPackage *plugin = (WebPluginPackage *)[[WebPluginDatabase installedPlugins] pluginForMIMEType:MIMEType];
    ASSERT([plugin isKindOfClass:[WebPluginPackage class]]);
    
    NSURL *URL = [response URL];
    NSString *URLString = [URL _web_originalDataAsString];
    NSDictionary *attributes = [[NSDictionary alloc] initWithObjectsAndKeys:MIMEType, @"type", URLString, @"src", nil];
    NSDictionary *arguments = [[NSDictionary alloc] initWithObjectsAndKeys:
        URL,                WebPlugInBaseURLKey,
        attributes,         WebPlugInAttributesKey,
        pluginController,   WebPlugInContainerKey,
        nil];
    [attributes release];
    NSView *view = [[plugin viewFactory] plugInViewWithArguments:arguments];
    [arguments release];

    ASSERT(view != nil);

    [self addSubview:view];
    [pluginController addPlugin:view];
    [view setFrame:[self frame]];
    [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource;
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
