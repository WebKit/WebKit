/*	
    WebController.m
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
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebWindowOperationsDelegate.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebNSUserDefaultsExtras.h>
#import <WebFoundation/WebResourceHandle.h>

const struct UserAgentSpoofTableEntry *_web_findSpoofTableEntry(const char *, unsigned);

// Turn off inlining to avoid warning with newer gcc.
#undef __inline
#define __inline
#include "WebUserAgentSpoofTable.c"
#undef __inline

NSString *WebElementFrameKey = 			@"WebElementFrame";
NSString *WebElementImageKey = 			@"WebElementImage";
NSString *WebElementImageAltStringKey = 	@"WebElementImageAltString";
NSString *WebElementImageLocationKey = 		@"WebElementImageLocation";
NSString *WebElementImageURLKey = 		@"WebElementImageURL";
NSString *WebElementLinkURLKey = 		@"WebElementLinkURL";
NSString *WebElementLinkTargetFrameKey =	@"WebElementTargetFrame";
NSString *WebElementLinkLabelKey = 		@"WebElementLinkLabel";
NSString *WebElementLinkTitleKey = 		@"WebElementLinkTitle";
NSString *WebElementStringKey = 		@"WebElementString";

@implementation WebController

- init
{
    return [self initWithView: nil  controllerSetName: nil];
}

- initWithView: (WebView *)view controllerSetName: (NSString *)name;
{
    [super init];
    
    _private = [[WebControllerPrivate alloc] init];
    _private->mainFrame = [[WebFrame alloc] initWithName: @"_top" webView: view  controller: self];
    _private->controllerSetName = [name retain];
    if (_private->controllerSetName != nil) {
	[WebControllerSets addController:self toSetNamed:_private->controllerSetName];
    }

    [self setUsesBackForwardList: YES];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(_defaultsDidChange)
                                                 name:NSUserDefaultsDidChangeNotification
                                               object:[NSUserDefaults standardUserDefaults]];
    
    ++WebControllerCount;

    return self;
}

- (void)dealloc
{
    if (_private->controllerSetName != nil) {
	[WebControllerSets removeController:self fromSetNamed:_private->controllerSetName];
    }

    --WebControllerCount;
    
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    
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

- (void)setResourceLoadDelegate: (id <WebResourceLoadDelegate>)delegate
{
    _private->resourceProgressDelegate = delegate;
}


- (id<WebResourceLoadDelegate>)resourceLoadDelegate
{
    return _private->resourceProgressDelegate;
}


- (void)setDownloadDelegate: (id<WebResourceLoadDelegate>)delegate
{
    _private->downloadProgressDelegate = delegate;
}


- (id<WebResourceLoadDelegate>)downloadDelegate
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
    // FIXME: This leaks!
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
        
    frames = [frame children];
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
        
    frames = [frame children];
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
        [WebPluginDatabase installedPlugins];
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

- (void)setUsesBackForwardList: (BOOL)flag
{
    _private->useBackForwardList = flag;
}

- (BOOL)usesBackForwardList
{
    return _private->useBackForwardList;
}

- (void)_goToItem: (WebHistoryItem *)item withLoadType: (WebFrameLoadType)type
{
    WebFrame *targetFrame;

    // abort any current load if we're going back/forward
    [[self mainFrame] stopLoading];
    targetFrame = [self _findFrameNamed: [item target]];
    // We never go back/forward on a per-frame basis, so the target must be the main frame
    ASSERT(targetFrame != nil && targetFrame == [self mainFrame]);
    [targetFrame _goToItem: item withLoadType: type];
}

- (BOOL)goBack
{
    WebHistoryItem *item = [[self backForwardList] backEntry];
    
    if (item){
        [self _goToItem: item withLoadType: WebFrameLoadTypeBack];
        return YES;
    }
    return NO;
}

- (BOOL)goForward
{
    WebHistoryItem *item = [[self backForwardList] forwardEntry];
    
    if (item){
        [self _goToItem: item withLoadType: WebFrameLoadTypeForward];
        return YES;
    }
    return NO;
}

- (BOOL)goBackOrForwardToItem:(WebHistoryItem *)item
{
    [self _goToItem: item withLoadType: WebFrameLoadTypeIndexedBackForward];
    return YES;
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
    [_private->applicationNameForUserAgent release];
    _private->applicationNameForUserAgent = name;
    [_private->userAgent release];
    _private->userAgent = nil;
}

- (NSString *)applicationNameForUserAgent
{
    return [[_private->applicationNameForUserAgent retain] autorelease];
}

- (void)setCustomUserAgent:(NSString *)userAgentString
{
    ASSERT_ARG(userAgentString, userAgentString);
    
    NSString *override = [userAgentString copy];
    [_private->userAgentOverride release];
    _private->userAgentOverride = override;
}

- (void)resetUserAgent
{
    [_private->userAgentOverride release];
    _private->userAgentOverride = nil;
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

    return [[_private->userAgentOverride retain] autorelease];
}

// Get the appropriate user-agent string for a particular URL.
- (NSString *)userAgentForURL:(NSURL *)URL
{
    if (_private->userAgentOverride) {
        return [[_private->userAgentOverride retain] autorelease];
    }

    // Look to see if we need to spoof.
    // First step is to get the host as a C-format string.
    BOOL pretendToBeMacIE = NO;
    NSString *host = [URL host];
    char hostBuffer[256];
    if (host && CFStringGetCString((CFStringRef)host, hostBuffer, sizeof(hostBuffer), kCFStringEncodingASCII)) {
        // Next step is to find the last part of the name. The part with only one dot.
        const char *nextToLastPeriod = NULL;
        const char *lastPeriod = NULL;
        char c;
        char *p = hostBuffer;
        while ((c = *p)) {
            if (c == '.') {
                nextToLastPeriod = lastPeriod;
                lastPeriod = p;
            } else {
                *p = tolower(c);
            }
            ++p;
        }
        // Now look up that last part in the gperf spoof table.
        if (lastPeriod) {
            const char *domain = nextToLastPeriod ? nextToLastPeriod + 1 : hostBuffer;
            pretendToBeMacIE = _web_findSpoofTableEntry(domain, p - domain) != NULL;
        }
    }
    
    NSString **userAgentStorage = pretendToBeMacIE
        ? &_private->userAgentWhenPretendingToBeMacIE : &_private->userAgent;

    NSString *userAgent = *userAgentStorage;
    if (userAgent) {
        return [[userAgent retain] autorelease];
    }

    // FIXME: Some day we will start reporting the actual CPU here instead of hardcoding PPC.
    
    NSString *language = [NSUserDefaults _web_preferredLanguageCode];
    id sourceVersion = [[NSBundle bundleForClass:[WebController class]]
        objectForInfoDictionaryKey:(id)kCFBundleVersionKey];
    NSString *applicationName = _private->applicationNameForUserAgent;

    if (pretendToBeMacIE) {
        if ([applicationName length]) {
            userAgent = [NSString stringWithFormat:@"Mozilla/4.0 (compatible; MSIE 5.2; Mac_PowerPC) AppleWebKit/%@ %@",
                language, sourceVersion, applicationName];
        } else {
            userAgent = [NSString stringWithFormat:@"Mozilla/4.0 (compatible; MSIE 5.2; Mac_PowerPC) AppleWebKit/%@",
                language, sourceVersion];
        }
    } else {
        if ([applicationName length]) {
            userAgent = [NSString stringWithFormat:@"Mozilla/5.0 (Macintosh; U; PPC Mac OS X; %@) AppleWebKit/%@ (like Gecko) %@",
                language, sourceVersion, applicationName];
        } else {
            userAgent = [NSString stringWithFormat:@"Mozilla/5.0 (Macintosh; U; PPC Mac OS X; %@) AppleWebKit/%@ (like Gecko)",
                language, sourceVersion];
        }
    }
    
    *userAgentStorage = [userAgent retain];
    return userAgent;
}

- (BOOL)supportsTextEncoding
{
    id documentView = [[[self mainFrame] webView] documentView];
    return [documentView conformsToProtocol:@protocol(WebDocumentText)]
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

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)script
{
    return [[[self mainFrame] _bridge] stringByEvaluatingJavaScriptFromString:script];
}

@end

@implementation WebResourceLoadDelegate

- identifierForInitialRequest: (WebResourceRequest *)request fromDataSource: (WebDataSource *)dataSource
{
    return [[[NSObject alloc] init] autorelease];
}

-(WebResourceRequest *)resource:identifier willSendRequest: (WebResourceRequest *)newRequest fromDataSource:(WebDataSource *)dataSource
{
    return newRequest;
}

-(void)resource:identifier didReceiveResponse: (WebResourceResponse *)response fromDataSource:(WebDataSource *)dataSource
{
}

-(void)resource:identifier didReceiveContentLength: (unsigned)length fromDataSource:(WebDataSource *)dataSource
{
}

-(void)resource:identifier didFinishLoadingFromDataSource:(WebDataSource *)dataSource
{
}

-(void)resource:identifier didFailLoadingWithError:(WebError *)error fromDataSource:(WebDataSource *)dataSource
{
}

- (void)pluginFailedWithError:(WebPluginError *)error dataSource:(WebDataSource *)dataSource
{
}

@end
