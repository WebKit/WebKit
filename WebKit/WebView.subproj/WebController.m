/*	
    WebController.mm
    Copyright 2001, 2002 Apple, Inc. All rights reserved.
*/

#import <WebKit/WebController.h>

#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebContextMenuDelegate.h>
#import <WebKit/WebController.h>
#import <WebKit/WebControllerSets.h>
#import <WebKit/WebControllerPolicyDelegate.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultPolicyDelegate.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDynamicScrollBarsView.h>
#import <WebKit/WebException.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebHistoryItem.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebResourceProgressDelegate.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebWindowOperationsDelegate.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebResourceHandle.h>

NSString *WebElementLinkURLKey = @"WebElementLinkURL";
NSString *WebElementLinkTargetFrameKey = @"WebElementTargetFrame";
NSString *WebElementLinkLabelKey = @"WebElementLinkLabel";
NSString *WebElementImageURLKey = @"WebElementImageURL";
NSString *WebElementStringKey = @"WebElementString";
NSString *WebElementImageKey = @"WebElementImage";
NSString *WebElementImageLocationKey = @"WebElementImageLocation";
NSString *WebElementFrameKey = @"WebElementFrame";

@implementation WebController

- init
{
    return [self initWithView: nil provisionalDataSource: nil controllerSetName: nil];
}

- initWithView: (WebView *)view provisionalDataSource: (WebDataSource *)dataSource controllerSetName: (NSString *)name;
{
    [super init];
    
    _private = [[WebControllerPrivate alloc] init];
    _private->mainFrame = [[WebFrame alloc] initWithName: @"_top" webView: view provisionalDataSource: dataSource controller: self];
    _private->controllerSetName = [name retain];
    if (_private->controllerSetName != nil) {
	[WebControllerSets addController:self toSetNamed:_private->controllerSetName];
    }

    [self setUseBackForwardList: YES];
    
    ++WebControllerCount;

    return self;
}

- (void)dealloc
{
    if (_private->controllerSetName != nil) {
	[WebControllerSets removeController:self fromSetNamed:_private->controllerSetName];
    }

    --WebControllerCount;
    
    [_private release];
    [super dealloc];
}


- (void)setWindowOperationsDelegate:(id <WebWindowOperationsDelegate>)delegate
{
    _private->windowContext = delegate;
}

- (id <WebWindowOperationsDelegate>)windowOperationsDelegate
{
    return _private->windowContext;
}

- (void)setResourceProgressDelegate: (id <WebResourceLoadDelegate>)delegate
{
    _private->resourceProgressDelegate = delegate;
}


- (id<WebResourceLoadDelegate>)resourceProgressDelegate
{
    return _private->resourceProgressDelegate;
}


- (void)setDownloadProgressDelegate: (id<WebResourceLoadDelegate>)delegate
{
    _private->downloadProgressDelegate = delegate;
}


- (id<WebResourceLoadDelegate>)downloadProgressDelegate
{
    return _private->downloadProgressDelegate;
}

- (void)setContextMenuDelegate: (id<WebContextMenuDelegate>)delegate
{
    _private->contextMenuDelegate = delegate;
}

- (id<WebContextMenuDelegate>)contextMenuDelegate
{
    return _private->contextMenuDelegate;
}

- (void)setPolicyDelegate:(id <WebControllerPolicyDelegate>)delegate
{
    _private->policyDelegate = delegate;
}

- (id<WebControllerPolicyDelegate>)policyDelegate
{
    if (!_private->policyDelegate)
        _private->policyDelegate = [[WebDefaultPolicyDelegate alloc] initWithWebController: self];
    return _private->policyDelegate;
}

- (void)setLocationChangeDelegate:(id <WebLocationChangeDelegate>)delegate
{
    _private->locationChangeDelegate = delegate;
}

- (id <WebLocationChangeDelegate>)locationChangeDelegate
{
    return _private->locationChangeDelegate;
}

- (WebFrame *)_frameForDataSource: (WebDataSource *)dataSource fromFrame: (WebFrame *)frame
{
    NSArray *frames;
    int i, count;
    WebFrame *result, *aFrame;
    
    if ([frame dataSource] == dataSource)
        return frame;
        
    if ([frame provisionalDataSource] == dataSource)
        return frame;
        
    frames = [[frame dataSource] children];
    count = [frames count];
    for (i = 0; i < count; i++){
        aFrame = [frames objectAtIndex: i];
        result = [self _frameForDataSource: dataSource fromFrame: aFrame];
        if (result)
            return result;
    }

    frames = [[frame provisionalDataSource] children];
    count = [frames count];
    for (i = 0; i < count; i++){
        aFrame = [frames objectAtIndex: i];
        result = [self _frameForDataSource: dataSource fromFrame: aFrame];
        if (result)
            return result;
    }
    
    return nil;       
}


