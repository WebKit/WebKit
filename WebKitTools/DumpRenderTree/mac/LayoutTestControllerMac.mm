/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "DumpRenderTree.h"
#import "LayoutTestController.h"

#import "EditingDelegate.h"
#import "MockGeolocationProvider.h"
#import "PolicyDelegate.h"
#import "UIDelegate.h"
#import "WorkQueue.h"
#import "WorkQueueItem.h"
#import <Foundation/Foundation.h>
#import <JavaScriptCore/JSRetainPtr.h>
#import <JavaScriptCore/JSStringRef.h>
#import <JavaScriptCore/JSStringRefCF.h>
#import <WebKit/DOMDocument.h>
#import <WebKit/DOMElement.h>
#import <WebKit/WebApplicationCache.h>
#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebCoreStatistics.h>
#import <WebKit/WebDOMOperationsPrivate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDatabaseManagerPrivate.h>
#import <WebKit/WebDeviceOrientation.h>
#import <WebKit/WebDeviceOrientationProviderMock.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebGeolocationPosition.h>
#import <WebKit/WebHTMLRepresentation.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebHistory.h>
#import <WebKit/WebHistoryPrivate.h>
#import <WebKit/WebIconDatabasePrivate.h>
#import <WebKit/WebInspectorPrivate.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebQuotaManager.h>
#import <WebKit/WebScriptWorld.h>
#import <WebKit/WebSecurityOriginPrivate.h>
#import <WebKit/WebTypesInternal.h>
#import <WebKit/WebView.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebWorkersPrivate.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>

@interface CommandValidationTarget : NSObject <NSValidatedUserInterfaceItem>
{
    SEL _action;
}
- (id)initWithAction:(SEL)action;
@end

@implementation CommandValidationTarget

- (id)initWithAction:(SEL)action
{
    self = [super init];
    if (!self)
        return nil;

    _action = action;
    return self;
}

- (SEL)action
{
    return _action;
}

- (NSInteger)tag
{
    return 0;
}

@end

LayoutTestController::~LayoutTestController()
{
}

void LayoutTestController::addDisallowedURL(JSStringRef url)
{
    RetainPtr<CFStringRef> urlCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, url));

    if (!disallowedURLs)
        disallowedURLs = CFSetCreateMutable(kCFAllocatorDefault, 0, NULL);

    // Canonicalize the URL
    NSURLRequest* request = [NSURLRequest requestWithURL:[NSURL URLWithString:(NSString *)urlCF.get()]];
    request = [NSURLProtocol canonicalRequestForRequest:request];

    CFSetAddValue(disallowedURLs, [request URL]);
}

bool LayoutTestController::callShouldCloseOnWebView()
{
    return [[mainFrame webView] shouldClose];
}

void LayoutTestController::clearAllApplicationCaches()
{
    [WebApplicationCache deleteAllApplicationCaches];
}

void LayoutTestController::clearAllDatabases()
{
    [[WebDatabaseManager sharedWebDatabaseManager] deleteAllDatabases];
}

void LayoutTestController::clearBackForwardList()
{
    WebBackForwardList *backForwardList = [[mainFrame webView] backForwardList];
    WebHistoryItem *item = [[backForwardList currentItem] retain];

    // We clear the history by setting the back/forward list's capacity to 0
    // then restoring it back and adding back the current item.
    int capacity = [backForwardList capacity];
    [backForwardList setCapacity:0];
    [backForwardList setCapacity:capacity];
    [backForwardList addItem:item];
    [backForwardList goToItem:item];
    [item release];
}

JSStringRef LayoutTestController::copyDecodedHostName(JSStringRef name)
{
    RetainPtr<CFStringRef> nameCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, name));
    NSString *nameNS = (NSString *)nameCF.get();
    return JSStringCreateWithCFString((CFStringRef)[nameNS _web_decodeHostName]);
}

JSStringRef LayoutTestController::copyEncodedHostName(JSStringRef name)
{
    RetainPtr<CFStringRef> nameCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, name));
    NSString *nameNS = (NSString *)nameCF.get();
    return JSStringCreateWithCFString((CFStringRef)[nameNS _web_encodeHostName]);
}

void LayoutTestController::display()
{
    displayWebView();
}

