/*	
    WebBridge.m
    Copyright (c) 2002, 2003, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebBridge.h>

#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebBaseNetscapePluginView.h>
#import <WebKit/WebBasePluginPackage.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebHistoryItemPrivate.h>
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
#import <WebKit/WebNetscapePluginDocumentView.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebSubresourceClient.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebWindowOperationsDelegate.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebError.h>
#import <WebFoundation/WebHTTPResourceRequest.h>
#import <WebFoundation/WebNSStringExtras.h>
#import <WebFoundation/WebNSDictionaryExtras.h>
#import <WebFoundation/WebNSURLExtras.h>
#import <WebFoundation/WebResourceHandle.h>
#import <WebFoundation/WebResourceResponse.h>
#import <WebFoundation/WebSystemBits.h>
#import <WebFoundation/WebFileTypeMappings.h>


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

- (WebCoreBridge *)createWindowWithURL:(NSString *)URL frameName:(NSString *)name
{
    ASSERT(frame != nil);

    WebResourceRequest *request = nil;

    if (URL != nil && [URL length] > 0) {
	request = [WebResourceRequest requestWithURL:[NSURL _web_URLWithString:URL]];
    }

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

- (WebDataSource *)dataSource
{
    ASSERT(frame != nil);
    WebDataSource *dataSource = [frame dataSource];

    ASSERT(dataSource != nil);
    ASSERT([dataSource _isCommitted]);

    return dataSource;
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

- (id <WebCoreResourceHandle>)startLoadingResource:(id <WebCoreResourceLoader>)resourceLoader withURL:(NSString *)URL
{
    // If we are no longer attached to a controller, this must be an attempted load from an
    // onUnload handler, so let's just block it.
    if ([[self dataSource] controller] == nil) {
	return nil;
    }

    return [WebSubresourceClient startLoadingResource:resourceLoader
                                              withURL:[NSURL _web_URLWithString:URL]
                                             referrer:[self referrer]
                                        forDataSource:[self dataSource]];
}

- (void)objectLoadedFromCacheWithURL:(NSString *)URL response: response size:(unsigned)bytes
{
    ASSERT(frame != nil);
    ASSERT(response != nil);

    WebResourceRequest *request = [[WebResourceRequest alloc] initWithURL:[NSURL _web_URLWithString:URL]];
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

- (void)reportClientRedirectToURL:(NSString *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date lockHistory:(BOOL)lockHistory
{
    [frame _clientRedirectedTo:[NSURL _web_URLWithString:URL] delay:seconds fireDate:date lockHistory:lockHistory];
}

- (void)reportClientRedirectCancelled
{
    [frame _clientRedirectCancelled];
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

- (void)unfocusWindow
{
    if ([[self window] isKeyWindow] || [[[self window] attachedSheet] isKeyWindow]) {
	[NSApp _cycleWindowsReversed:FALSE];
    }
}

- (void)setIconURL:(NSString *)URL
{
    [[self dataSource] _setIconURL:[NSURL _web_URLWithString:URL]];
}

- (void)setIconURL:(NSString *)URL withType:(NSString *)type
{
    [[self dataSource] _setIconURL:[NSURL _web_URLWithString:URL] withType:type];
}

- (void)loadURL:(NSString *)URL reload:(BOOL)reload triggeringEvent:(NSEvent *)event isFormSubmission:(BOOL)isFormSubmission
{
    [frame _loadURL:[NSURL _web_URLWithString:URL] loadType:(reload ? WebFrameLoadTypeReload : WebFrameLoadTypeStandard)  triggeringEvent:event isFormSubmission:isFormSubmission];
}

- (void)postWithURL:(NSString *)URL data:(NSData *)data contentType:(NSString *)contentType triggeringEvent:(NSEvent *)event
{
    [frame _postWithURL:[NSURL _web_URLWithString:URL] data:data contentType:contentType triggeringEvent:event];
}

- (NSString *)generateFrameName
{
    return [frame _generateFrameName];
}

- (WebCoreBridge *)createChildFrameNamed:(NSString *)frameName withURL:(NSString *)URL
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

    [frame _loadURL:[NSURL _web_URLWithString:URL] intoChild:newFrame];

    return [newFrame _bridge];
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

- (BOOL)saveDocumentToPageCache: documentInfo
{
    WebHistoryItem *item = [frame _itemForSavingDocState];
    if (![item hasPageCache]){
        return false;
    }
    [[item pageCache] setObject: documentInfo forKey: @"WebCorePageState"];
    return true;
}

- (NSString *)userAgentForURL:(NSString *)URL
{
    return [[frame controller] userAgentForURL:[NSURL _web_URLWithString:URL]];
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

- (NSString *)requestedURL
{
    return [[[[self dataSource] request] URL] absoluteString];
}

- (NSString *)incomingReferrer
{
    return [[[self dataSource] request] referrer];
}

- (NSView <WebPlugin> *)pluginViewWithPackage:(WebPluginPackage *)pluginPackage
                                   attributes:(NSDictionary *)attributes
                                      baseURL:(NSURL *)baseURL
{
    WebHTMLView *docView = (WebHTMLView *)[[frame webView] documentView];

    ASSERT ([docView isKindOfClass:[WebHTMLView class]]);
    
    WebPluginController *pluginController = [docView _pluginController];
    
    NSDictionary *arguments = [NSDictionary dictionaryWithObjectsAndKeys:
        baseURL, WebPluginBaseURLKey,
        attributes, WebPluginAttributesKey,
        pluginController, WebPluginContainerKey,
        nil];

    LOG(Plugins, "arguments:\n%@", arguments);
    
    return [[pluginPackage viewFactory] pluginViewWithArguments:arguments];
}

- (NSView *)viewForPluginWithURL:(NSString *)URL
                      attributes:(NSArray *)attributesArray
                         baseURL:(NSString *)baseURL
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
        NSString *extension = [URL pathExtension];
        pluginPackage = [[WebPluginDatabase installedPlugins] pluginForExtension:extension];
        MIMEType = [pluginPackage MIMETypeForExtension:extension];
    }

    if (pluginPackage) {
        if([pluginPackage isKindOfClass:[WebPluginPackage class]]){
            view = [self pluginViewWithPackage:(WebPluginPackage *)pluginPackage
                                    attributes:attributes
                                       baseURL:[NSURL _web_URLWithString:baseURL]];
        }
        else if([pluginPackage isKindOfClass:[WebNetscapePluginPackage class]]){
            view = [[[WebNetscapePluginEmbeddedView alloc] initWithFrame:NSZeroRect
                                                                  plugin:(WebNetscapePluginPackage *)pluginPackage
                                                                     URL:[NSURL _web_URLWithString:URL]
                                                                 baseURL:[NSURL _web_URLWithString:baseURL]
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
                                                         contentURL:[NSURL _web_URLWithString:URL]
                                                      pluginPageURL:pluginPageURL
                                                         pluginName:[pluginPackage name]
                                                           MIMEType:MIMEType];
        
        view = [[[WebNullPluginView alloc] initWithFrame:NSZeroRect error:error] autorelease];
    }

    ASSERT(view);
    
    return view;
}

- (NSView *)viewForJavaAppletWithFrame:(NSRect)theFrame attributes:(NSDictionary *)attributes baseURL:(NSString *)baseURL
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
                                       baseURL:[NSURL _web_URLWithString:baseURL]];
        }
        else if([pluginPackage isKindOfClass:[WebNetscapePluginPackage class]]){
            view = [[[WebNetscapePluginEmbeddedView alloc] initWithFrame:theFrame
                                                                  plugin:(WebNetscapePluginPackage *)pluginPackage
                                                                     URL:nil
                                                                 baseURL:[NSURL _web_URLWithString:baseURL]
                                                                MIMEType:MIMEType
                                                              attributes:attributes] autorelease];
        }else{
            [NSException raise:NSInternalInconsistencyException
                        format:@"Plugin package class not recognized"];
        }
    }

    if(!view){
        WebPluginError *error = [WebPluginError pluginErrorWithCode:WebErrorJavaUnavailable
                                                         contentURL:nil
                                                      pluginPageURL:nil
                                                         pluginName:[pluginPackage name]
                                                           MIMEType:MIMEType];

        view = [[[WebNullPluginView alloc] initWithFrame:theFrame error:error] autorelease];
    }

    ASSERT(view);

    return view;
}

#ifndef NDEBUG
static BOOL loggedObjectCacheSize = NO;
#endif


-(int)getObjectCacheSize
{
    vm_size_t memSize = WebSystemMainMemory();
    int cacheSize = [[WebPreferences standardPreferences] _objectCacheSize];
    int multiplier = 1;
    if (memSize > 1024 * 1024 * 1024)
        multiplier = 4;
    else if (memSize > 512 * 1024 * 1024)
        multiplier = 2;

#ifndef NDEBUG
    if (!loggedObjectCacheSize){
        LOG (CacheSizes, "Object cache size set to %d bytes.", cacheSize * multiplier);
        loggedObjectCacheSize = YES;
    }
#endif

    return cacheSize * multiplier;
}

- (BOOL)frameRequiredForMIMEType: (NSString*)mimeType
{
    // Assume a plugin is required. Don't make a frame.
    if ([mimeType length] == 0)
        return NO;
    
    Class result = [[WebView _viewTypes] _web_objectForMIMEType: mimeType];
    if (!result)
        return NO;  // Want to display a "plugin not found" dialog/image, so let a plugin get made.
        
    // If we're a supported type other than a plugin, we want to make a frame.
    // Ultimately we should just use frames for all mime types (plugins and HTML/XML/text documents),
    // but for now we're burdened with making a distinction between the two.
    return ![result isSubclassOfClass: [WebNetscapePluginDocumentView class]];
}

- (void)loadEmptyDocumentSynchronously
{
    NSURL *url = [[NSURL alloc] initWithString:@""];
    WebResourceRequest *request = [[WebResourceRequest alloc] initWithURL:url];
    [frame loadRequest:request];
    [request release];
    [url release];
}

- (NSString *)MIMETypeForPath:(NSString *)path
{
    ASSERT(path);
    NSString *extension = [path pathExtension];
    return [[WebFileTypeMappings sharedMappings] MIMETypeForExtension:extension];
}

- (void)handleMouseDragged:(NSEvent *)event
{
    WebHTMLView *docView = (WebHTMLView *)[[frame webView] documentView];

    ASSERT ([docView isKindOfClass:[WebHTMLView class]]);

    [docView _handleMouseDragged:event];
}

- (void)handleAutoscrollForMouseDragged:(NSEvent *)event;
{
    WebHTMLView *docView = (WebHTMLView *)[[frame webView] documentView];

    ASSERT ([docView isKindOfClass:[WebHTMLView class]]);

    [docView _handleAutoscrollForMouseDragged:event];
}

- (BOOL)mayStartDragWithMouseDragged:(NSEvent *)event
{
    WebHTMLView *docView = (WebHTMLView *)[[frame webView] documentView];

    ASSERT ([docView isKindOfClass:[WebHTMLView class]]);

    return [docView _mayStartDragWithMouseDragged:event];
}

- (int)historyLength
{
    return [[[frame controller] backForwardList] backListCount];
}

- (void)goBackOrForward:(int)distance
{
    WebController *controller = [frame controller];
    WebHistoryItem *item = [[controller backForwardList] entryAtIndex:distance];
    if (item) {
        [controller goBackOrForwardToItem:item];
    }
}

@end