- (WebFrame *)frameForDataSource: (WebDataSource *)dataSource
{
    WebFrame *frame = [self mainFrame];
    
    return [self _frameForDataSource: dataSource fromFrame: frame];
}


- (WebFrame *)_frameForView: (WebView *)aView fromFrame: (WebFrame *)frame
{
    NSArray *frames;
    int i, count;
    WebFrame *result, *aFrame;
    
    if ([frame webView] == aView)
        return frame;
        
    frames = [[frame dataSource] children];
    count = [frames count];
    for (i = 0; i < count; i++){
        aFrame = [frames objectAtIndex: i];
        result = [self _frameForView: aView fromFrame: aFrame];
        if (result)
            return result;
    }

    frames = [[frame provisionalDataSource] children];
    count = [frames count];
    for (i = 0; i < count; i++){
        aFrame = [frames objectAtIndex: i];
        result = [self _frameForView: aView fromFrame: aFrame];
        if (result)
            return result;
    }
    
    return nil;       
}


- (WebFrame *)frameForView: (WebView *)aView
{
    WebFrame *frame = [self mainFrame];
    
    return [self _frameForView: aView fromFrame: frame];
}


- (WebFrame *)frameNamed: (NSString *)name
{
    // Try this controller first
    WebFrame *frame = [self _frameInThisWindowNamed:name];

    if (frame != nil) {
	return frame;
    }

    // Try other controllers in the same set
    if (_private->controllerSetName != nil) {
	NSEnumerator *enumerator = [WebControllerSets controllersInSetNamed:_private->controllerSetName];
	WebController *controller;
	while ((controller = [enumerator nextObject]) != nil && frame == nil) {
	    frame = [controller _frameInThisWindowNamed:name];
	}
    }

    return frame;
}

- (WebFrame *)mainFrame
{
    return _private->mainFrame;
}


+ (BOOL)canShowMIMEType:(NSString *)MIMEType
{
    if([WebView _canShowMIMEType:MIMEType] && [WebDataSource _canShowMIMEType:MIMEType]){
        return YES;
    }else{
        // Have the plug-ins register views and representations
        [WebNetscapePluginDatabase installedPlugins];
        if([WebView _canShowMIMEType:MIMEType] && [WebDataSource _canShowMIMEType:MIMEType])
            return YES;
    }
    return NO;
}

+ (BOOL)canShowFile:(NSString *)path
{    
    NSString *MIMEType;
    
    MIMEType = [[self class] _MIMETypeForFile:path];   
    return [[self class] canShowMIMEType:MIMEType];
}

- (WebBackForwardList *)backForwardList
{
    return _private->backForwardList;
}

- (void)setUseBackForwardList: (BOOL)flag
{
    _private->useBackForwardList = flag;
}

- (BOOL)useBackForwardList
{
    return _private->useBackForwardList;
}

- (void)_goToItem: (WebHistoryItem *)item withFrameLoadType: (WebFrameLoadType)type
{
    WebFrame *targetFrame;
    
    targetFrame = [self frameNamed: [item target]];
    if (targetFrame == nil){
        NSLog (@"Target frame not found, using main frame instead, will be fixed soon");
#if 0
        int pos = 1;
        WebHistoryItem *next = item;
        while (next){
            NSLog (@"frame name %@, parent %@", [next target], [next parent]);
            nextFrame = [self frameNamed: [next parent]];
            next = [[self backForwardList] backEntryAtIndex: pos++];
            if ([[next target] isEqual: @"_top"]){
                [[self mainFrame] _goToItem: next withFrameLoadType: WebFrameLoadTypeIntermediateBack];
                return;
            }
        }
#endif            
        targetFrame = [self mainFrame];
    }
    [targetFrame _goToItem: item withFrameLoadType: type];
}

- (BOOL)goBack
{
    WebHistoryItem *item = [[self backForwardList] backEntry];
    
    if (item){
        [self _goToItem: item withFrameLoadType: WebFrameLoadTypeBack];
        return YES;
    }
    return NO;
}