JSRetainPtr<JSStringRef> LayoutTestController::counterValueForElementById(JSStringRef id)
{
    RetainPtr<CFStringRef> idCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, id));
    NSString *idNS = (NSString *)idCF.get();

    DOMElement *element = [[mainFrame DOMDocument] getElementById:idNS];
    if (!element)
        return 0;

    JSRetainPtr<JSStringRef> counterValue(Adopt, JSStringCreateWithCFString((CFStringRef)[mainFrame counterValueForElement:element]));
    return counterValue;
}

void LayoutTestController::keepWebHistory()
{
    if (![WebHistory optionalSharedHistory]) {
        WebHistory *history = [[WebHistory alloc] init];
        [WebHistory setOptionalSharedHistory:history];
        [history release];
    }
}

JSValueRef LayoutTestController::computedStyleIncludingVisitedInfo(JSContextRef context, JSValueRef value)
{   
    return [[mainFrame webView] _computedStyleIncludingVisitedInfo:context forElement:value];
}

JSValueRef LayoutTestController::nodesFromRect(JSContextRef context, JSValueRef value, int x, int y, unsigned hPadding, unsigned vPadding, bool ignoreClipping)
{
    return [[mainFrame webView] _nodesFromRect:context forDocument:value x:x y:y hPadding:hPadding vPadding:vPadding ignoreClipping:ignoreClipping];
}

JSRetainPtr<JSStringRef> LayoutTestController::layerTreeAsText() const
{
    JSRetainPtr<JSStringRef> string(Adopt, JSStringCreateWithCFString((CFStringRef)[mainFrame _layerTreeAsText]));
    return string;
}

JSRetainPtr<JSStringRef> LayoutTestController::markerTextForListItem(JSContextRef context, JSValueRef nodeObject) const
{
    DOMElement *element = [DOMElement _DOMElementFromJSContext:context value:nodeObject];
    if (!element)
        return JSRetainPtr<JSStringRef>();

    JSRetainPtr<JSStringRef> markerText(Adopt, JSStringCreateWithCFString((CFStringRef)[element _markerTextForListItem]));
    return markerText;
}

int LayoutTestController::pageNumberForElementById(JSStringRef id, float pageWidthInPixels, float pageHeightInPixels)
{
    RetainPtr<CFStringRef> idCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, id));
    NSString *idNS = (NSString *)idCF.get();

    DOMElement *element = [[mainFrame DOMDocument] getElementById:idNS];
    if (!element)
        return -1;

    return [mainFrame pageNumberForElement:element:pageWidthInPixels:pageHeightInPixels];
}

JSRetainPtr<JSStringRef> LayoutTestController::pageProperty(const char* propertyName, int pageNumber) const
{
    JSRetainPtr<JSStringRef> propertyValue(Adopt, JSStringCreateWithCFString((CFStringRef)[mainFrame pageProperty:propertyName:pageNumber]));
    return propertyValue;
}

bool LayoutTestController::isPageBoxVisible(int pageNumber) const
{
    return [mainFrame isPageBoxVisible:pageNumber];
}

JSRetainPtr<JSStringRef> LayoutTestController::pageSizeAndMarginsInPixels(int pageNumber, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft) const
{
    JSRetainPtr<JSStringRef> propertyValue(Adopt, JSStringCreateWithCFString((CFStringRef)[mainFrame pageSizeAndMarginsInPixels:pageNumber:width:height:marginTop:marginRight:marginBottom:marginLeft]));
    return propertyValue;
}

int LayoutTestController::numberOfPages(float pageWidthInPixels, float pageHeightInPixels)
{
    return [mainFrame numberOfPages:pageWidthInPixels:pageHeightInPixels];
}

size_t LayoutTestController::webHistoryItemCount()
{
    return [[[WebHistory optionalSharedHistory] allItems] count];
}

unsigned LayoutTestController::workerThreadCount() const
{
    return [WebWorkersPrivate workerThreadCount];
}

void LayoutTestController::notifyDone()
{
    if (m_waitToDump && !topLoadingFrame && !WorkQueue::shared()->count())
        dump();
    m_waitToDump = false;
}

JSStringRef LayoutTestController::pathToLocalResource(JSContextRef context, JSStringRef url)
{
    return JSStringRetain(url); // Do nothing on mac.
}

