/*	
    WebBridge.m
    Copyright (c) 2002, 2003, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebBridge.h>

#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebBaseNetscapePluginView.h>
#import <WebKit/WebBasePluginPackage.h>
#import <WebKit/WebBaseResourceHandleDelegate.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultResourceLoadDelegate.h>
#import <WebKit/WebDefaultUIDelegate.h>
#import <WebKit/WebFileButton.h>
#import <WebKit/WebFormDelegate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebHistoryItemPrivate.h>
#import <WebKit/WebHTMLRepresentationPrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebJavaScriptTextInputPanel.h>
#import <WebKit/WebKitErrorsPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebNetscapePluginEmbeddedView.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebNullPluginView.h>
#import <WebKit/WebPlugin.h>
#import <WebKit/WebPluginController.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebPluginPackage.h>
#import <WebKit/WebPluginViewFactory.h>
#import <WebKit/WebNetscapePluginDocumentView.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebSubresourceClient.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebUIDelegate.h>

#import <WebFoundation/WebAssertions.h>

#import <WebFoundation/NSURLRequest.h>
#import <WebFoundation/NSURLRequestPrivate.h>
#import <WebFoundation/WebNSStringExtras.h>
#import <WebFoundation/WebNSDictionaryExtras.h>
#import <WebFoundation/WebNSURLExtras.h>
#import <WebFoundation/NSURLConnection.h>
#import <WebFoundation/NSURLResponse.h>
#import <WebFoundation/WebSystemBits.h>
#import <WebFoundation/WebFileTypeMappings.h>
#import <WebFoundation/WebLocalizableStrings.h>


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
    NSArray *frames = [frame childFrames];
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
    return [[[frame webView] mainFrame] _bridge];
}

- (WebCoreBridge *)findFrameNamed:(NSString *)name;
{
    ASSERT(frame != nil);
    return [[frame findFrameNamed:name] _bridge];
}

- (WebCoreBridge *)createWindowWithURL:(NSString *)URL frameName:(NSString *)name
{
    ASSERT(frame != nil);

    NSMutableURLRequest *request = nil;

    if (URL != nil && [URL length] > 0) {
	request = [NSMutableURLRequest requestWithURL:[NSURL _web_URLWithString:URL]];
	[request HTTPSetReferrer:[self referrer]];
    }

    WebView *currentController = [frame webView];
    id wd = [currentController UIDelegate];
    WebView *newController = nil;
    
    if ([wd respondsToSelector:@selector(webView:createWindowWithRequest:)])
        newController = [wd webView:currentController createWindowWithRequest:request];
    else
        newController = [[WebDefaultUIDelegate sharedUIDelegate] webView:currentController createWindowWithRequest:request];
    [newController _setTopLevelFrameName:name];
    return [[newController mainFrame] _bridge];
}

- (void)showWindow
{
    WebView *c = [frame webView];
    [[c _UIDelegateForwarder] webViewShowWindow: c];
}

- (BOOL)areToolbarsVisible
{
    ASSERT(frame != nil);
    WebView *c = [frame webView];
    id wd = [c UIDelegate];
    if ([wd respondsToSelector: @selector(webViewAreToolbarsVisible:)])
        return [wd webViewAreToolbarsVisible: c];
    return [[WebDefaultUIDelegate sharedUIDelegate] webViewAreToolbarsVisible:c];
}

- (void)setToolbarsVisible:(BOOL)visible
{
    ASSERT(frame != nil);
    WebView *c = [frame webView];
    [[c _UIDelegateForwarder] webView:c setToolbarsVisible:visible];
}

- (BOOL)areScrollbarsVisible
{
    ASSERT(frame != nil);
    return [[frame frameView] allowsScrolling];
}

- (void)setScrollbarsVisible:(BOOL)visible
{
    ASSERT(frame != nil);
    return [[frame frameView] setAllowsScrolling:visible];
}

- (BOOL)isStatusBarVisible
{
    ASSERT(frame != nil);
    WebView *c = [frame webView];
    id wd = [c UIDelegate];
    if ([wd respondsToSelector: @selector(webViewIsStatusBarVisible:)])
        return [wd webViewIsStatusBarVisible:c];
    return [[WebDefaultUIDelegate sharedUIDelegate] webViewIsStatusBarVisible:c];
}

- (void)setStatusBarVisible:(BOOL)visible
{
    ASSERT(frame != nil);
    WebView *c = [frame webView];
    [[c _UIDelegateForwarder] webView:c setStatusBarVisible:visible];
}

- (void)setWindowFrame:(NSRect)frameRect
{
    ASSERT(frame != nil);
    WebView *webView = [frame webView];
    [[webView _UIDelegateForwarder] webView:webView setFrame:frameRect];
}

- (NSRect)windowFrame
{
    ASSERT(frame != nil);
    WebView *webView = [frame webView];
    return [[webView _UIDelegateForwarder] webViewFrame:webView];
}

- (void)setWindowContentRect:(NSRect)contentRect
{
    ASSERT(frame != nil);
    WebView *webView = [frame webView];
    [[webView _UIDelegateForwarder] webView:webView setFrame:contentRect];
}

- (NSRect)windowContentRect
{
    ASSERT(frame != nil);
    WebView *webView = [frame webView];
    return [[webView _UIDelegateForwarder] webViewContentRect:webView];
}

- (void)setWindowIsResizable:(BOOL)resizable
{
    ASSERT(frame != nil);
    WebView *webView = [frame webView];
    [[webView _UIDelegateForwarder] webView:webView setResizable:resizable];
}

- (BOOL)windowIsResizable
{
    ASSERT(frame != nil);
    WebView *webView = [frame webView];
    return [[webView _UIDelegateForwarder] webViewIsResizable:webView];
}

- (NSResponder *)firstResponder
{
    ASSERT(frame != nil);
    WebView *webView = [frame webView];
    return [[webView _UIDelegateForwarder] webViewFirstResponderInWindow:webView];
}

- (void)makeFirstResponder:(NSResponder *)view
{
    ASSERT(frame != nil);
    WebView *webView = [frame webView];
    [[webView _UIDelegateForwarder] webView:webView makeFirstResponderInWindow:view];
}

- (void)closeWindow
{
    ASSERT(frame != nil);
    WebView *webView = [frame webView];
    [[webView _UIDelegateForwarder] webViewCloseWindow:webView];
}


- (NSWindow *)window
{
    ASSERT(frame != nil);
    return [[frame frameView] window];
}

- (void)runJavaScriptAlertPanelWithMessage:(NSString *)message
{
    WebView *c = [frame webView];
    [[c _UIDelegateForwarder] webView:c runJavaScriptAlertPanelWithMessage:message];
}

- (BOOL)runJavaScriptConfirmPanelWithMessage:(NSString *)message
{
    WebView *c = [frame webView];
    id wd = [c UIDelegate];
    if ([wd respondsToSelector: @selector(webView:runJavaScriptConfirmPanelWithMessage:)])
        return [wd webView:c runJavaScriptConfirmPanelWithMessage:message];
    return [[WebDefaultUIDelegate sharedUIDelegate] webView:c runJavaScriptConfirmPanelWithMessage:message];
}

- (BOOL)runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText returningText:(NSString **)result
{
    WebView *c = [frame webView];
    id wd = [c UIDelegate];
    if ([wd respondsToSelector: @selector(webView:runJavaScriptTextInputPanelWithPrompt:defaultText:)])
        *result = [wd webView:c runJavaScriptTextInputPanelWithPrompt:prompt defaultText:defaultText];
    else
        *result = [[WebDefaultUIDelegate sharedUIDelegate] webView:c runJavaScriptTextInputPanelWithPrompt:prompt defaultText:defaultText];
    return *result != nil;
}

- (NSView <WebCoreFileButton> *)fileButton
{
    return [[WebFileButton alloc] initWithBridge:self];
}

- (void)runOpenPanelForFileButtonWithResultListener:(id<WebOpenPanelResultListener>)resultListener
{
    WebView *c = [frame webView];
    [[c _UIDelegateForwarder] webView:c runOpenPanelForFileButtonWithResultListener:resultListener];
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
    WebView *c = [frame webView];
    [[c _UIDelegateForwarder] webView:c setStatusText:status];
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)withDataSource
{
    ASSERT([self dataSource] == withDataSource);

    // Set the encoding. This only needs to be done once, but it's harmless to do it again later.
    NSString *encoding = [withDataSource _overrideEncoding];
    BOOL userChosen = encoding != nil;
    if (encoding == nil) {
        encoding = [[withDataSource response] textEncodingName];
    }
    [self setEncoding:encoding userChosen:userChosen];

    [self addData:data];
}

- (id <WebCoreResourceHandle>)startLoadingResource:(id <WebCoreResourceLoader>)resourceLoader withURL:(NSString *)URL
{
    // If we are no longer attached to a controller, this must be an attempted load from an
    // onUnload handler, so let's just block it.
    if ([[self dataSource] _controller] == nil) {
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

    NSURLRequest *request = [[NSURLRequest alloc] initWithURL:[NSURL _web_URLWithString:URL]];
    WebView *c = [frame webView];
    id delegate = [c resourceLoadDelegate];
    id sharedDelegate = [WebDefaultResourceLoadDelegate sharedResourceLoadDelegate];
    id identifier;
    WebResourceDelegateImplementationCache implementations = [c _resourceLoadDelegateImplementations];
    
    // No chance for delegate to modify request, so we don't send a willSendRequest:redirectResponse: message.
    if (implementations.delegateImplementsIdentifierForRequest)
        identifier = [delegate webView:c identifierForInitialRequest: request fromDataSource: [self dataSource]];
    else
        identifier = [sharedDelegate webView:c identifierForInitialRequest:request fromDataSource:[self dataSource]];
    
    if (implementations.delegateImplementsDidReceiveResponse)
        [delegate webView:c resource: identifier didReceiveResponse: response fromDataSource: [self dataSource]];
    else
        [sharedDelegate webView:c resource: identifier didReceiveResponse: response fromDataSource: [self dataSource]];

    if (implementations.delegateImplementsDidReceiveContentLength)
        [delegate webView:c resource: identifier didReceiveContentLength: bytes fromDataSource: [self dataSource]];
    else
        [sharedDelegate webView:c resource: identifier didReceiveContentLength: bytes fromDataSource: [self dataSource]];

    if (implementations.delegateImplementsDidFinishLoadingFromDataSource)
        [delegate webView:c resource: identifier didFinishLoadingFromDataSource: [self dataSource]];
    else
        [sharedDelegate webView:c resource: identifier didFinishLoadingFromDataSource: [self dataSource]];
    
    [[frame webView] _finishedLoadingResourceFromDataSource:[self dataSource]];

    [request release];
}

- (BOOL)isReloading
{
    return [[[self dataSource] request] cachePolicy] == NSURLRequestReloadIgnoringCacheData;
}

- (void)reportClientRedirectToURL:(NSString *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date lockHistory:(BOOL)lockHistory
{
    [frame _clientRedirectedTo:[NSURL _web_URLWithString:URL] delay:seconds fireDate:date lockHistory:lockHistory];
}

- (void)reportClientRedirectCancelled
{
    [frame _clientRedirectCancelled];
}

- (void)setWebFrame:(WebFrame *)webFrame
{
    ASSERT(webFrame != nil);

    if (frame == nil) {
	// Non-retained because data source owns representation owns bridge
	frame = webFrame;
        [self setTextSizeMultiplier:[[frame webView] textSizeMultiplier]];
    } else {
	ASSERT(frame == webFrame);
    }
}

- (void)focusWindow
{
    [[[frame webView] _UIDelegateForwarder] webViewFocusWindow:[frame webView]];
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

- (void)loadURL:(NSString *)URL referrer:(NSString *)referrer reload:(BOOL)reload target:(NSString *)target triggeringEvent:(NSEvent *)event form:(id <WebDOMElement>)form formValues:(NSDictionary *)values
{
    if ([target length] == 0) {
	target = nil;
    }

    WebFrame *targetFrame = [frame findFrameNamed:target];

    [frame _loadURL:[NSURL _web_URLWithString:URL] referrer:referrer loadType:(reload ? WebFrameLoadTypeReload : WebFrameLoadTypeStandard) target:target triggeringEvent:event form:form formValues:values];

    if (targetFrame != nil && frame != targetFrame) {
	[[targetFrame _bridge] focusWindow];
    }
}

- (void)postWithURL:(NSString *)URL referrer:(NSString *)referrer target:(NSString *)target data:(NSData *)data contentType:(NSString *)contentType triggeringEvent:(NSEvent *)event form:(id <WebDOMElement>)form formValues:(NSDictionary *)values
{
    if ([target length] == 0) {
	target = nil;
    }

    WebFrame *targetFrame = [frame findFrameNamed:target];

    [frame _postWithURL:[NSURL _web_URLWithString:URL] referrer:(NSString *)referrer target:target data:data contentType:contentType triggeringEvent:event form:form formValues:values];

    if (targetFrame != nil && frame != targetFrame) {
	[[targetFrame _bridge] focusWindow];
    }
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
    WebFrame *newFrame = [[frame webView] _createFrameNamed:frameName inParent:frame allowsScrolling:allowsScrolling];
    if (newFrame == nil) {
        return nil;
    }
    
    [[newFrame _bridge] setRenderPart:childRenderPart];

    [[newFrame frameView] _setMarginWidth:width];
    [[newFrame frameView] _setMarginHeight:height];

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
    return [[frame webView] userAgentForURL:[NSURL _web_URLWithString:URL]];
}

- (NSView *)nextKeyViewOutsideWebFrameViews
{
    WebView *webView = [frame webView];
    NSView *nextKeyView = [webView nextKeyView];
    if (nextKeyView) {
        return nextKeyView;
    }
    // Old way, here so we don't break early WebKit adopters, but could be removed later.
    return [[[webView mainFrame] frameView] nextKeyView];
}

- (NSView *)previousKeyViewOutsideWebFrameViews
{
    WebView *webView = [frame webView];
    NSView *previousKeyView = [webView previousKeyView];
    if (previousKeyView) {
        return previousKeyView;
    }
    // Old way, here so we don't break early WebKit adopters, but could be removed later.
    return [[[webView mainFrame] frameView] previousKeyView];
}

- (BOOL)defersLoading
{
    return [[frame webView] defersCallbacks];
}

- (void)setDefersLoading:(BOOL)defers
{
    [[frame webView] setDefersCallbacks:defers];
}

- (void)setNeedsReapplyStyles
{
    NSView <WebDocumentView> *view = [[frame frameView] documentView];
    if ([view isKindOfClass:[WebHTMLView class]]) {
        [(WebHTMLView *)view setNeedsToApplyStyles:YES];
        [view setNeedsLayout:YES];
        [view setNeedsDisplay:YES];
    }
}

- (void)setNeedsLayout
{
    NSView <WebDocumentView> *view = [[frame frameView] documentView];
    [view setNeedsLayout:YES];
    [view setNeedsDisplay:YES];
}

- (NSString *)requestedURL
{
    return [[[[self dataSource] request] URL] absoluteString];
}

- (NSString *)incomingReferrer
{
    return [[[self dataSource] request] HTTPReferrer];
}

- (NSView <WebPlugin> *)pluginViewWithPackage:(WebPluginPackage *)pluginPackage
                                   attributes:(NSDictionary *)attributes
                                      baseURL:(NSURL *)baseURL
{
    WebHTMLView *docView = (WebHTMLView *)[[frame frameView] documentView];

    ASSERT ([docView isKindOfClass:[WebHTMLView class]]);
    
    WebPluginController *pluginController = [docView _pluginController];
    
    NSDictionary *arguments = [NSDictionary dictionaryWithObjectsAndKeys:
        baseURL, WebPluginBaseURLKey,
        attributes, WebPluginAttributesKey,
        pluginController, WebPluginContainerKey,
        nil];

    LOG(Plugins, "arguments:\n%@", arguments);

    [pluginPackage load];
    
    return [[pluginPackage viewFactory] pluginViewWithArguments:arguments];
}

- (NSView *)viewForPluginWithURLString:(NSString *)URLString
                            attributes:(NSArray *)attributesArray
                         baseURLString:(NSString *)baseURLString
                              MIMEType:(NSString *)MIMEType
{
    NSRange r1, r2, r3;
    uint i;

    // Parse the attributesArray into key/value pairs.
    // Store them in a dictionary so they can be passed to WebPlugins.
    // Store them in ordered arrays so they can be passed to Netscape plug-ins.
    NSMutableDictionary *attributes = [[NSMutableDictionary alloc] init];
    NSMutableArray *attributeKeys = [[NSMutableArray alloc] init];
    NSMutableArray *attributeValues = [[NSMutableArray alloc] init];
    for (i = 0; i < [attributesArray count]; i++) {
        NSString *attribute = [attributesArray objectAtIndex:i];
        if ([attribute rangeOfString:@"__KHTML__"].length == 0) {
            r1 = [attribute rangeOfString:@"="];
            r2 = [attribute rangeOfString:@"\""];
            r3.location = r2.location + 1;
            r3.length = [attribute length] - r2.location - 2; // don't include quotes
            NSString *key = [attribute substringToIndex:r1.location];
            NSString *value = [attribute substringWithRange:r3];
            [attributes setObject:value forKey:key];
            [attributeKeys addObject:key];
            [attributeValues addObject:value];
        }
    }

    NSURL *URL = URLString ? [NSURL _web_URLWithString:URLString] : nil;
    NSURL *baseURL = baseURLString ? [NSURL _web_URLWithString:baseURLString] : nil;
    WebBasePluginPackage *pluginPackage = nil;
    NSView *view = nil;
    int errorCode = 0;
    
    if ([MIMEType length] != 0) {
        pluginPackage = [[WebPluginDatabase installedPlugins] pluginForMIMEType:MIMEType];
    }
    
    NSString *extension = [[URL path] pathExtension];
    if (!pluginPackage && [extension length] != 0) {
        pluginPackage = [[WebPluginDatabase installedPlugins] pluginForExtension:extension];
        if (pluginPackage) {
            MIMEType = [pluginPackage MIMETypeForExtension:extension];
            ASSERT(MIMEType);
        }
    }

    if (pluginPackage) {
        if ([pluginPackage isKindOfClass:[WebPluginPackage class]]) {
            view = [self pluginViewWithPackage:(WebPluginPackage *)pluginPackage
                                    attributes:attributes
                                       baseURL:baseURL];
        } else if ([pluginPackage isKindOfClass:[WebNetscapePluginPackage class]]) {
            view = [[[WebNetscapePluginEmbeddedView alloc] initWithFrame:NSZeroRect
                                                                  plugin:(WebNetscapePluginPackage *)pluginPackage
                                                                     URL:URL
                                                                 baseURL:baseURL
                                                                MIMEType:MIMEType
                                                           attributeKeys:attributeKeys
                                                         attributeValues:attributeValues] autorelease];
        } else {
            ASSERT_NOT_REACHED();
        }
    } else {
        errorCode = WebKitErrorCannotFindPlugin;
    }

    if (!errorCode && !view) {
        errorCode = WebKitErrorCannotLoadPlugin;
    }

    if (errorCode) {
        NSError *error = [[NSError alloc] _initWithPluginErrorCode:errorCode
                                                  contentURLString:URLString
                                               pluginPageURLString:[attributes objectForKey:@"pluginspage"]
                                                        pluginName:[pluginPackage name]
                                                          MIMEType:MIMEType];
        view = [[[WebNullPluginView alloc] initWithFrame:NSZeroRect error:error] autorelease];
        [error release];
    }

    ASSERT(view);

    [attributes release];
    [attributeKeys release];
    [attributeValues release];
    
    return view;
}

- (NSView *)viewForJavaAppletWithFrame:(NSRect)theFrame
                            attributes:(NSDictionary *)attributes
                         baseURLString:(NSString *)baseURLString
{
    NSURL *baseURL = baseURLString ? [NSURL _web_URLWithString:baseURLString] : nil;
    NSString *MIMEType = @"application/x-java-applet";
    WebBasePluginPackage *pluginPackage;
    NSView *view = nil;
    
    pluginPackage = [[WebPluginDatabase installedPlugins] pluginForMIMEType:MIMEType];

    if (pluginPackage) {
        if ([pluginPackage isKindOfClass:[WebPluginPackage class]]) {
            NSMutableDictionary *theAttributes = [NSMutableDictionary dictionary];
            [theAttributes addEntriesFromDictionary:attributes];
            [theAttributes setObject:[NSString stringWithFormat:@"%d", (int)theFrame.size.width] forKey:@"width"];
            [theAttributes setObject:[NSString stringWithFormat:@"%d", (int)theFrame.size.height] forKey:@"height"];
            
            view = [self pluginViewWithPackage:(WebPluginPackage *)pluginPackage
                                    attributes:theAttributes
                                       baseURL:baseURL];
        } else if ([pluginPackage isKindOfClass:[WebNetscapePluginPackage class]]) {
            // Convert the attributes dictionary to 2 string arrays because this is what Netscape plug-ins expect.
            NSMutableArray *attributeKeys = [[NSMutableArray alloc] init];
            NSMutableArray *attributeValues = [[NSMutableArray alloc] init];
            NSEnumerator *enumerator = [attributes keyEnumerator];
            NSString *key;
            
            while ((key = [enumerator nextObject]) != nil) {
                [attributeKeys addObject:key];
                [attributeValues addObject:[attributes objectForKey:key]];
            }
            view = [[[WebNetscapePluginEmbeddedView alloc] initWithFrame:theFrame
                                                                  plugin:(WebNetscapePluginPackage *)pluginPackage
                                                                     URL:nil
                                                                 baseURL:baseURL
                                                                MIMEType:MIMEType
                                                           attributeKeys:attributeKeys
                                                         attributeValues:attributeValues] autorelease];
            [attributeKeys release];
            [attributeValues release];
        } else {
            ASSERT_NOT_REACHED();
        }
    }

    if (!view) {
        NSError *error = [[NSError alloc] _initWithPluginErrorCode:WebKitErrorJavaUnavailable
                                                  contentURLString:nil
                                               pluginPageURLString:nil
                                                        pluginName:[pluginPackage name]
                                                          MIMEType:MIMEType];
        view = [[[WebNullPluginView alloc] initWithFrame:theFrame error:error] autorelease];
        [error release];
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

- (BOOL)frameRequiredForMIMEType:(NSString*)mimeType
{
    // Assume a plugin is required. Don't make a frame.
    if ([mimeType length] == 0)
        return NO;
    
    Class result = [WebFrameView _viewClassForMIMEType:mimeType];
    if (!result)
        return NO;  // Want to display a "plugin not found" dialog/image, so let a plugin get made.
        
    // If we're a supported type other than a plugin, we want to make a frame.
    // Ultimately we should just use frames for all mime types (plugins and HTML/XML/text documents),
    // but for now we're burdened with making a distinction between the two.
    return !([result isSubclassOfClass:[WebNetscapePluginDocumentView class]] ||
            [result conformsToProtocol:@protocol(WebPlugin)]);
}

- (void)loadEmptyDocumentSynchronously
{
    NSURL *url = [[NSURL alloc] initWithString:@""];
    NSURLRequest *request = [[NSURLRequest alloc] initWithURL:url];
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
    WebHTMLView *docView = (WebHTMLView *)[[frame frameView] documentView];

    ASSERT ([docView isKindOfClass:[WebHTMLView class]]);

    [docView _handleMouseDragged:event];
}

- (void)handleAutoscrollForMouseDragged:(NSEvent *)event;
{
    WebHTMLView *docView = (WebHTMLView *)[[frame frameView] documentView];

    ASSERT ([docView isKindOfClass:[WebHTMLView class]]);

    [docView _handleAutoscrollForMouseDragged:event];
}

- (BOOL)mayStartDragWithMouseDragged:(NSEvent *)event
{
    WebHTMLView *docView = (WebHTMLView *)[[frame frameView] documentView];

    ASSERT ([docView isKindOfClass:[WebHTMLView class]]);

    return [docView _mayStartDragWithMouseDragged:event];
}

- (int)historyLength
{
    return [[[frame webView] backForwardList] backListCount];
}

- (void)goBackOrForward:(int)distance
{
    if (distance == 0) {
        return;
    }
    WebView *controller = [frame webView];
    WebBackForwardList *list = [controller backForwardList];
    WebHistoryItem *item = [list itemAtIndex:distance];
    if (!item) {
        if (distance > 0) {
            int forwardListCount = [list forwardListCount];
            if (forwardListCount > 0) {
                item = [list itemAtIndex:forwardListCount];
            }
        } else {
            int backListCount = [list forwardListCount];
            if (backListCount > 0) {
                item = [list itemAtIndex:-backListCount];
            }
        }
    }
    if (item) {
        [controller goBackOrForwardToItem:item];
    }
}

static id <WebFormDelegate> formDelegate(WebBridge *self)
{
    ASSERT(self->frame != nil);
    return [[self->frame webView] _formDelegate];
}

#define FormDelegateLog(ctrl)  LOG(FormDelegate, "control=%@", ctrl)

- (void)controlTextDidBeginEditing:(NSNotification *)obj
{
    FormDelegateLog([obj object]);
    [formDelegate(self) controlTextDidBeginEditing:obj inFrame:frame];
}

- (void)controlTextDidEndEditing:(NSNotification *)obj
{
    FormDelegateLog([obj object]);
    [formDelegate(self) controlTextDidEndEditing:obj inFrame:frame];
}

- (void)controlTextDidChange:(NSNotification *)obj
{
    FormDelegateLog([obj object]);
    [formDelegate(self) controlTextDidChange:obj inFrame:frame];
}

- (BOOL)control:(NSControl *)control textShouldBeginEditing:(NSText *)fieldEditor
{
    FormDelegateLog(control);
    return [formDelegate(self) control:control textShouldBeginEditing:fieldEditor inFrame:frame];
}

- (BOOL)control:(NSControl *)control textShouldEndEditing:(NSText *)fieldEditor
{
    FormDelegateLog(control);
    return [formDelegate(self) control:control textShouldEndEditing:fieldEditor inFrame:frame];
}

- (BOOL)control:(NSControl *)control didFailToFormatString:(NSString *)string errorDescription:(NSString *)error
{
    FormDelegateLog(control);
    return [formDelegate(self) control:control didFailToFormatString:string errorDescription:error inFrame:frame];
}

- (void)control:(NSControl *)control didFailToValidatePartialString:(NSString *)string errorDescription:(NSString *)error
{
    FormDelegateLog(control);
    [formDelegate(self) control:control didFailToValidatePartialString:string errorDescription:error inFrame:frame];
}

- (BOOL)control:(NSControl *)control isValidObject:(id)obj
{
    FormDelegateLog(control);
    return [formDelegate(self) control:control isValidObject:obj inFrame:frame];
}

- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)commandSelector
{
    FormDelegateLog(control);
    return [formDelegate(self) control:control textView:textView doCommandBySelector:commandSelector inFrame:frame];
}

- (void)frameDetached
{
    [frame stopLoading];
    [[frame parentFrame] _removeChild:frame];
}

@end
