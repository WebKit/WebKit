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
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebLocationChangeDelegate.h>
#import <WebKit/WebNetscapePluginEmbeddedView.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebNullPluginView.h>
#import <WebKit/WebPlugin.h>
#import <WebKit/WebPluginController.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebPluginError.h>
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
    ASSERT(frame != nil);
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

- (WebCoreBridge *)mainFrame
{
    ASSERT(frame != nil);
    return [[[frame controller] mainFrame] _bridge];
}

- (WebCoreBridge *)findFramedNamed:(NSString *)name;
{
    ASSERT(frame != nil);
    return [[frame findFrameNamed:name] _bridge];
}

- (WebCoreBridge *)findOrCreateFramedNamed:(NSString *)name
{
    ASSERT(frame != nil);
    return [[frame findOrCreateFramedNamed:name] _bridge];
}

- (WebCoreBridge *)createWindowWithURL:(NSURL *)URL frameName:(NSString *)name
{
    ASSERT(frame != nil);

    WebResourceRequest *request = URL ? [WebResourceRequest requestWithURL:URL] : nil;
    WebController *newController = [[[frame controller] windowOperationsDelegate] createWindowWithRequest:request];
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
    ASSERT(response != nil);

    WebResourceRequest *request = [[WebResourceRequest alloc] initWithURL:URL];
    id <WebResourceLoadDelegate> delegate = [[frame controller] resourceLoadDelegate];
    id identifier;
    
    // No chance for delegate to modify request, so we don't send a willSendRequest: message.
    identifier = [delegate identifierForInitialRequest: request fromDataSource: [self dataSource]];
    [delegate resource: identifier didReceiveResponse: response fromDataSource: [self dataSource]];
    [delegate resource: identifier didReceiveContentLength: bytes fromDataSource: [self dataSource]];
    [delegate resource: identifier didFinishLoadingFromDataSource: [self dataSource]];
    
    [[frame controller] _finishedLoadingResourceFromDataSource:[self dataSource]];
    [request release];
}

- (BOOL)isReloading
{
    return [[[self dataSource] request] requestCachePolicy] == WebRequestCachePolicyLoadFromOrigin;
}

- (void)reportClientRedirectTo:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date
{
    LOG(Redirect, "Client redirect to: %@", URL);
    [[[frame controller] locationChangeDelegate] clientWillRedirectTo:URL delay:seconds fireDate:date forFrame:frame];
    if (seconds == 0.0) {
        // used to set loadType to internal, to prevent a redirect from going in the backforward list
        _doingClientRedirect = YES;
    }
}