void LayoutTestController::queueLoad(JSStringRef url, JSStringRef target)
{
    RetainPtr<CFStringRef> urlCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, url));
    NSString *urlNS = (NSString *)urlCF.get();

    NSURL *nsurl = [NSURL URLWithString:urlNS relativeToURL:[[[mainFrame dataSource] response] URL]];
    NSString* nsurlString = [nsurl absoluteString];

    JSRetainPtr<JSStringRef> absoluteURL(Adopt, JSStringCreateWithUTF8CString([nsurlString UTF8String]));
    WorkQueue::shared()->queue(new LoadItem(absoluteURL.get(), target));
}

void LayoutTestController::setAcceptsEditing(bool newAcceptsEditing)
{
    [(EditingDelegate *)[[mainFrame webView] editingDelegate] setAcceptsEditing:newAcceptsEditing];
}

void LayoutTestController::setAlwaysAcceptCookies(bool alwaysAcceptCookies)
{
    if (alwaysAcceptCookies == m_alwaysAcceptCookies)
        return;

    m_alwaysAcceptCookies = alwaysAcceptCookies;
    NSHTTPCookieAcceptPolicy cookieAcceptPolicy = alwaysAcceptCookies ? NSHTTPCookieAcceptPolicyAlways : NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
    [[NSHTTPCookieStorage sharedHTTPCookieStorage] setCookieAcceptPolicy:cookieAcceptPolicy];
}

void LayoutTestController::setAppCacheMaximumSize(unsigned long long size)
{
    [WebApplicationCache setMaximumSize:size];
}

void LayoutTestController::setApplicationCacheOriginQuota(unsigned long long quota)
{
    WebSecurityOrigin *origin = [[WebSecurityOrigin alloc] initWithURL:[NSURL URLWithString:@"http://127.0.0.1:8000"]];
    [[origin applicationCacheQuotaManager] setQuota:quota];
    [origin release];
}

void LayoutTestController::setAuthorAndUserStylesEnabled(bool flag)
{
    [[[mainFrame webView] preferences] setAuthorAndUserStylesEnabled:flag];
}

void LayoutTestController::setCustomPolicyDelegate(bool setDelegate, bool permissive)
{
    if (setDelegate) {
        [policyDelegate setPermissive:permissive];
        [[mainFrame webView] setPolicyDelegate:policyDelegate];
    } else
        [[mainFrame webView] setPolicyDelegate:nil];
}

void LayoutTestController::setDatabaseQuota(unsigned long long quota)
{    
    WebSecurityOrigin *origin = [[WebSecurityOrigin alloc] initWithURL:[NSURL URLWithString:@"file:///"]];
    [[origin databaseQuotaManager] setQuota:quota];
    [origin release];
}

void LayoutTestController::setDomainRelaxationForbiddenForURLScheme(bool forbidden, JSStringRef scheme)
{
    RetainPtr<CFStringRef> schemeCFString(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, scheme));
    [WebView _setDomainRelaxationForbidden:forbidden forURLScheme:(NSString *)schemeCFString.get()];
}

void LayoutTestController::setMockDeviceOrientation(bool canProvideAlpha, double alpha, bool canProvideBeta, double beta, bool canProvideGamma, double gamma)
{
    // DumpRenderTree configured the WebView to use WebDeviceOrientationProviderMock.
    id<WebDeviceOrientationProvider> provider = [[mainFrame webView] _deviceOrientationProvider];
    WebDeviceOrientationProviderMock* mockProvider = static_cast<WebDeviceOrientationProviderMock*>(provider);
    WebDeviceOrientation* orientation = [[WebDeviceOrientation alloc] initWithCanProvideAlpha:canProvideAlpha alpha:alpha canProvideBeta:canProvideBeta beta:beta canProvideGamma:canProvideGamma gamma:gamma];
    [mockProvider setOrientation:orientation];
}

void LayoutTestController::setMockGeolocationPosition(double latitude, double longitude, double accuracy)
{
    WebGeolocationPosition *position = [[WebGeolocationPosition alloc] initWithTimestamp:0 latitude:latitude longitude:longitude accuracy:accuracy];
    [[MockGeolocationProvider shared] setPosition:position];
    [position release];
}

void LayoutTestController::setMockGeolocationError(int code, JSStringRef message)
{
    RetainPtr<CFStringRef> messageCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, message));
    NSString *messageNS = (NSString *)messageCF.get();
    NSError *error = [NSError errorWithDomain:WebKitErrorDomain code:code userInfo:[NSDictionary dictionaryWithObject:messageNS forKey:NSLocalizedDescriptionKey]];
    [[MockGeolocationProvider shared] setError:error];
}

