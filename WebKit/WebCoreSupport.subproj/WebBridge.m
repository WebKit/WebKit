/*	
    WebBridge.m
    Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebBridge.h>

#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebBaseNetscapePluginView.h>
#import <WebKit/WebBasePluginPackage.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebHistoryItem.h>
#import <WebKit/WebHTMLRepresentationPrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebLocationChangeDelegate.h>
#import <WebKit/WebNetscapePluginEmbeddedView.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebNullPluginView.h>
#import <WebKit/WebPlugin.h>
#import <WebKit/WebPluginController.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebPluginPackage.h>
#import <WebKit/WebPluginViewFactory.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebSubresourceClient.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebWindowOperationsDelegate.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebError.h>
#import <WebFoundation/WebHTTPResourceRequest.h>
#import <WebFoundation/WebNSStringExtras.h>
#import <WebFoundation/WebNSURLExtras.h>
#import <WebFoundation/WebResourceHandle.h>
#import <WebFoundation/WebResourceResponse.h>


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
    NSArray *frames = [frame children];
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

- (WebCoreBridge *)createWindowWithURL:(NSURL *)URL frameName:(NSString *)name
{
    ASSERT(frame != nil);

    WebController *newController = [[[frame controller] windowOperationsDelegate] createWindowWithURL:URL referrer:[self referrer]];
    [newController _setTopLevelFrameName:name];
    return [[newController mainFrame] _bridge];
}

- (void)showWindow
{
    [[[frame controller] windowOperationsDelegate] showWindow];
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
    } else if ([[withDataSource response] textEncodingName]) {
	[self addData:data withEncoding:[[withDataSource response] textEncodingName]];
    } else {
        [self addData:data withEncoding:[[WebPreferences standardPreferences] defaultTextEncodingName]];
    }
}

- (id <WebCoreResourceHandle>)startLoadingResource:(id <WebCoreResourceLoader>)resourceLoader withURL:(NSURL *)URL
{
    return [WebSubresourceClient startLoadingResource:resourceLoader
                                              withURL:URL
                                             referrer:[self referrer]
                                        forDataSource:[self dataSource]];
}

- (void)objectLoadedFromCache:(NSURL *)URL response: response size:(unsigned)bytes
{
    ASSERT(frame != nil);

    WebResourceRequest *request = [[WebResourceRequest alloc] initWithURL:URL];
    id <WebResourceLoadDelegate> delegate = [[frame controller] resourceLoadDelegate];
    id identifier;
    
    // No chance for delegate to modify request, so we don't send a willSendRequest: message.
    identifier = [delegate identifierForInitialRequest: request fromDataSource: [self dataSource]];
    [delegate resource: identifier didReceiveResponse: response fromDataSource: [self dataSource]];
    [delegate resource: identifier didReceiveContentLength: bytes fromDataSource: [self dataSource]];
    [delegate resource: identifier didFinishLoadingFromDataSource: [self dataSource]];
    
    [[frame controller] _finsishedLoadingResourceFromDataSource:[self dataSource]];
    [request release];
}

- (BOOL)isReloading
{
    return ([[[self dataSource] request] requestCachePolicy] == WebRequestCachePolicyLoadFromOrigin);
}

- (void)reportClientRedirectTo:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date
{
    [[[frame controller] locationChangeDelegate] clientWillRedirectTo:URL delay:seconds fireDate:date forFrame:frame];
}

- (void)reportClientRedirectCancelled
{
    [[[frame controller] locationChangeDelegate] clientRedirectCancelledForFrame:frame];
}

- (void)setFrame:(WebFrame *)webFrame
{
    ASSERT(webFrame != nil);

    if (frame == nil) {
	// Non-retained because data source owns representation owns bridge
	frame = webFrame;
        [self setTextSizeMultiplier:[[frame controller] textSizeMultiplier]];
    } else {
	ASSERT(frame == webFrame);
    }
}

- (void)dataSourceChanged
{
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

- (void)loadRequest:(WebResourceRequest *)request
{
    WebDataSource *newDataSource = [[WebDataSource alloc] initWithRequest:request];
    if ([frame setProvisionalDataSource:newDataSource]) {
        [frame startLoading];
    }
    [newDataSource release];
}

- (void)loadURL:(NSURL *)URL
{
    // FIXME: This logic doesn't exactly match what KHTML does in openURL, so it's possible
    // this will screw up in some cases involving framesets.
    if ([[URL _web_URLByRemovingFragment] isEqual:[[self URL] _web_URLByRemovingFragment]]) {
        [self openURL:URL];

        WebHistoryItem *backForwardItem = [[WebHistoryItem alloc] initWithURL:URL
            target:[frame name] parent:[[frame parent] name] title:[[frame dataSource] pageTitle]];
        [backForwardItem setAnchor:[URL fragment]];
        [[[frame controller] backForwardList] addEntry:backForwardItem];
        [backForwardItem release];

        WebDataSource *dataSource = [frame dataSource];
        [dataSource _setURL:URL];        
        [[[frame controller] locationChangeDelegate] locationChangedWithinPageForDataSource:dataSource];
        return;
    }
    
    WebResourceRequest *request = [[WebResourceRequest alloc] initWithURL:URL];
    [request setReferrer:[self referrer]];
    [self loadRequest:request];
    [request release];
}

- (void)postWithURL:(NSURL *)URL data:(NSData *)data contentType:(NSString *)contentType
{
    // When posting, use the WebResourceHandleFlagLoadFromOrigin load flag. 
    // This prevents a potential bug which may cause a page
    // with a form that uses itself as an action to be returned 
    // from the cache without submitting.
    WebResourceRequest *request = [[WebResourceRequest alloc] initWithURL:URL];
    [request setRequestCachePolicy:WebRequestCachePolicyLoadFromOrigin];
    [request setMethod:@"POST"];
    [request setData:data];
    [request setContentType:contentType];
    [request setReferrer:[self referrer]];
    [self loadRequest:request];
    [request release];
}

- (WebCoreBridge *)createChildFrameNamed:(NSString *)frameName withURL:(NSURL *)URL
    renderPart:(KHTMLRenderPart *)childRenderPart
    allowsScrolling:(BOOL)allowsScrolling marginWidth:(int)width marginHeight:(int)height
{
    ASSERT(frame != nil);
    WebFrame *newFrame = [[frame controller] _createFrameNamed:frameName inParent:frame allowsScrolling:allowsScrolling];
    if (newFrame == nil) {
        return nil;
    }
    
    [[newFrame _bridge] setRenderPart:childRenderPart];
    
    [[newFrame webView] _setMarginWidth:width];
    [[newFrame webView] _setMarginHeight:height];
    
    WebResourceRequest *request = [[WebResourceRequest alloc] initWithURL:URL];
    [request setReferrer:[self referrer]];
    [[newFrame _bridge] loadRequest:request];
    [request release];
    
    // Set the load type so this load doesn't end up in the back/forward list.
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

- (NSView <WebPlugin> *)pluginViewWithPackage:(WebPluginPackage *)pluginPackage
                                   attributes:(NSDictionary *)attributes
                                      baseURL:(NSURL *)baseURL
{
    WebPluginController *pluginController = [frame pluginController];

    NSDictionary *arguments = [NSDictionary dictionaryWithObjectsAndKeys:
        baseURL, WebPluginBaseURLKey,
        attributes, WebPluginAttributesKey,
        pluginController, WebPluginContainerKey, nil];

    NSView<WebPlugin> *view = [[pluginPackage viewFactory] pluginViewWithArguments:arguments];
    [pluginController addPluginView:view];

    return view;
}

- (NSView *)viewForPluginWithURL:(NSURL *)URL
                      attributes:(NSArray *)attributesArray
                         baseURL:(NSURL *)baseURL
                     serviceType:(NSString *)serviceType
{
    NSString *mimeType, *extension;
    NSRange r1, r2, r3;
    WebBasePluginPackage *pluginPackage;
    uint i;

    NSMutableDictionary *attributes = [NSMutableDictionary dictionary];
    for (i = 0; i < [attributesArray count]; i++){
        NSString *attribute = [attributesArray objectAtIndex:i];
        if ([attribute rangeOfString:@"__KHTML__"].length == 0) {
            r1 = [attribute rangeOfString:@"="]; // parse out attributes and values
            r2 = [attribute rangeOfString:@"\""];
            r3.location = r2.location + 1;
            r3.length = [attribute length] - r2.location - 2; // don't include quotes
            [attributes setObject:[attribute substringWithRange:r3] forKey:[attribute substringToIndex:r1.location]];
        }
    }

    if ([serviceType length]) {
        mimeType = serviceType;
        pluginPackage = [[WebPluginDatabase installedPlugins] pluginForMIMEType:mimeType];
    } else {
        extension = [[URL path] pathExtension];
        pluginPackage = [[WebPluginDatabase installedPlugins] pluginForExtension:extension];
        mimeType = [pluginPackage MIMETypeForExtension:extension];
    }

    if (pluginPackage) {
        if([pluginPackage isKindOfClass:[WebPluginPackage class]]){
            return [self pluginViewWithPackage:(WebPluginPackage *)pluginPackage
                                    attributes:attributes
                                       baseURL:baseURL];
        }
        else if([pluginPackage isKindOfClass:[WebNetscapePluginPackage class]]){
            return [[[WebNetscapePluginEmbeddedView alloc] initWithFrame:NSMakeRect(0,0,0,0)
                                                                  plugin:(WebNetscapePluginPackage *)pluginPackage
                                                                     URL:URL
                                                                 baseURL:baseURL
                                                                    mime:mimeType
                                                               attributes:attributes] autorelease];
        }else{
            [NSException raise:NSInternalInconsistencyException
                        format:@"Plugin package class not recognized"];
            return nil;
        }
    }else{
        return [[[WebNullPluginView alloc] initWithFrame:NSMakeRect(0,0,0,0)
                                                mimeType:mimeType
                                              attributes:attributes] autorelease];
    }
}

- (NSView *)viewForJavaAppletWithFrame:(NSRect)theFrame attributes:(NSDictionary *)attributes baseURL:(NSURL *)baseURL
{
    WebBasePluginPackage *pluginPackage;

    pluginPackage = [[WebPluginDatabase installedPlugins] pluginForMIMEType:@"application/x-java-applet"];

    if (!pluginPackage) {
        return nil;
    }

    if([pluginPackage isKindOfClass:[WebPluginPackage class]]){
        return [self pluginViewWithPackage:(WebPluginPackage *)pluginPackage
                                attributes:attributes
                                   baseURL:baseURL];
    }
    else if([pluginPackage isKindOfClass:[WebNetscapePluginPackage class]]){
        return [[[WebNetscapePluginEmbeddedView alloc] initWithFrame:theFrame
                                                              plugin:(WebNetscapePluginPackage *)pluginPackage
                                                                 URL:nil
                                                             baseURL:baseURL
                                                                mime:@"application/x-java-applet"
                                                           attributes:attributes] autorelease];
    }else{
        [NSException raise:NSInternalInconsistencyException
                    format:@"Plugin package class not recognized"];
        return nil;
    }
}


- (void)didAddSubview:(NSView *)view
{
    [frame _didAddSubview:view];
}

@end
