/*	
    WebBridge.mm
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebBridge.h>

#import <WebKit/WebHTMLRepresentationPrivate.h>
#import <WebKit/WebSubresourceClient.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebFrameBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebLoadProgress.h>

#import <WebKit/WebKitDebug.h>

#import <WebFoundation/WebResourceHandle.h>

@implementation WebDataSource (WebBridge)

- (WebBridge *)_bridge
{
    id representation = [self representation];
    return [representation respondsToSelector:@selector(_bridge)] ? [representation _bridge] : nil;
}

@end

@interface NSApplication (DeclarationStolenFromAppKit)
- (void)_cycleWindowsReversed:(BOOL)reversed;
@end

@implementation WebBridge

- (WebCoreFrameBridge *)frame
{
    return [[dataSource webFrame] _frameBridge];
}

- (WebCoreBridge *)parent
{
    WEBKIT_ASSERT(dataSource);
    return [[dataSource parent] _bridge];
}

- (NSArray *)childFrames
{
    WEBKIT_ASSERT(dataSource);
    NSArray *frames = [dataSource children];
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
    WebCoreFrameBridge *provisionalBridge = [[[[dataSource webFrame] provisionalDataSource] frameNamed:name] _frameBridge];
    if (provisionalBridge) {
        return provisionalBridge;
    }
    return [[[dataSource webFrame] frameNamed:name] _frameBridge];
}

- (BOOL)createChildFrameNamed:(NSString *)frameName
    withURL:(NSURL *)URL renderPart:(KHTMLRenderPart *)renderPart
    allowsScrolling:(BOOL)allowsScrolling marginWidth:(int)width marginHeight:(int)height
{
    WEBKIT_ASSERT(dataSource);

    WebFrame *frame = [[dataSource controller] createFrameNamed:frameName for:nil inParent:dataSource allowsScrolling:allowsScrolling];
    if (frame == nil) {
        return NO;
    }
    
    [[frame _frameBridge] setRenderPart:renderPart];
    
    [[frame webView] _setMarginWidth:width];
    [[frame webView] _setMarginHeight:height];

    [[frame _frameBridge] loadURL:URL attributes:nil flags:0 withParent:dataSource];
    
    return YES;
}

- (WebCoreBridge *)openNewWindowWithURL:(NSURL *)url
{
    WebController *newController = [[[dataSource controller] windowContext] openNewWindowWithURL:url];
    WebDataSource *newDataSource;
    
    newDataSource = [[newController mainFrame] provisionalDataSource];
    if ([newDataSource isDocumentHTML])
        return [(WebHTMLRepresentation *)[newDataSource representation] _bridge];
        
    return nil;
}

- (BOOL)areToolbarsVisisble
{
    return [[[dataSource controller] windowContext] areToolbarsVisible];
}

- (void)setToolbarsVisible:(BOOL)visible
{
    [[[dataSource controller] windowContext] setToolbarsVisible:visible];
}

- (BOOL)areScrollbarsVisible
{
    return [[[dataSource webFrame] webView] allowsScrolling];
}

- (void)setScrollbarsVisible:(BOOL)visible
{
    return [[[dataSource webFrame] webView] setAllowsScrolling:visible];
}

- (BOOL)isStatusBarVisisble
{
    return [[[dataSource controller] windowContext] isStatusBarVisible];
}

- (void)setStatusBarVisible:(BOOL)visible
{
    [[[dataSource controller] windowContext] setStatusBarVisible:visible];
}

- (void)setWindowFrame:(NSRect)frame
{
    [[[dataSource controller] windowContext] setFrame:frame];
}


- (NSWindow *)window
{
    return [[[dataSource controller] windowContext] window];
}

- (void)setTitle:(NSString *)title
{
    WEBKIT_ASSERT(dataSource);
    [dataSource _setTitle:title];
}

- (void)setStatusText:(NSString *)status
{
    WEBKIT_ASSERT(dataSource);
    [[[dataSource controller] windowContext] setStatusText:status];
}

- (WebCoreFrameBridge *)mainFrame
{
    return [[[dataSource controller] mainFrame] _frameBridge];
}

- (WebCoreFrameBridge *)frameNamed:(NSString *)name
{
    return [[[dataSource controller] frameNamed:name] _frameBridge];
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)withDataSource
{
    if (dataSource == nil) {
        [self setDataSource: withDataSource];
        [self openURL:[dataSource inputURL]];
        if ([dataSource redirectedURL]) {
            [self setURL:[dataSource redirectedURL]];
        }
    } else {
        WEBKIT_ASSERT(dataSource == withDataSource);
    }
    
    [self addData:data withEncoding:[dataSource encoding]];
}

- (WebResourceHandle *)startLoadingResource:(id <WebCoreResourceLoader>)resourceLoader withURL:(NSURL *)URL
{
    return [WebSubresourceClient startLoadingResource:resourceLoader withURL:URL dataSource:dataSource];
}

- (void)objectLoadedFromCache:(NSURL *)URL size:(unsigned)bytes
{
    WebResourceHandle *handle;
    WebLoadProgress *loadProgress;
    
    handle = [[WebResourceHandle alloc] initWithURL:URL];
    loadProgress = [[WebLoadProgress alloc] initWithBytesSoFar:bytes totalToLoad:bytes];
    [[dataSource controller] _receivedProgress:loadProgress forResourceHandle:handle fromDataSource: dataSource complete:YES];
    [loadProgress release];
    [handle release];
}

- (void)setDataSource: (WebDataSource *)ds
{
    // FIXME: non-retained because data source owns representation owns bridge
    dataSource = ds;
}

- (BOOL)openedByScript
{
    return [[dataSource controller] _openedByScript];
}

- (void)setOpenedByScript:(BOOL)openedByScript
{
    [[dataSource controller] _setOpenedByScript:openedByScript];
}

- (void)unfocusWindow
{
    if ([[self window] isKeyWindow] || [[[self window] attachedSheet] isKeyWindow]) {
	[NSApp _cycleWindowsReversed:FALSE];
    }
}

@end