void LayoutTestController::setGeolocationPermission(bool allow)
{
    setGeolocationPermissionCommon(allow);
    [[[mainFrame webView] UIDelegate] didSetMockGeolocationPermission];
}

void LayoutTestController::setMockSpeechInputResult(JSStringRef result)
{
    // FIXME: Implement for speech input layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=39485.
}

void LayoutTestController::setIconDatabaseEnabled(bool iconDatabaseEnabled)
{
    // FIXME: Workaround <rdar://problem/6480108>
    static WebIconDatabase* sharedWebIconDatabase = NULL;
    if (!sharedWebIconDatabase) {
        if (!iconDatabaseEnabled)
            return;
        sharedWebIconDatabase = [WebIconDatabase sharedIconDatabase];
        if ([sharedWebIconDatabase isEnabled] == iconDatabaseEnabled)
            return;
    }
    [sharedWebIconDatabase setEnabled:iconDatabaseEnabled];
}

void LayoutTestController::setJavaScriptProfilingEnabled(bool profilingEnabled)
{
    setDeveloperExtrasEnabled(profilingEnabled);
    [[[mainFrame webView] inspector] setJavaScriptProfilingEnabled:profilingEnabled];
}

void LayoutTestController::setMainFrameIsFirstResponder(bool flag)
{
    NSView *documentView = [[mainFrame frameView] documentView];
    
    NSResponder *firstResponder = flag ? documentView : nil;
    [[[mainFrame webView] window] makeFirstResponder:firstResponder];
}

void LayoutTestController::setPrivateBrowsingEnabled(bool privateBrowsingEnabled)
{
    [[[mainFrame webView] preferences] setPrivateBrowsingEnabled:privateBrowsingEnabled];
}

void LayoutTestController::setXSSAuditorEnabled(bool enabled)
{
    [[[mainFrame webView] preferences] setXSSAuditorEnabled:enabled];
}

void LayoutTestController::setFrameFlatteningEnabled(bool enabled)
{
    [[[mainFrame webView] preferences] setFrameFlatteningEnabled:enabled];
}

void LayoutTestController::setSpatialNavigationEnabled(bool enabled)
{
    // FIXME: Implement for SpatialNavigation layout tests.
}

void LayoutTestController::setAllowUniversalAccessFromFileURLs(bool enabled)
{
    [[[mainFrame webView] preferences] setAllowUniversalAccessFromFileURLs:enabled];
}

void LayoutTestController::setAllowFileAccessFromFileURLs(bool enabled)
{
    [[[mainFrame webView] preferences] setAllowFileAccessFromFileURLs:enabled];
}

void LayoutTestController::setPopupBlockingEnabled(bool popupBlockingEnabled)
{
    [[[mainFrame webView] preferences] setJavaScriptCanOpenWindowsAutomatically:!popupBlockingEnabled];
}

void LayoutTestController::setPluginsEnabled(bool pluginsEnabled)
{
    [[[mainFrame webView] preferences] setPlugInsEnabled:pluginsEnabled];
}

void LayoutTestController::setJavaScriptCanAccessClipboard(bool enabled)
{
    [[[mainFrame webView] preferences] setJavaScriptCanAccessClipboard:enabled];
}

void LayoutTestController::setTabKeyCyclesThroughElements(bool cycles)
{
    [[mainFrame webView] setTabKeyCyclesThroughElements:cycles];
}

void LayoutTestController::setTimelineProfilingEnabled(bool enabled)
{
    [[[mainFrame webView] inspector] setTimelineProfilingEnabled:enabled];
}

void LayoutTestController::setUseDashboardCompatibilityMode(bool flag)
{
    [[mainFrame webView] _setDashboardBehavior:WebDashboardBehaviorUseBackwardCompatibilityMode to:flag];
}

void LayoutTestController::setUserStyleSheetEnabled(bool flag)
{
    [[WebPreferences standardPreferences] setUserStyleSheetEnabled:flag];
}

void LayoutTestController::setUserStyleSheetLocation(JSStringRef path)
{
    RetainPtr<CFStringRef> pathCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, path));
    NSURL *url = [NSURL URLWithString:(NSString *)pathCF.get()];
    [[WebPreferences standardPreferences] setUserStyleSheetLocation:url];
}

