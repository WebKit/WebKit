/*	
    WebView.m
    Copyright 2001, 2002 Apple, Inc. All rights reserved.
*/
#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebContextMenuDelegate.h>
#import <WebKit/WebControllerSets.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultPolicyDelegate.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDynamicScrollBarsView.h>
#import <WebKit/WebException.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebHistoryItem.h>
#import <WebKit/WebHistoryItemPrivate.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebPolicyDelegate.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebTextView.h>
#import <WebKit/WebTextRepresentation.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebWindowOperationsDelegate.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebNSUserDefaultsExtras.h>
#import <WebFoundation/NSURLConnection.h>

static const struct UserAgentSpoofTableEntry *_web_findSpoofTableEntry(const char *, unsigned);

// Turn off inlining to avoid warning with newer gcc.
#undef __inline
#define __inline
#include "WebUserAgentSpoofTable.c"
#undef __inline

NSString *WebElementFrameKey = 			@"WebElementFrame";
NSString *WebElementImageKey = 			@"WebElementImage";
NSString *WebElementImageAltStringKey = 	@"WebElementImageAltString";
NSString *WebElementImageRectKey = 		@"WebElementImageRect";
NSString *WebElementImageURLKey = 		@"WebElementImageURL";
NSString *WebElementIsSelectedTextKey = 	@"WebElementIsSelectedText";
NSString *WebElementLinkURLKey = 		@"WebElementLinkURL";
NSString *WebElementLinkTargetFrameKey =	@"WebElementTargetFrame";
NSString *WebElementLinkLabelKey = 		@"WebElementLinkLabel";
NSString *WebElementLinkTitleKey = 		@"WebElementLinkTitle";




@implementation WebView

+ (BOOL)canShowMIMEType:(NSString *)MIMEType
{
    Class viewClass = [WebFrameView _viewClassForMIMEType:MIMEType];
    Class repClass = [WebDataSource _representationClassForMIMEType:MIMEType];

    if (!viewClass || !repClass) {
	[[WebPluginDatabase installedPlugins] loadPluginIfNeededForMIMEType: MIMEType];
        viewClass = [WebFrameView _viewClassForMIMEType:MIMEType];
        repClass = [WebDataSource _representationClassForMIMEType:MIMEType];
    }
    
    // Special-case WebTextView for text types that shouldn't be shown.
    if (viewClass && repClass) {
        if (viewClass == [WebTextView class] &&
            repClass == [WebTextRepresentation class] &&
            [[WebTextView unsupportedTextMIMETypes] containsObject:MIMEType]) {
            return NO;
        }
        return YES;
    }
    
    return NO;
}

- (void)_commonInitialization: (WebFrameView *)wv frameName:(NSString *)frameName groupName:(NSString *)groupName
{
    _private = [[WebViewPrivate alloc] init];
    _private->mainFrame = [[WebFrame alloc] initWithName: frameName webFrameView: wv  webView: self];
    _private->controllerSetName = [groupName copy];
    if (_private->controllerSetName != nil) {
        [WebControllerSets addController:self toSetNamed:_private->controllerSetName];
    }

    [self setMaintainsBackForwardList: YES];

    ++WebControllerCount;

    [self _updateWebCoreSettingsFromPreferences: [WebPreferences standardPreferences]];

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_preferencesChangedNotification:)
                                                 name:WebPreferencesChangedNotification object:[self preferences]];
}

- init
{
    return [self initWithFrame: NSZeroRect frameName: nil groupName: nil];
}

- initWithFrame: (NSRect)f
{
    [self initWithFrame: f frameName:nil groupName:nil];
    return self;
}

- initWithFrame: (NSRect)f frameName: (NSString *)frameName groupName: (NSString *)groupName;
{
    [super initWithFrame: f];
    WebFrameView *wv = [[WebFrameView alloc] initWithFrame: NSMakeRect(0,0,f.size.width,f.size.height)];
    [wv setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    [self addSubview: wv];
    [self _commonInitialization: wv frameName:frameName groupName:groupName];
    [wv release];
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

- (void)setPreferences: (WebPreferences *)prefs
{
    if (_private->preferences != prefs){
        [[NSNotificationCenter defaultCenter] removeObserver: self name: WebPreferencesChangedNotification object: [self preferences]];
        [_private->preferences release];
        _private->preferences = [prefs retain];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_preferencesChangedNotification:)
                                                    name:WebPreferencesChangedNotification object:[self preferences]];
    }
}

- (WebPreferences *)preferences
{
    return _private->preferences ? _private->preferences : [WebPreferences standardPreferences];
}