- (BOOL)goForward
{
    WebHistoryItem *item = [[self backForwardList] forwardEntry];
    
    if (item){
        [self _goToItem: item withFrameLoadType: WebFrameLoadTypeForward];
        return YES;
    }
    return NO;
}

- (void)setTextSizeMultiplier:(float)m
{
    if (_private->textSizeMultiplier == m) {
        return;
    }
    _private->textSizeMultiplier = m;
    [[self mainFrame] _textSizeMultiplierChanged];
}

- (float)textSizeMultiplier
{
    return _private->textSizeMultiplier;
}

- (void)setApplicationNameForUserAgent:(NSString *)applicationName
{
    NSString *name = [applicationName copy];
    [_private->userAgentLock lock];
    [_private->applicationNameForUserAgent release];
    _private->applicationNameForUserAgent = name;
    [_private->userAgentLock unlock];
}

- (NSString *)applicationNameForUserAgent
{
    return [[_private->applicationNameForUserAgent copy] autorelease];
}

- (void)setCustomUserAgent:(NSString *)userAgentString
{
    ASSERT_ARG(userAgentString, userAgentString);
    
    // FIXME: Lock can go away once WebFoundation's user agent callback is replaced with something
    // that's thread safe.
    NSString *override = [userAgentString copy];
    [_private->userAgentLock lock];
    [_private->userAgentOverride release];
    _private->userAgentOverride = override;
    [_private->userAgentLock unlock];
}

- (void)resetUserAgent
{
    // FIXME: Lock can go away once WebFoundation's user agent callback is replaced with something
    // that's thread safe.
    [_private->userAgentLock lock];
    [_private->userAgentOverride release];
    _private->userAgentOverride = nil;
    [_private->userAgentLock unlock];
}

- (BOOL)hasCustomUserAgent
{
    return _private->userAgentOverride != nil;
}

- (NSString *)customUserAgent
{
    if (_private->userAgentOverride == nil) {
        ERROR("must not ask for customUserAgent is hasCustomUserAgent is NO");
    }

    return [[_private->userAgentOverride copy] autorelease];
}

// Get the appropriate user-agent string for a particular URL.
- (NSString *)userAgentForURL:(NSURL *)URL
{
    // FIXME: Lock can go away once WebFoundation's user agent callback is replaced with something
    // that's thread safe.
    [_private->userAgentLock lock];
    NSString *result = [[_private->userAgentOverride copy] autorelease];
    [_private->userAgentLock unlock];
    if (result) {
        return result;
    }

    // Note that we currently don't look at the URL.
    // If we find that we need to spoof different user agent strings for different web pages
    // for best results, then that logic will go here.

    // FIXME: Incorporate applicationNameForUserAgent in this string so that people
    // can tell that they are talking to Alexander and not another WebKit client.
    // Maybe also incorporate something that identifies WebKit's involvement.
    
    return @"Mozilla/5.0 (Macintosh; U; PPC Mac OS X; en-US; rv:1.0.0) Gecko/20020715";
}

- (BOOL)supportsTextEncoding
{
    id documentView = [[[self mainFrame] webView] documentView];
    return [documentView conformsToProtocol:@protocol(WebDocumentTextEncoding)]
        && [documentView supportsTextEncoding];
}

- (void)setCustomTextEncodingName:(NSString *)encoding
{
    ASSERT_ARG(encoding, encoding);
    
    if ([self hasCustomTextEncoding] && [encoding isEqualToString:[self customTextEncodingName]]) {
        return;
    }

    [[self mainFrame] _reloadAllowingStaleDataWithOverrideEncoding:encoding];
}

- (void)resetTextEncoding
{
    if (![self hasCustomTextEncoding]) {
        return;
    }
    
    [[self mainFrame] _reloadAllowingStaleDataWithOverrideEncoding:nil];
}

- (NSString *)_mainFrameOverrideEncoding
{
    WebDataSource *dataSource = [[self mainFrame] provisionalDataSource];
    if (dataSource == nil) {
        dataSource = [[self mainFrame] dataSource];
    }
    if (dataSource == nil) {
        return nil;
    }
    return [dataSource _overrideEncoding];
}

- (BOOL)hasCustomTextEncoding
{
    return [self _mainFrameOverrideEncoding] != nil;
}

- (NSString *)customTextEncodingName
{
    NSString *result = [self _mainFrameOverrideEncoding];
    
    if (result == nil) {
        ERROR("must not ask for customTextEncoding is hasCustomTextEncoding is NO");
    }

    return result;
}

@end