void LayoutTestController::setViewModeMediaFeature(JSStringRef mode)
{
    // FIXME: implement
}

void LayoutTestController::disableImageLoading()
{
    [[WebPreferences standardPreferences] setLoadsImagesAutomatically:NO];
}

void LayoutTestController::dispatchPendingLoadRequests()
{
    [[mainFrame webView] _dispatchPendingLoadRequests];
}

void LayoutTestController::overridePreference(JSStringRef key, JSStringRef value)
{
    RetainPtr<CFStringRef> keyCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, key));
    NSString *keyNS = (NSString *)keyCF.get();

    RetainPtr<CFStringRef> valueCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, value));
    NSString *valueNS = (NSString *)valueCF.get();

    [[WebPreferences standardPreferences] _setPreferenceForTestWithValue:valueNS forKey:keyNS];
}

void LayoutTestController::removeAllVisitedLinks()
{
    [WebHistory _removeAllVisitedLinks];
}

void LayoutTestController::setPersistentUserStyleSheetLocation(JSStringRef jsURL)
{
    RetainPtr<CFStringRef> urlString(AdoptCF, JSStringCopyCFString(0, jsURL));
    ::setPersistentUserStyleSheetLocation(urlString.get());
}

void LayoutTestController::clearPersistentUserStyleSheet()
{
    ::setPersistentUserStyleSheetLocation(0);
}

void LayoutTestController::setWindowIsKey(bool windowIsKey)
{
    m_windowIsKey = windowIsKey;
    [[mainFrame webView] _updateActiveState];
}

void LayoutTestController::setSmartInsertDeleteEnabled(bool flag)
{
    [[mainFrame webView] setSmartInsertDeleteEnabled:flag];
}

void LayoutTestController::setSelectTrailingWhitespaceEnabled(bool flag)
{
    [[mainFrame webView] setSelectTrailingWhitespaceEnabled:flag];
}

static const CFTimeInterval waitToDumpWatchdogInterval = 30.0;

static void waitUntilDoneWatchdogFired(CFRunLoopTimerRef timer, void* info)
{
    gLayoutTestController->waitToDumpWatchdogTimerFired();
}

void LayoutTestController::setWaitToDump(bool waitUntilDone)
{
    m_waitToDump = waitUntilDone;
    if (m_waitToDump && !waitToDumpWatchdog) {
        waitToDumpWatchdog = CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + waitToDumpWatchdogInterval, 0, 0, 0, waitUntilDoneWatchdogFired, NULL);
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), waitToDumpWatchdog, kCFRunLoopCommonModes);
    }
}

int LayoutTestController::windowCount()
{
    return CFArrayGetCount(openWindowsRef);
}

bool LayoutTestController::elementDoesAutoCompleteForElementWithId(JSStringRef jsString)
{
    RetainPtr<CFStringRef> idCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, jsString));
    NSString *idNS = (NSString *)idCF.get();
    
    DOMElement *element = [[mainFrame DOMDocument] getElementById:idNS];
    id rep = [[mainFrame dataSource] representation];
    
    if ([rep class] == [WebHTMLRepresentation class])
        return [(WebHTMLRepresentation *)rep elementDoesAutoComplete:element];

    return false;
}

void LayoutTestController::execCommand(JSStringRef name, JSStringRef value)
{
    RetainPtr<CFStringRef> nameCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, name));
    NSString *nameNS = (NSString *)nameCF.get();

    RetainPtr<CFStringRef> valueCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, value));
    NSString *valueNS = (NSString *)valueCF.get();

    [[mainFrame webView] _executeCoreCommandByName:nameNS value:valueNS];
}

void LayoutTestController::setCacheModel(int cacheModel)
{
    [[WebPreferences standardPreferences] setCacheModel:cacheModel];
}

bool LayoutTestController::isCommandEnabled(JSStringRef name)
{
    RetainPtr<CFStringRef> nameCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, name));
    NSString *nameNS = (NSString *)nameCF.get();

    // Accept command strings with capital letters for first letter without trailing colon.
    if (![nameNS hasSuffix:@":"] && [nameNS length]) {
        nameNS = [[[[nameNS substringToIndex:1] lowercaseString]
            stringByAppendingString:[nameNS substringFromIndex:1]]
            stringByAppendingString:@":"];
    }

    SEL selector = NSSelectorFromString(nameNS);
    RetainPtr<CommandValidationTarget> target(AdoptNS, [[CommandValidationTarget alloc] initWithAction:selector]);
    id validator = [NSApp targetForAction:selector to:[mainFrame webView] from:target.get()];
    if (!validator)
        return false;
    if (![validator respondsToSelector:selector])
        return false;
    if (![validator respondsToSelector:@selector(validateUserInterfaceItem:)])
        return true;
    return [validator validateUserInterfaceItem:target.get()];
}