- (void)setWindowOperationsDelegate:delegate
{
    _private->windowContext = delegate;
    [_private->windowOperationsDelegateForwarder release];
    _private->windowOperationsDelegateForwarder = nil;
}

- windowOperationsDelegate
{
    return _private->windowContext;
}

- (void)setResourceLoadDelegate: delegate
{
    _private->resourceProgressDelegate = delegate;
    [_private->resourceProgressDelegateForwarder release];
    _private->resourceProgressDelegateForwarder = nil;
    [self _cacheResourceLoadDelegateImplementations];
}


- resourceLoadDelegate
{
    return _private->resourceProgressDelegate;
}


- (void)setDownloadDelegate: delegate
{
    _private->downloadDelegate = delegate;
}


- downloadDelegate
{
    return _private->downloadDelegate;
}

- (void)setContextMenuDelegate: delegate
{
    _private->contextMenuDelegate = delegate;
    [_private->contextMenuDelegateForwarder release];
    _private->contextMenuDelegateForwarder = nil;
}

- contextMenuDelegate
{
    return _private->contextMenuDelegate;
}

- (void)setPolicyDelegate:delegate
{
    _private->policyDelegate = delegate;
    [_private->policyDelegateForwarder release];
    _private->policyDelegateForwarder = nil;
}

- policyDelegate
{
    return _private->policyDelegate;
}

- (void)setLocationChangeDelegate:delegate
{
    _private->locationChangeDelegate = delegate;
    [_private->locationChangeDelegateForwarder release];
    _private->locationChangeDelegateForwarder = nil;
}

- locationChangeDelegate
{
    return _private->locationChangeDelegate;
}

- (WebFrame *)mainFrame
{
    return _private->mainFrame;
}

- (WebBackForwardList *)backForwardList
{
    if (_private->useBackForwardList)
        return _private->backForwardList;
    return nil;
}

- (void)setMaintainsBackForwardList: (BOOL)flag
{
    _private->useBackForwardList = flag;
}

- (void)_goToItem: (WebHistoryItem *)item withLoadType: (WebFrameLoadType)type
{
    // We never go back/forward on a per-frame basis, so the target must be the main frame
    ASSERT([item target] == nil || [self _findFrameNamed:[item target]] == [self mainFrame]);

    // abort any current load if we're going back/forward
    [[self mainFrame] stopLoading];
    [[self mainFrame] _goToItem: item withLoadType: type];
}

- (BOOL)goBack
{
    WebHistoryItem *item = [[self backForwardList] backItem];
    
    if (item){
        [self _goToItem: item withLoadType: WebFrameLoadTypeBack];
        return YES;
    }
    return NO;
}

- (BOOL)goForward
{
    WebHistoryItem *item = [[self backForwardList] forwardItem];
    
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
    int i;
    for (i = 0; i != NumUserAgentStringTypes; ++i) {
        [_private->userAgent[i] release];
        _private->userAgent[i] = nil;
    }
}

- (NSString *)applicationNameForUserAgent
{
    return [[_private->applicationNameForUserAgent retain] autorelease];
}

- (void)setCustomUserAgent:(NSString *)userAgentString
{
    NSString *override = [userAgentString copy];
    [_private->userAgentOverride release];
    _private->userAgentOverride = override;
}

- (NSString *)customUserAgent
{
    return [[_private->userAgentOverride retain] autorelease];
}

- (BOOL)supportsTextEncoding
{
    id documentView = [[[self mainFrame] frameView] documentView];
    return [documentView conformsToProtocol:@protocol(WebDocumentText)]
        && [documentView supportsTextEncoding];
}

- (void)setCustomTextEncodingName:(NSString *)encoding
{
    NSString *oldEncoding = [self customTextEncodingName];
    if (encoding == oldEncoding || [encoding isEqualToString:oldEncoding]) {
        return;
    }
    [[self mainFrame] _reloadAllowingStaleDataWithOverrideEncoding:encoding];
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

- (NSString *)customTextEncodingName
{
    return [self _mainFrameOverrideEncoding];
}

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)script
{
    return [[[self mainFrame] _bridge] stringByEvaluatingJavaScriptFromString:script];
}

