/*	
    WebBridge.mm
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebBridge.h>

#import <WebKit/WebHTMLRepresentationPrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebSubresourceClient.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebFrameBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebLoadProgress.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebKitDebug.h>

#import <WebFoundation/WebResourceHandle.h>

@interface NSApplication (DeclarationStolenFromAppKit)
- (void)_cycleWindowsReversed:(BOOL)reversed;
@end

@implementation WebBridge

- init
{
    ++WebBridgeCount;

    return [super init];
}

- (void)dealloc
{
    --WebBridgeCount;
    
    [super dealloc];
}

- (WebCoreFrameBridge *)frame
{
    return [[[self dataSource] webFrame] _frameBridge];
}

- (WebCoreBridge *)parent
{
    WEBKIT_ASSERT(dataSource);
    return [[dataSource parent] _bridge];
}

- (NSArray *)childFrames
{
    NSArray *frames = [[self dataSource] children];
    NSEnumerator *e = [frames objectEnumerator];
    NSMutableArray *frameBridges = [NSMutableArray arrayWithCapacity:[frames count]];
    WebFrame *frame;
    while ((frame = [e nextObject])) {
        id frameBridge = [frame _frameBridge];
        if (frameBridge)
            [frameBridges addObject:frameBridge];
    }
    return frameBridges;
}

- (WebCoreFrameBridge *)descendantFrameNamed:(NSString *)name
{
    return [[[[self dataSource] webFrame] frameNamed:name] _frameBridge];
}

- (BOOL)createChildFrameNamed:(NSString *)frameName
    withURL:(NSURL *)URL renderPart:(KHTMLRenderPart *)renderPart
    allowsScrolling:(BOOL)allowsScrolling marginWidth:(int)width marginHeight:(int)height
{
    WebFrame *frame = [[[self dataSource] controller] createFrameNamed:frameName for:nil inParent:[self dataSource] allowsScrolling:allowsScrolling];
    if (frame == nil) {
        return NO;
    }
    
    [[frame _frameBridge] setRenderPart:renderPart];
    
    [[frame webView] _setMarginWidth:width];
    [[frame webView] _setMarginHeight:height];

    [[frame _frameBridge] loadURL:URL attributes:nil flags:0 withParent:[self dataSource]];
    
    return YES;
}

- (WebCoreBridge *)openNewWindowWithURL:(NSURL *)url
{
    WebController *newController = [[[[self dataSource] controller] windowContext] openNewWindowWithURL:url];
    WebDataSource *newDataSource;
    
    newDataSource = [[newController mainFrame] dataSource];
    if ([newDataSource isDocumentHTML])
        return [(WebHTMLRepresentation *)[newDataSource representation] _bridge];
        
    return nil;
}

- (BOOL)areToolbarsVisible
{
    return [[[[self dataSource] controller] windowContext] areToolbarsVisible];
}

- (void)setToolbarsVisible:(BOOL)visible
{
    [[[[self dataSource] controller] windowContext] setToolbarsVisible:visible];
}

- (BOOL)areScrollbarsVisible
{
    return [[[[self dataSource] webFrame] webView] allowsScrolling];
}

- (void)setScrollbarsVisible:(BOOL)visible
{
    return [[[[self dataSource] webFrame] webView] setAllowsScrolling:visible];
}

- (BOOL)isStatusBarVisible
{
    return [[[[self dataSource] controller] windowContext] isStatusBarVisible];
}

- (void)setStatusBarVisible:(BOOL)visible
{
    [[[[self dataSource] controller] windowContext] setStatusBarVisible:visible];
}

- (void)setWindowFrame:(NSRect)frame
{
    [[[[self dataSource] controller] windowContext] setFrame:frame];
}

- (NSWindow *)window
{
    return [[[[self dataSource] controller] windowContext] window];
}

- (void)setTitle:(NSString *)title
{
    [[self dataSource] _setTitle:title];
}

- (void)setStatusText:(NSString *)status
{
    [[[[self dataSource] controller] windowContext] setStatusText:status];
}

- (WebCoreFrameBridge *)mainFrame
{
    return [[[[self dataSource] controller] mainFrame] _frameBridge];
}

- (WebCoreFrameBridge *)frameNamed:(NSString *)name
{
    return [[[[self dataSource] controller] frameNamed:name] _frameBridge];
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)withDataSource
{
    WEBKIT_ASSERT(dataSource == withDataSource);

    [self addData:data withEncoding:[dataSource encoding]];
}

- (WebResourceHandle *)startLoadingResource:(id <WebCoreResourceLoader>)resourceLoader withURL:(NSURL *)URL
{
    return [WebSubresourceClient startLoadingResource:resourceLoader withURL:URL dataSource:[self dataSource]];
}

- (void)objectLoadedFromCache:(NSURL *)URL size:(unsigned)bytes
{
    WebResourceHandle *handle;
    WebLoadProgress *loadProgress;
    
    handle = [[WebResourceHandle alloc] initWithURL:URL];
    loadProgress = [[WebLoadProgress alloc] initWithBytesSoFar:bytes totalToLoad:bytes];
    [[[self dataSource] controller] _receivedProgress:loadProgress forResourceHandle:handle fromDataSource: [self dataSource] complete:YES];
    [loadProgress release];
    [handle release];
}

- (void)setDataSource: (WebDataSource *)ds
{
    WEBKIT_ASSERT(ds != nil);
    WEBKIT_ASSERT([ds _isCommitted]);

    if (dataSource == nil) {
	// FIXME: non-retained because data source owns representation owns bridge
	dataSource = ds;
        [self openURL:[dataSource inputURL]];
        if ([dataSource redirectedURL]) {
            [self setURL:[dataSource redirectedURL]];
        }
    } else {
        WEBKIT_ASSERT(dataSource == ds);
    }

}

- (WebDataSource *)dataSource
{
    WEBKIT_ASSERT(dataSource != nil);
    WEBKIT_ASSERT([dataSource _isCommitted]);

    return dataSource;
}


- (BOOL)openedByScript
{
    return [[[self dataSource] controller] _openedByScript];
}

- (void)setOpenedByScript:(BOOL)openedByScript
{
    [[[self dataSource] controller] _setOpenedByScript:openedByScript];
}

- (void)unfocusWindow
{
    if ([[self window] isKeyWindow] || [[[self window] attachedSheet] isKeyWindow]) {
	[NSApp _cycleWindowsReversed:FALSE];
    }
}

- (BOOL)modifierTrackingEnabled
{
    return [WebHTMLView _modifierTrackingEnabled];
}

- (void)setIconURL:(NSURL *)url
{
    [[self dataSource] _setIconURL:url];
}

- (void)setIconURL:(NSURL *)url withType:(NSString *)type
{
    [[self dataSource] _setIconURL:url withType:type];
}

@end