bool LayoutTestController::pauseAnimationAtTimeOnElementWithId(JSStringRef animationName, double time, JSStringRef elementId)
{
    RetainPtr<CFStringRef> idCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, elementId));
    NSString *idNS = (NSString *)idCF.get();
    RetainPtr<CFStringRef> nameCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, animationName));
    NSString *nameNS = (NSString *)nameCF.get();
    
    return [mainFrame _pauseAnimation:nameNS onNode:[[mainFrame DOMDocument] getElementById:idNS] atTime:time];
}

bool LayoutTestController::pauseTransitionAtTimeOnElementWithId(JSStringRef propertyName, double time, JSStringRef elementId)
{
    RetainPtr<CFStringRef> idCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, elementId));
    NSString *idNS = (NSString *)idCF.get();
    RetainPtr<CFStringRef> nameCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, propertyName));
    NSString *nameNS = (NSString *)nameCF.get();
    
    return [mainFrame _pauseTransitionOfProperty:nameNS onNode:[[mainFrame DOMDocument] getElementById:idNS] atTime:time];
}

bool LayoutTestController::sampleSVGAnimationForElementAtTime(JSStringRef animationId, double time, JSStringRef elementId)
{
    RetainPtr<CFStringRef> animationIDCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, animationId));
    NSString *animationIDNS = (NSString *)animationIDCF.get();
    RetainPtr<CFStringRef> elementIDCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, elementId));
    NSString *elementIDNS = (NSString *)elementIDCF.get();

    return [mainFrame _pauseSVGAnimation:elementIDNS onSMILNode:[[mainFrame DOMDocument] getElementById:animationIDNS] atTime:time];
}

unsigned LayoutTestController::numberOfActiveAnimations() const
{
    return [mainFrame _numberOfActiveAnimations];
}

void LayoutTestController::suspendAnimations() const
{
    return [mainFrame _suspendAnimations];
}

void LayoutTestController::resumeAnimations() const
{
    return [mainFrame _resumeAnimations];
}

void LayoutTestController::waitForPolicyDelegate()
{
    setWaitToDump(true);
    [policyDelegate setControllerToNotifyDone:this];
    [[mainFrame webView] setPolicyDelegate:policyDelegate];
}

void LayoutTestController::addOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains)
{
    RetainPtr<CFStringRef> sourceOriginCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, sourceOrigin));
    NSString *sourceOriginNS = (NSString *)sourceOriginCF.get();
    RetainPtr<CFStringRef> protocolCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, destinationProtocol));
    NSString *destinationProtocolNS = (NSString *)protocolCF.get();
    RetainPtr<CFStringRef> hostCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, destinationHost));
    NSString *destinationHostNS = (NSString *)hostCF.get();
    [WebView _addOriginAccessWhitelistEntryWithSourceOrigin:sourceOriginNS destinationProtocol:destinationProtocolNS destinationHost:destinationHostNS allowDestinationSubdomains:allowDestinationSubdomains];
}

void LayoutTestController::removeOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains)
{
    RetainPtr<CFStringRef> sourceOriginCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, sourceOrigin));
    NSString *sourceOriginNS = (NSString *)sourceOriginCF.get();
    RetainPtr<CFStringRef> protocolCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, destinationProtocol));
    NSString *destinationProtocolNS = (NSString *)protocolCF.get();
    RetainPtr<CFStringRef> hostCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, destinationHost));
    NSString *destinationHostNS = (NSString *)hostCF.get();
    [WebView _removeOriginAccessWhitelistEntryWithSourceOrigin:sourceOriginNS destinationProtocol:destinationProtocolNS destinationHost:destinationHostNS allowDestinationSubdomains:allowDestinationSubdomains];
}

void LayoutTestController::setScrollbarPolicy(JSStringRef orientation, JSStringRef policy)
{
    // FIXME: implement
}