// Get the appropriate user-agent string for a particular URL.
- (NSString *)userAgentForURL:(NSURL *)URL
{
    if (_private->userAgentOverride) {
        return [[_private->userAgentOverride retain] autorelease];
    }
    
    // Look to see if we need to spoof.
    // First step is to get the host as a C-format string.
    UserAgentStringType type = Safari;
    NSString *host = [URL host];
    char hostBuffer[256];
    if (host && CFStringGetCString((CFStringRef)host, hostBuffer, sizeof(hostBuffer), kCFStringEncodingASCII)) {
        // Next step is to find the last part of the name.
        // We get the part with only two dots, and the part with only one dot.
        const char *thirdToLastPeriod = NULL;
        const char *nextToLastPeriod = NULL;
        const char *lastPeriod = NULL;
        char c;
        char *p = hostBuffer;
        while ((c = *p)) {
            if (c == '.') {
                thirdToLastPeriod = nextToLastPeriod;
                nextToLastPeriod = lastPeriod;
                lastPeriod = p;
            } else {
                *p = tolower(c);
            }
            ++p;
        }
        // Now look up that last part in the gperf spoof table.
        if (lastPeriod) {
            const char *lastPart;
            const struct UserAgentSpoofTableEntry *entry = NULL;
            if (nextToLastPeriod) {
                lastPart = thirdToLastPeriod ? thirdToLastPeriod + 1 : hostBuffer;
                entry = _web_findSpoofTableEntry(lastPart, p - lastPart);
            }
            if (!entry) {
                lastPart = nextToLastPeriod ? nextToLastPeriod + 1 : hostBuffer;
                entry = _web_findSpoofTableEntry(lastPart, p - lastPart);
            }
            if (entry) {
                type = entry->type;
            }
        }
    }

    NSString **userAgentStorage = &_private->userAgent[type];

    NSString *userAgent = *userAgentStorage;
    if (userAgent) {
        return [[userAgent retain] autorelease];
    }
    
    // FIXME: Some day we will start reporting the actual CPU here instead of hardcoding PPC.

    NSString *language = [NSUserDefaults _web_preferredLanguageCode];
    id sourceVersion = [[NSBundle bundleForClass:[WebView class]]
        objectForInfoDictionaryKey:(id)kCFBundleVersionKey];
    NSString *applicationName = _private->applicationNameForUserAgent;

    switch (type) {
        case Safari:
            if ([applicationName length]) {
                userAgent = [NSString stringWithFormat:@"Mozilla/5.0 (Macintosh; U; PPC Mac OS X; %@) AppleWebKit/%@ (KHTML, like Gecko) %@",
                    language, sourceVersion, applicationName];
            } else {
                userAgent = [NSString stringWithFormat:@"Mozilla/5.0 (Macintosh; U; PPC Mac OS X; %@) AppleWebKit/%@ (KHTML, like Gecko)",
                    language, sourceVersion];
            }
            break;
        case MacIE:
            if ([applicationName length]) {
                userAgent = [NSString stringWithFormat:@"Mozilla/4.0 (compatible; MSIE 5.2; Mac_PowerPC) AppleWebKit/%@ %@",
                    language, sourceVersion, applicationName];
            } else {
                userAgent = [NSString stringWithFormat:@"Mozilla/4.0 (compatible; MSIE 5.2; Mac_PowerPC) AppleWebKit/%@",
                    language, sourceVersion];
            }
            break;
        case WinIE:
            if ([applicationName length]) {
                userAgent = [NSString stringWithFormat:@"Mozilla/4.0 (compatible; MSIE 6.0; Windows NT) AppleWebKit/%@ %@",
                    language, sourceVersion, applicationName];
            } else {
                userAgent = [NSString stringWithFormat:@"Mozilla/4.0 (compatible; MSIE 6.0; Windows NT) AppleWebKit/%@",
                    language, sourceVersion];
            }
            break;
    }

    *userAgentStorage = [userAgent retain];
    return userAgent;
}

- (void)setHostWindow:(NSWindow *)hostWindow
{
    if (hostWindow != _private->hostWindow) {
        [[self mainFrame] _viewWillMoveToHostWindow:hostWindow];
        [_private->hostWindow release];
        _private->hostWindow = [hostWindow retain];
        [[self mainFrame] _viewDidMoveToHostWindow];
    }
}

- (NSWindow *)hostWindow
{
    return _private->hostWindow;
}

-(BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    if ([[self mainFrame] frameView]) {
        [[self window] makeFirstResponder:[[self mainFrame] frameView]];
    }
    return YES;
}

@end


@implementation WebView (WebIBActions)

- (IBAction)takeStringURLFrom: sender
{
    NSString *URLString = [sender stringValue];
    
    [[self mainFrame] loadRequest: [NSURLRequest requestWithURL: [NSURL URLWithString: URLString]]];
}

- (IBAction)goBack:(id)sender
{
    [self goBack];
}

- (IBAction)goForward:(id)sender
{
    [self goForward];
}

- (IBAction)stopLoading:(id)sender
{
    [[self mainFrame] stopLoading];
}


@end

