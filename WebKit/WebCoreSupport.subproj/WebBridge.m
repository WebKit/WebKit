/*	
    WebBridge.mm
    Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebBridge.h>

#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebHistoryItem.h>
#import <WebKit/WebHTMLRepresentationPrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebLoadProgress.h>
#import <WebKit/WebLocationChangeDelegate.h>
#import <WebKit/WebSubresourceClient.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebWindowOperationsDelegate.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebError.h>
#import <WebFoundation/WebNSStringExtras.h>
#import <WebFoundation/WebResourceHandle.h>
#import <WebFoundation/WebResourceRequest.h>
#import <WebFoundation/WebHTTPResourceRequest.h>

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

- (NSArray *)childFrames
{
    NSArray *frames = [[self dataSource] children];
    NSEnumerator *e = [frames objectEnumerator];
    NSMutableArray *frameBridges = [NSMutableArray arrayWithCapacity:[frames count]];
    WebFrame *childFrame;
    while ((childFrame = [e nextObject])) {
        id frameBridge = [childFrame _bridge];
        if (frameBridge)
            [frameBridges addObject:frameBridge];
    }
    return frameBridges;
}

- (WebCoreBridge *)descendantFrameNamed:(NSString *)name
{
    ASSERT(frame != nil);
    return [[frame frameNamed:name] _bridge];
}

- (WebCoreBridge *)openNewWindowWithURL:(NSURL *)URL referrer:(NSString *)referrer frameName:(NSString *)name
{
    ASSERT(frame != nil);

    WebController *newController = [[[frame controller] windowOperationsDelegate] openNewWindowWithURL:URL referrer:referrer];
    [newController _setTopLevelFrameName:name];
    WebFrame *newFrame = [newController mainFrame];
    return [newFrame _bridge];
}

- (BOOL)areToolbarsVisible
{
    ASSERT(frame != nil);
    return [[[frame controller] windowOperationsDelegate] areToolbarsVisible];
}

- (void)setToolbarsVisible:(BOOL)visible
{
    ASSERT(frame != nil);
    [[[frame controller] windowOperationsDelegate] setToolbarsVisible:visible];
}

- (BOOL)areScrollbarsVisible
{
    ASSERT(frame != nil);
    return [[frame webView] allowsScrolling];
}

- (void)setScrollbarsVisible:(BOOL)visible
{
    ASSERT(frame != nil);
    return [[frame webView] setAllowsScrolling:visible];
}

- (BOOL)isStatusBarVisible
{
    ASSERT(frame != nil);
    return [[[frame controller] windowOperationsDelegate] isStatusBarVisible];
}

- (void)setStatusBarVisible:(BOOL)visible
{
    ASSERT(frame != nil);
    [[[frame controller] windowOperationsDelegate] setStatusBarVisible:visible];
}

- (void)setWindowFrame:(NSRect)frameRect
{
    ASSERT(frame != nil);
    [[[frame controller] windowOperationsDelegate] setFrame:frameRect];
}

- (NSWindow *)window
{
    ASSERT(frame != nil);
    return [[[frame controller] windowOperationsDelegate] window];
}

- (void)setTitle:(NSString *)title
{
    [[self dataSource] _setTitle:[title _web_stringByCollapsingNonPrintingCharacters]];
}

- (void)setStatusText:(NSString *)status
{
    ASSERT(frame != nil);
    [[[frame controller] windowOperationsDelegate] setStatusText:status];
}

- (WebCoreBridge *)mainFrame
{
    ASSERT(frame != nil);
    return [[[frame controller] mainFrame] _bridge];
}

- (WebCoreBridge *)frameNamed:(NSString *)name
{
    ASSERT(frame != nil);
    return [[[frame controller] frameNamed:name] _bridge];
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)withDataSource
{
    ASSERT([self dataSource] == withDataSource);

    if ([withDataSource _overrideEncoding]) {
	[self addData:data withOverrideEncoding:[withDataSource _overrideEncoding]];
    } else {
	[self addData:data withEncoding:[withDataSource encoding]];
    }
}

- (id <WebCoreResourceHandle>)startLoadingResource:(id <WebCoreResourceLoader>)resourceLoader withURL:(NSURL *)URL referrer:(NSString *)referrer
{
    return [WebSubresourceClient startLoadingResource:resourceLoader withURL:URL referrer:referrer forDataSource:[self dataSource]];
}

- (void)objectLoadedFromCache:(NSURL *)URL size:(unsigned)bytes
{
    ASSERT(frame != nil);

    WebResourceRequest *request = [[WebResourceRequest alloc] initWithURL:URL];
    WebResourceHandle *handle = [[WebResourceHandle alloc] initWithRequest:request];
    [handle loadWithDelegate:nil];
    WebLoadProgress *loadProgress = [[WebLoadProgress alloc] initWithBytesSoFar:bytes totalToLoad:bytes];
    [[frame controller] _receivedProgress:loadProgress forResourceHandle:handle fromDataSource:[self dataSource] complete:YES];
    [loadProgress release];
    [handle release];
    [request release];
}

- (BOOL)isReloading
{
    return ([[[self dataSource] request] requestCachePolicy] == WebRequestCachePolicyLoadFromOrigin);
}

- (void)reportClientRedirectTo:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date
{
    [[[frame controller] locationChangeDelegate] clientRedirectTo:URL delay:seconds fireDate:date forFrame:frame];
}

- (void)reportClientRedirectCancelled
{
    [[[frame controller] locationChangeDelegate] clientRedirectCancelledForFrame:frame];
}

- (void)setFrame:(WebFrame *)webFrame
{
    ASSERT(webFrame != nil);

    if (frame == nil) {
	// FIXME: non-retained because data source owns representation owns bridge
	frame = webFrame;
        [self setTextSizeMultiplier:[[frame controller] textSizeMultiplier]];
    } else {
	ASSERT(frame == webFrame);
    }
}

- (void)dataSourceChanged
{
    [self setParent:[[[self dataSource] parent] _bridge]];
    [self openURL:[[self dataSource] URL]];
}

- (WebDataSource *)dataSource
{
    ASSERT(frame != nil);
    WebDataSource *dataSource = [frame dataSource];

    ASSERT(dataSource != nil);
    ASSERT([dataSource _isCommitted]);

    return dataSource;
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

- (void)setIconURL:(NSURL *)URL
{
    [[self dataSource] _setIconURL:URL];
}

- (void)setIconURL:(NSURL *)URL withType:(NSString *)type
{
    [[self dataSource] _setIconURL:URL withType:type];
}

- (void)loadRequest:(WebResourceRequest *)request withParent:(WebDataSource *)parent
{
    WebDataSource *newDataSource = [[WebDataSource alloc] initWithRequest:request];
    [newDataSource _setParent:parent];
    if ([frame setProvisionalDataSource:newDataSource]) {
        [frame startLoading];
    }
    [newDataSource release];
}

- (void)loadURL:(NSURL *)URL referrer:(NSString *)referrer
{
    WebResourceRequest *request = [[WebResourceRequest alloc] initWithURL:URL];
    [request setReferrer:referrer];
    [self loadRequest:request withParent:[[frame dataSource] parent]];
    [request release];
}

- (void)loadURL:(NSURL *)URL referrer:(NSString *)referrer withParent:(WebDataSource *)parent
{
    WebResourceRequest *request = [[WebResourceRequest alloc] initWithURL:URL];
    [request setReferrer:referrer];
    [self loadRequest:request withParent:parent];
    [request release];
}

- (void)postWithURL:(NSURL *)URL referrer:(NSString *)referrer data:(NSData *)data
{
    // When posting, use the WebResourceHandleFlagLoadFromOrigin load flag. 
    // This prevents a potential bug which may cause a page
    // with a form that uses itself as an action to be returned 
    // from the cache without submitting.
    WebResourceRequest *request = [[WebResourceRequest alloc] initWithURL:URL];
    [request setRequestCachePolicy:WebRequestCachePolicyLoadFromOrigin];
    [request setMethod:@"POST"];
    [request setData:data];
    [request setReferrer:referrer];
    [self loadRequest:request withParent:[[frame dataSource] parent]];
    [request release];
}

- (WebCoreBridge *)createChildFrameNamed:(NSString *)frameName
    withURL:(NSURL *)URL referrer:(NSString *)referrer
    renderPart:(KHTMLRenderPart *)childRenderPart
    allowsScrolling:(BOOL)allowsScrolling marginWidth:(int)width marginHeight:(int)height
{
    ASSERT(frame != nil);
    WebFrame *newFrame = [[frame controller] createFrameNamed:frameName for:nil inParent:[self dataSource] allowsScrolling:allowsScrolling];
    if (newFrame == nil) {
        return nil;
    }
    
    [[newFrame _bridge] setRenderPart:childRenderPart];
    
    [[newFrame webView] _setMarginWidth:width];
    [[newFrame webView] _setMarginHeight:height];
    
    [[newFrame _bridge] loadURL:URL referrer:referrer withParent:[self dataSource]];
    
    // Set the load type so this load doesn't end up in the back
    // forward list.
    [newFrame _setLoadType:WebFrameLoadTypeInternal];

    return [newFrame _bridge];
}

- (void)reportBadURL:(NSString *)badURL
{
    WebError *badURLError = [[WebError alloc] initWithErrorCode:WebErrorCodeBadURLError
                                                        inDomain:WebErrorDomainWebFoundation
                                                        failingURL:badURL];
    [[frame controller] _receivedError:badURLError
                        forResourceHandle:nil
                        partialProgress:nil
                        fromDataSource:[self dataSource]];
    [badURLError release];
}

- (void)saveDocumentState: (NSArray *)documentState
{
    WebHistoryItem *item;
    
    if ([frame _loadType] == WebFrameLoadTypeBack)
        item = [[[frame controller] backForwardList] forwardEntry];
    else
        item = [[[frame controller] backForwardList] backEntry];

    [item setDocumentState: documentState];
}

- (NSArray *)documentState
{
    WebHistoryItem *currentItem;
    
    currentItem = [[[frame controller] backForwardList] currentEntry];
    
    return [currentItem documentState];
}

- (void)addBackForwardItemWithURL:(NSURL *)URL anchor:(NSString *)anchor;
{
    WebHistoryItem *backForwardItem;
    WebFrame *parentFrame = [[frame controller] frameForDataSource:[[frame dataSource] parent]]; 

    backForwardItem = [[WebHistoryItem alloc] initWithURL:URL target:[frame name] parent:[parentFrame name] title:[[frame dataSource] pageTitle]];
    [backForwardItem setAnchor:anchor];
    [[[frame controller] backForwardList] addEntry:backForwardItem];
    [backForwardItem release];
}

- (NSString *)userAgentForURL:(NSURL *)URL
{
    return [[frame controller] userAgentForURL:URL];
}

- (NSView *)nextKeyViewOutsideWebViews
{
    return [[[[frame controller] mainFrame] webView] nextKeyView];
}

- (NSView *)previousKeyViewOutsideWebViews
{
    return [[[[frame controller] mainFrame] webView] previousKeyView];
}

- (BOOL)defersLoading
{
    return [[frame controller] _defersCallbacks];
}

- (void)setDefersLoading:(BOOL)defers
{
    [[frame controller] _setDefersCallbacks:defers];
}

- (void)setNeedsReapplyStyles
{
    NSView <WebDocumentView> *view = [[frame webView] documentView];
    if ([view isKindOfClass:[WebHTMLView class]]) {
        [(WebHTMLView *)view setNeedsToApplyStyles:YES];
        [view setNeedsLayout:YES];
        [view setNeedsDisplay:YES];
    }
}

- (void)setNeedsLayout
{
    NSView <WebDocumentView> *view = [[frame webView] documentView];
    [view setNeedsLayout:YES];
    [view setNeedsDisplay:YES];
}

- (NSURL *)requestedURL
{
    return [[[self dataSource] request] URL];
}

@end