void LayoutTestController::addUserScript(JSStringRef source, bool runAtStart, bool allFrames)
{
    RetainPtr<CFStringRef> sourceCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, source));
    NSString *sourceNS = (NSString *)sourceCF.get();
    [WebView _addUserScriptToGroup:@"org.webkit.DumpRenderTree" world:[WebScriptWorld world] source:sourceNS url:nil whitelist:nil blacklist:nil injectionTime:(runAtStart ? WebInjectAtDocumentStart : WebInjectAtDocumentEnd) injectedFrames:(allFrames ? WebInjectInAllFrames : WebInjectInTopFrameOnly)];
}

void LayoutTestController::addUserStyleSheet(JSStringRef source, bool allFrames)
{
    RetainPtr<CFStringRef> sourceCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, source));
    NSString *sourceNS = (NSString *)sourceCF.get();
    [WebView _addUserStyleSheetToGroup:@"org.webkit.DumpRenderTree" world:[WebScriptWorld world] source:sourceNS url:nil whitelist:nil blacklist:nil injectedFrames:(allFrames ? WebInjectInAllFrames : WebInjectInTopFrameOnly)];
}

void LayoutTestController::setDeveloperExtrasEnabled(bool enabled)
{
    [[[mainFrame webView] preferences] setDeveloperExtrasEnabled:enabled];
}

void LayoutTestController::showWebInspector()
{
    [[[mainFrame webView] inspector] show:nil];
}

void LayoutTestController::closeWebInspector()
{
    [[[mainFrame webView] inspector] close:nil];
}

void LayoutTestController::evaluateInWebInspector(long callId, JSStringRef script)
{
    RetainPtr<CFStringRef> scriptCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, script));
    NSString *scriptNS = (NSString *)scriptCF.get();
    [[[mainFrame webView] inspector] evaluateInFrontend:nil callId:callId script:scriptNS];
}

typedef HashMap<unsigned, RetainPtr<WebScriptWorld> > WorldMap;
static WorldMap& worldMap()
{
    static WorldMap& map = *new WorldMap;
    return map;
}

unsigned worldIDForWorld(WebScriptWorld *world)
{
    WorldMap::const_iterator end = worldMap().end();
    for (WorldMap::const_iterator it = worldMap().begin(); it != end; ++it) {
        if (it->second == world)
            return it->first;
    }

    return 0;
}

void LayoutTestController::evaluateScriptInIsolatedWorld(unsigned worldID, JSObjectRef globalObject, JSStringRef script)
{
    RetainPtr<CFStringRef> scriptCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, script));
    NSString *scriptNS = (NSString *)scriptCF.get();

    // A worldID of 0 always corresponds to a new world. Any other worldID corresponds to a world
    // that is created once and cached forever.
    WebScriptWorld *world;
    if (!worldID)
        world = [WebScriptWorld world];
    else {
        RetainPtr<WebScriptWorld>& worldSlot = worldMap().add(worldID, 0).first->second;
        if (!worldSlot)
            worldSlot.adoptNS([[WebScriptWorld alloc] init]);
        world = worldSlot.get();
    }

    [mainFrame _stringByEvaluatingJavaScriptFromString:scriptNS withGlobalObject:globalObject inScriptWorld:world];
}

@interface APITestDelegate : NSObject
{
    bool* m_condition;
}
@end

@implementation APITestDelegate

- (id)initWithCompletionCondition:(bool*)condition
{
    [super init];
    ASSERT(condition);
    m_condition = condition;
    *m_condition = false;
    return self;
}

- (void)webView:(WebView *)sender didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    printf("API Test load failed\n");
    *m_condition = true;
}

- (void)webView:(WebView *)sender didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    printf("API Test load failed provisional\n");
    *m_condition = true;
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    printf("API Test load succeeded\n");
    *m_condition = true;
}

@end

void LayoutTestController::apiTestNewWindowDataLoadBaseURL(JSStringRef utf8Data, JSStringRef baseURL)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    RetainPtr<CFStringRef> utf8DataCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, utf8Data));
    RetainPtr<CFStringRef> baseURLCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, baseURL));
    
    WebView *webView = [[WebView alloc] initWithFrame:NSZeroRect frameName:@"" groupName:@""];

    bool done = false;
    APITestDelegate *delegate = [[APITestDelegate alloc] initWithCompletionCondition:&done];
    [webView setFrameLoadDelegate:delegate];

    [[webView mainFrame] loadData:[(NSString *)utf8DataCF.get() dataUsingEncoding:NSUTF8StringEncoding] MIMEType:@"text/html" textEncodingName:@"utf-8" baseURL:[NSURL URLWithString:(NSString *)baseURLCF.get()]];
    
    while (!done) {
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
        [pool release];
    }
        
    [webView close];
    [webView release];
    [delegate release];
    [pool release];
}