- (void)reportClientRedirectCancelled
{
    [[[frame controller] locationChangeDelegate] clientRedirectCancelledForFrame:frame];
    _doingClientRedirect = NO;
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

- (void)setIconURL:(NSURL *)URL
{
    [[self dataSource] _setIconURL:URL];
}

- (void)setIconURL:(NSURL *)URL withType:(NSString *)type
{
    [[self dataSource] _setIconURL:URL withType:type];
}

- (void)loadURL:(NSURL *)URL reload:(BOOL)reload triggeringEvent:(NSEvent *)event
{
    [frame _loadURL:URL loadType:(reload ? WebFrameLoadTypeReload : WebFrameLoadTypeStandard) clientRedirect:_doingClientRedirect triggeringEvent:event];
    _doingClientRedirect = NO;
}

- (void)postWithURL:(NSURL *)URL data:(NSData *)data contentType:(NSString *)contentType
{
    [frame _postWithURL:URL data:data contentType:contentType];
}

- (NSString *)generateFrameName
{
    return [frame _generateFrameName];
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

    // We must avoid loading the document itself as a subframe, like
    // other browsers do, otherwise bugs like Radar 3083732
    if (![[[URL _web_URLByRemovingFragment] absoluteURL] isEqual:[[[frame dataSource] URL] absoluteURL]]) {
	[frame _loadURL:URL intoChild:newFrame];
    }

    return [newFrame _bridge];
}

- (void)reportBadURL:(NSString *)badURL
{
    WebError *badURLError = [[WebError alloc] initWithErrorCode:WebErrorCodeBadURLError
                                                        inDomain:WebErrorDomainWebFoundation
                                                        failingURL:badURL];
    [[frame controller] _receivedError:badURLError
                        fromDataSource:[self dataSource]];
    [badURLError release];
}

- (void)saveDocumentState: (NSArray *)documentState
{
    WebHistoryItem *item = [frame _itemForSavingDocState];
    LOG(Loading, "%@: saving form state from to 0x%x", [frame name], item);
    if (item) {
        [item setDocumentState: documentState];
        // You might think we could save the scroll state here too, but unfortunately this
        // often gets called after WebFrame::_transitionToCommitted has restored the scroll
        // position of the next document.
    }
}

- (NSArray *)documentState
{
    LOG(Loading, "%@: restoring form state from item 0x%x", [frame name], [frame _itemForRestoringDocState]);
    return [[frame _itemForRestoringDocState] documentState];
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
    WebPluginController *pluginController = [frame _pluginController];
    
    NSDictionary *arguments = [NSDictionary dictionaryWithObjectsAndKeys:
        baseURL, WebPluginBaseURLKey,
        attributes, WebPluginAttributesKey,
        pluginController, WebPluginContainerKey, nil];

    LOG(Plugins, "arguments:\n%s", [[arguments description] lossyCString]);
    
    NSView<WebPlugin> *view = [[pluginPackage viewFactory] pluginViewWithArguments:arguments];
    [pluginController addPluginView:view];

    return view;
}

- (NSView *)viewForPluginWithURL:(NSURL *)URL
                      attributes:(NSArray *)attributesArray
                         baseURL:(NSURL *)baseURL
                        MIMEType:(NSString *)MIMEType
{
    NSRange r1, r2, r3;
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

    WebBasePluginPackage *pluginPackage;
    NSView *view = nil;
    int errorCode = 0;
    
    if ([MIMEType length]) {
        pluginPackage = [[WebPluginDatabase installedPlugins] pluginForMIMEType:MIMEType];
    } else {
        NSString *extension = [[URL path] pathExtension];
        pluginPackage = [[WebPluginDatabase installedPlugins] pluginForExtension:extension];
        MIMEType = [pluginPackage MIMETypeForExtension:extension];
    }

    if (pluginPackage) {
        if([pluginPackage isKindOfClass:[WebPluginPackage class]]){
            view = [self pluginViewWithPackage:(WebPluginPackage *)pluginPackage
                                    attributes:attributes
                                       baseURL:baseURL];
        }
        else if([pluginPackage isKindOfClass:[WebNetscapePluginPackage class]]){
            view = [[[WebNetscapePluginEmbeddedView alloc] initWithFrame:NSZeroRect
                                                                  plugin:(WebNetscapePluginPackage *)pluginPackage
                                                                     URL:URL
                                                                 baseURL:baseURL
                                                                MIMEType:MIMEType
                                                              attributes:attributes] autorelease];
        }else{
            [NSException raise:NSInternalInconsistencyException
                        format:@"Plugin package class not recognized"];
        }
    }else{
        errorCode = WebErrorCannotFindPlugin;
    }

    if(!errorCode && !view){
        errorCode = WebErrorCannotLoadPlugin;
    }

    if(errorCode){
        NSString *pluginPageString = [attributes objectForKey:@"pluginspage"];
        NSURL *pluginPageURL = nil;
        if(pluginPageString){
            pluginPageURL = [NSURL _web_URLWithString:pluginPageString];
        }

        WebPluginError *error = [WebPluginError pluginErrorWithCode:errorCode
                                                                URL:URL
                                                      pluginPageURL:pluginPageURL
                                                         pluginName:[pluginPackage name]
                                                           MIMEType:MIMEType];
        
        view = [[[WebNullPluginView alloc] initWithFrame:NSZeroRect error:error] autorelease];
    }

    ASSERT(view);
    
    return view;
}

- (NSView *)viewForJavaAppletWithFrame:(NSRect)theFrame attributes:(NSDictionary *)attributes baseURL:(NSURL *)baseURL
{
    NSString *MIMEType = @"application/x-java-applet";
    WebBasePluginPackage *pluginPackage;
    NSView *view = nil;
    
    pluginPackage = [[WebPluginDatabase installedPlugins] pluginForMIMEType:MIMEType];

    if (pluginPackage) {
        if([pluginPackage isKindOfClass:[WebPluginPackage class]]){
            NSMutableDictionary *theAttributes = [NSMutableDictionary dictionary];
            [theAttributes addEntriesFromDictionary:attributes];
            [theAttributes setObject:[NSString stringWithFormat:@"%d", (int)theFrame.size.width] forKey:@"width"];
            [theAttributes setObject:[NSString stringWithFormat:@"%d", (int)theFrame.size.height] forKey:@"height"];
            
            view = [self pluginViewWithPackage:(WebPluginPackage *)pluginPackage
                                    attributes:theAttributes
                                       baseURL:baseURL];
        }
        else if([pluginPackage isKindOfClass:[WebNetscapePluginPackage class]]){
            view = [[[WebNetscapePluginEmbeddedView alloc] initWithFrame:theFrame
                                                                  plugin:(WebNetscapePluginPackage *)pluginPackage
                                                                     URL:nil
                                                                 baseURL:baseURL
                                                                MIMEType:MIMEType
                                                              attributes:attributes] autorelease];
        }else{
            [NSException raise:NSInternalInconsistencyException
                        format:@"Plugin package class not recognized"];
        }
    }

    if(!view){
        WebPluginError *error = [WebPluginError pluginErrorWithCode:WebErrorJavaUnavailable
                                                                URL:nil
                                                      pluginPageURL:nil
                                                         pluginName:[pluginPackage name]
                                                           MIMEType:MIMEType];

        view = [[[WebNullPluginView alloc] initWithFrame:theFrame error:error] autorelease];
    }

    ASSERT(view);

    return view;
}

@end