void LayoutTestController::apiTestGoToCurrentBackForwardItem()
{
    WebView *view = [mainFrame webView];
    [view goToBackForwardItem:[[view backForwardList] currentItem]];
}

void LayoutTestController::setWebViewEditable(bool editable)
{
    WebView *view = [mainFrame webView];
    [view setEditable:editable];
}

#ifndef BUILDING_ON_TIGER
static NSString *SynchronousLoaderRunLoopMode = @"DumpRenderTreeSynchronousLoaderRunLoopMode";

#if defined(BUILDING_ON_LEOPARD) || defined(BUILDING_ON_SNOW_LEOPARD)
@protocol NSURLConnectionDelegate <NSObject>
@end
#endif

@interface SynchronousLoader : NSObject <NSURLConnectionDelegate>
{
    NSString *m_username;
    NSString *m_password;
    BOOL m_isDone;
}
+ (void)makeRequest:(NSURLRequest *)request withUsername:(NSString *)username password:(NSString *)password;
@end

@implementation SynchronousLoader : NSObject
- (void)dealloc
{
    [m_username release];
    [m_password release];

    [super dealloc];
}

- (BOOL)connectionShouldUseCredentialStorage:(NSURLConnection *)connection
{
    return YES;
}

- (void)connection:(NSURLConnection *)connection didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if ([challenge previousFailureCount] == 0) {
        NSURLCredential *credential = [[NSURLCredential alloc]  initWithUser:m_username password:m_password persistence:NSURLCredentialPersistenceForSession];
        [[challenge sender] useCredential:credential forAuthenticationChallenge:challenge];
        return;
    }
    [[challenge sender] cancelAuthenticationChallenge:challenge];
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    printf("SynchronousLoader failed: %s\n", [[error description] UTF8String]);
    m_isDone = YES;
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    m_isDone = YES;
}

+ (void)makeRequest:(NSURLRequest *)request withUsername:(NSString *)username password:(NSString *)password
{
    ASSERT(![[request URL] user]);
    ASSERT(![[request URL] password]);

    SynchronousLoader *delegate = [[SynchronousLoader alloc] init];
    delegate->m_username = [username copy];
    delegate->m_password = [password copy];

    NSURLConnection *connection = [[NSURLConnection alloc] initWithRequest:request delegate:delegate startImmediately:NO];
    [connection scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:SynchronousLoaderRunLoopMode];
    [connection start];
    
    while (!delegate->m_isDone)
        [[NSRunLoop currentRunLoop] runMode:SynchronousLoaderRunLoopMode beforeDate:[NSDate distantFuture]];

    [connection cancel];
    
    [connection release];
    [delegate release];
}

@end
#endif

void LayoutTestController::authenticateSession(JSStringRef url, JSStringRef username, JSStringRef password)
{
    // See <rdar://problem/7880699>.
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
    RetainPtr<CFStringRef> urlStringCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, url));
    RetainPtr<CFStringRef> usernameCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, username));
    RetainPtr<CFStringRef> passwordCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, password));

    NSURLRequest *request = [[NSURLRequest alloc] initWithURL:[NSURL URLWithString:(NSString *)urlStringCF.get()]];

    [SynchronousLoader makeRequest:request withUsername:(NSString *)usernameCF.get() password:(NSString *)passwordCF.get()];
#endif
}

void LayoutTestController::setEditingBehavior(const char* editingBehavior)
{
    NSString* editingBehaviorNS = [[NSString alloc] initWithUTF8String:editingBehavior];
    if ([editingBehaviorNS isEqualToString:@"mac"])
        [[WebPreferences standardPreferences] setEditingBehavior:WebKitEditingMacBehavior];
    if ([editingBehaviorNS isEqualToString:@"win"])
        [[WebPreferences standardPreferences] setEditingBehavior:WebKitEditingWinBehavior];
    [editingBehaviorNS release];
}

void LayoutTestController::abortModal()
{
    [NSApp abortModal];
}
