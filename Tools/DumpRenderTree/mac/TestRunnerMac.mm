/*
 * Copyright (C) 2007-2016 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#import "TestRunner.h"

#import "DefaultPolicyDelegate.h"
#import "EditingDelegate.h"
#import "JSBasics.h"
#import "LayoutTestSpellChecker.h"
#import "MockGeolocationProvider.h"
#import "MockWebNotificationProvider.h"
#import "PolicyDelegate.h"
#import "UIDelegate.h"
#import "WorkQueue.h"
#import "WorkQueueItem.h"
#import <Foundation/Foundation.h>
#import <JavaScriptCore/JSStringRefCF.h>
#import <WebCore/GeolocationPositionData.h>
#import <WebKit/DOMDocument.h>
#import <WebKit/DOMElement.h>
#import <WebKit/DOMHTMLInputElementPrivate.h>
#import <WebKit/WebApplicationCache.h>
#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebCoreStatistics.h>
#import <WebKit/WebDOMOperationsPrivate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDatabaseManagerPrivate.h>
#import <WebKit/WebDeviceOrientation.h>
#import <WebKit/WebDeviceOrientationProviderMock.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameLoadDelegate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebGeolocationPosition.h>
#import <WebKit/WebHTMLRepresentation.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebHistory.h>
#import <WebKit/WebHistoryPrivate.h>
#import <WebKit/WebInspectorPrivate.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebQuotaManager.h>
#import <WebKit/WebScriptWorld.h>
#import <WebKit/WebSecurityOriginPrivate.h>
#import <WebKit/WebStorageManagerPrivate.h>
#import <WebKit/WebView.h>
#import <WebKit/WebViewPrivate.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/WallTime.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#import <WebKit/WebCoreThread.h>
#import <WebKit/WebCoreThreadMessage.h>
#import <WebKit/WebDOMOperationsPrivate.h>
#endif

#if PLATFORM(MAC)
#import "WebHTMLViewForTestingMac.h"
#endif

#if !PLATFORM(IOS_FAMILY)

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
#endif

@interface WebGeolocationPosition (Internal)
- (id)initWithGeolocationPosition:(WebCore::GeolocationPositionData&&)coreGeolocationPosition;
@end

TestRunner::~TestRunner()
{
}

JSContextRef TestRunner::mainFrameJSContext()
{
    return [mainFrame globalContext];
}

void TestRunner::addDisallowedURL(JSStringRef url)
{
    RetainPtr<CFStringRef> urlCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, url));

    if (!disallowedURLs)
        disallowedURLs = adoptCF(CFSetCreateMutable(kCFAllocatorDefault, 0, NULL));

    // Canonicalize the URL
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:(__bridge NSString *)urlCF.get()]];
    request = [NSURLProtocol canonicalRequestForRequest:request];

    CFSetAddValue(disallowedURLs.get(), (__bridge CFURLRef)[request URL]);
}

bool TestRunner::callShouldCloseOnWebView()
{
    return [[mainFrame webView] shouldClose];
}

void TestRunner::clearAllApplicationCaches()
{
    [WebApplicationCache deleteAllApplicationCaches];
}

long long TestRunner::applicationCacheDiskUsageForOrigin(JSStringRef url)
{
    auto urlCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, url));
    auto origin = adoptNS([[WebSecurityOrigin alloc] initWithURL:[NSURL URLWithString:(__bridge NSString *)urlCF.get()]]);
    long long usage = [WebApplicationCache diskUsageForOrigin:origin.get()];
    return usage;
}

void TestRunner::clearApplicationCacheForOrigin(JSStringRef url)
{
    auto urlCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, url));
    auto origin = adoptNS([[WebSecurityOrigin alloc] initWithURL:[NSURL URLWithString:(__bridge NSString *)urlCF.get()]]);
    [WebApplicationCache deleteCacheForOrigin:origin.get()];
}

static JSObjectRef originsArrayToJS(JSContextRef context, NSArray *origins)
{
    auto count = [origins count];
    auto array = JSObjectMakeArray(context, 0, nullptr, nullptr);
    for (NSUInteger i = 0; i < count; i++) {
        NSString *origin = [[origins objectAtIndex:i] databaseIdentifier];
        auto originJS = adopt(JSStringCreateWithCFString((__bridge CFStringRef)origin));
        JSObjectSetPropertyAtIndex(context, array, i, JSValueMakeString(context, originJS.get()), 0);
    }
    return array;
}

JSValueRef TestRunner::originsWithApplicationCache(JSContextRef context)
{
    return originsArrayToJS(context, [WebApplicationCache originsWithCache]);
}

void TestRunner::clearAllDatabases()
{
    [[WebDatabaseManager sharedWebDatabaseManager] deleteAllDatabases];
    [[WebDatabaseManager sharedWebDatabaseManager] deleteAllIndexedDatabases];
}

void TestRunner::clearNotificationPermissionState()
{
    [[mainFrame webView] _clearNotificationPermissionState];
}

void TestRunner::setStorageDatabaseIdleInterval(double interval)
{
    [WebStorageManager setStorageDatabaseIdleInterval:interval];
}

void TestRunner::setSpellCheckerLoggingEnabled(bool enabled)
{
#if PLATFORM(MAC)
    [LayoutTestSpellChecker checker].spellCheckerLoggingEnabled = enabled;
#else
    UNUSED_PARAM(enabled);
#endif
}

void TestRunner::closeIdleLocalStorageDatabases()
{
    [WebStorageManager closeIdleLocalStorageDatabases];
}

void TestRunner::clearBackForwardList()
{
    WebBackForwardList *backForwardList = [[mainFrame webView] backForwardList];
    auto item = retainPtr([backForwardList currentItem]);

    // We clear the history by setting the back/forward list's capacity to 0
    // then restoring it back and adding back the current item.
    int capacity = [backForwardList capacity];
    [backForwardList setCapacity:0];
    [backForwardList setCapacity:capacity];
    [backForwardList addItem:item.get()];
    [backForwardList goToItem:item.get()];
}

JSRetainPtr<JSStringRef> TestRunner::copyDecodedHostName(JSStringRef name)
{
    auto nameCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, name));
    NSString *nameNS = (__bridge NSString *)nameCF.get();
    return adopt(JSStringCreateWithCFString((__bridge CFStringRef)[nameNS _web_decodeHostName]));
}

JSRetainPtr<JSStringRef> TestRunner::copyEncodedHostName(JSStringRef name)
{
    auto nameCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, name));
    NSString *nameNS = (__bridge NSString *)nameCF.get();
    return adopt(JSStringCreateWithCFString((__bridge CFStringRef)[nameNS _web_encodeHostName]));
}

void TestRunner::display()
{
    displayWebView();
}

void TestRunner::displayAndTrackRepaints()
{
    displayAndTrackRepaintsWebView();
}

void TestRunner::keepWebHistory()
{
    if (![WebHistory optionalSharedHistory])
        [WebHistory setOptionalSharedHistory:adoptNS([[WebHistory alloc] init]).get()];
}

int TestRunner::numberOfPendingGeolocationPermissionRequests()
{
    return [(UIDelegate *)[[mainFrame webView] UIDelegate] numberOfPendingGeolocationPermissionRequests];
}

bool TestRunner::isGeolocationProviderActive()
{
    return MockGeolocationProvider.shared.isActive;
}

size_t TestRunner::webHistoryItemCount()
{
    return [[[WebHistory optionalSharedHistory] allItems] count];
}

void TestRunner::notifyDone()
{
    if (m_waitToDump) {
        m_waitToDump = false;
        if (!topLoadingFrame && !DRT::WorkQueue::singleton().count())
            dump();
    } else
        fprintf(stderr, "TestRunner::notifyDone() called unexpectedly.");
}

void TestRunner::stopLoading()
{
    [mainFrame.webView stopLoading:nil];
}

void TestRunner::forceImmediateCompletion()
{
    if (m_waitToDump) {
        m_waitToDump = false;
        if (!DRT::WorkQueue::singleton().count())
            dump();
    } else
        fprintf(stderr, "TestRunner::forceImmediateCompletion() called unexpectedly.");
}

static inline std::string stringFromJSString(JSStringRef jsString)
{
    size_t maxBufferSize = JSStringGetMaximumUTF8CStringSize(jsString);
    char* utf8Buffer = new char[maxBufferSize];
    size_t bytesWrittenToUTF8Buffer = JSStringGetUTF8CString(jsString, utf8Buffer, maxBufferSize);
    std::string stdString(utf8Buffer, bytesWrittenToUTF8Buffer - 1); // bytesWrittenToUTF8Buffer includes a trailing \0 which std::string doesn't need.
    delete[] utf8Buffer;
    return stdString;
}

static inline size_t indexOfSeparatorAfterDirectoryName(const std::string& directoryName, const std::string& fullPath)
{
    std::string searchKey = "/" + directoryName + "/";
    size_t indexOfSearchKeyStart = fullPath.rfind(searchKey);
    if (indexOfSearchKeyStart == std::string::npos) {
        ASSERT_NOT_REACHED();
        return 0;
    }
    // Callers expect the return value not to end in "/", so searchKey.length() - 1.
    return indexOfSearchKeyStart + searchKey.length() - 1;
}

static inline std::string resourceRootAbsolutePath(const std::string& testURL, const std::string& expectedRootName)
{
    char* localResourceRootEnv = getenv("LOCAL_RESOURCE_ROOT");
    if (localResourceRootEnv)
        return std::string(localResourceRootEnv);

    // This fallback approach works for non-http tests and is useful
    // in the case when we're running DRT directly from the command line.
    return testURL.substr(0, indexOfSeparatorAfterDirectoryName(expectedRootName, testURL));
}

JSRetainPtr<JSStringRef> TestRunner::pathToLocalResource(JSContextRef context, JSStringRef localResourceJSString)
{
    // The passed in path will be an absolute path to the resource starting
    // with "/tmp" or "/tmp/LayoutTests", optionally starting with the explicit file:// protocol.
    // /tmp maps to DUMPRENDERTREE_TEMP, and /tmp/LayoutTests maps to LOCAL_RESOURCE_ROOT.
    // FIXME: This code should work on all *nix platforms and can be moved into TestRunner.cpp.
    std::string expectedRootName;
    std::string absolutePathToResourceRoot;
    std::string localResourceString = stringFromJSString(localResourceJSString);

    if (localResourceString.find("LayoutTests") != std::string::npos) {
        expectedRootName = "LayoutTests";
        absolutePathToResourceRoot = resourceRootAbsolutePath(m_testURL, expectedRootName);
    } else if (localResourceString.find("tmp") != std::string::npos) {
        expectedRootName = "tmp";
        absolutePathToResourceRoot = getenv("DUMPRENDERTREE_TEMP");
    } else {
        ASSERT_NOT_REACHED(); // pathToLocalResource was passed a path it doesn't know how to map.
    }
    ASSERT(!absolutePathToResourceRoot.empty());
    size_t indexOfSeparatorAfterRootName = indexOfSeparatorAfterDirectoryName(expectedRootName, localResourceString);
    std::string absolutePathToLocalResource = absolutePathToResourceRoot + localResourceString.substr(indexOfSeparatorAfterRootName);

    // Note: It's important that we keep the file:// or http tests will get confused.
    if (localResourceString.find("file://") != std::string::npos) {
        ASSERT(absolutePathToLocalResource[0] == '/');
        absolutePathToLocalResource = std::string("file://") + absolutePathToLocalResource;
    }
    return adopt(JSStringCreateWithUTF8CString(absolutePathToLocalResource.c_str()));
}

void TestRunner::queueLoad(JSStringRef url, JSStringRef target)
{
    RetainPtr<CFStringRef> urlCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, url));
    NSString *urlNS = (__bridge NSString *)urlCF.get();

    NSURL *nsurl = [NSURL URLWithString:urlNS relativeToURL:[[[mainFrame dataSource] response] URL]];
    NSString *nsurlString = [nsurl absoluteString];

    auto absoluteURL = adopt(JSStringCreateWithUTF8CString([nsurlString UTF8String]));
    DRT::WorkQueue::singleton().queue(new LoadItem(absoluteURL.get(), target));
}

void TestRunner::setAcceptsEditing(bool newAcceptsEditing)
{
    [(EditingDelegate *)[[mainFrame webView] editingDelegate] setAcceptsEditing:newAcceptsEditing];
}

void TestRunner::setAlwaysAcceptCookies(bool alwaysAcceptCookies)
{
    if (alwaysAcceptCookies == m_alwaysAcceptCookies)
        return;

    m_alwaysAcceptCookies = alwaysAcceptCookies;
    NSHTTPCookieAcceptPolicy cookieAcceptPolicy = alwaysAcceptCookies ? NSHTTPCookieAcceptPolicyAlways : NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
    [WebPreferences _setCurrentNetworkLoaderSessionCookieAcceptPolicy:cookieAcceptPolicy];
}

void TestRunner::setOnlyAcceptFirstPartyCookies(bool onlyAcceptFirstPartyCookies)
{
    if (onlyAcceptFirstPartyCookies)
        m_alwaysAcceptCookies = NO;

    NSHTTPCookieAcceptPolicy cookieAcceptPolicy = onlyAcceptFirstPartyCookies ? static_cast<NSHTTPCookieAcceptPolicy>(NSHTTPCookieAcceptPolicyExclusivelyFromMainDocumentDomain) : NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
    [WebPreferences _setCurrentNetworkLoaderSessionCookieAcceptPolicy:cookieAcceptPolicy];
}

void TestRunner::setAppCacheMaximumSize(unsigned long long size)
{
    [WebApplicationCache setMaximumSize:size];
}

void TestRunner::setCustomPolicyDelegate(bool setDelegate, bool permissive)
{
    if (!setDelegate) {
        [[mainFrame webView] setPolicyDelegate:defaultPolicyDelegate.get()];
        return;
    }

    [policyDelegate setPermissive:permissive];
    [[mainFrame webView] setPolicyDelegate:policyDelegate.get()];
}

void TestRunner::setDatabaseQuota(unsigned long long quota)
{    
    auto origin = adoptNS([[WebSecurityOrigin alloc] initWithURL:[NSURL URLWithString:@"file:///"]]);
    [[origin databaseQuotaManager] setQuota:quota];
}

void TestRunner::goBack()
{
    [[mainFrame webView] goBack];
}

void TestRunner::setDefersLoading(bool defers)
{
    [[mainFrame webView] setDefersCallbacks:defers];
}

void TestRunner::setDomainRelaxationForbiddenForURLScheme(bool forbidden, JSStringRef scheme)
{
    RetainPtr<CFStringRef> schemeCFString = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, scheme));
    [WebView _setDomainRelaxationForbidden:forbidden forURLScheme:(__bridge NSString *)schemeCFString.get()];
}

void TestRunner::setMockDeviceOrientation(bool canProvideAlpha, double alpha, bool canProvideBeta, double beta, bool canProvideGamma, double gamma)
{
    // DumpRenderTree configured the WebView to use WebDeviceOrientationProviderMock.
    id<WebDeviceOrientationProvider> provider = [[mainFrame webView] _deviceOrientationProvider];
    WebDeviceOrientationProviderMock *mockProvider = static_cast<WebDeviceOrientationProviderMock*>(provider);
    auto orientation = adoptNS([[WebDeviceOrientation alloc] initWithCanProvideAlpha:canProvideAlpha alpha:alpha canProvideBeta:canProvideBeta beta:beta canProvideGamma:canProvideGamma gamma:gamma]);
    [mockProvider setOrientation:orientation.get()];
}

void TestRunner::setMockGeolocationPosition(double latitude, double longitude, double accuracy, bool providesAltitude, double altitude, bool providesAltitudeAccuracy, double altitudeAccuracy, bool providesHeading, double heading, bool providesSpeed, double speed, bool providesFloorLevel, double floorLevel)
{
    RetainPtr<WebGeolocationPosition> position;
    if (!providesAltitude && !providesAltitudeAccuracy && !providesHeading && !providesSpeed) {
        // Test the exposed API.
        position = adoptNS([[WebGeolocationPosition alloc] initWithTimestamp:WallTime::now().secondsSinceEpoch().seconds() latitude:latitude longitude:longitude accuracy:accuracy]);
    } else {
        WebCore::GeolocationPositionData geolocationPosition { WallTime::now().secondsSinceEpoch().seconds(), latitude, longitude, accuracy };
        if (providesAltitude)
            geolocationPosition.altitude = altitude;
        if (providesAltitudeAccuracy)
            geolocationPosition.altitudeAccuracy = altitudeAccuracy;
        if (providesHeading)
            geolocationPosition.heading = heading;
        if (providesSpeed)
            geolocationPosition.speed = speed;
        if (providesFloorLevel)
            geolocationPosition.floorLevel = floorLevel;
        position = adoptNS([[WebGeolocationPosition alloc] initWithGeolocationPosition:(WTFMove(geolocationPosition))]);
    }
    [[MockGeolocationProvider shared] setPosition:position.get()];
}

void TestRunner::setMockGeolocationPositionUnavailableError(JSStringRef message)
{
    RetainPtr<CFStringRef> messageCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, message));
    NSString *messageNS = (__bridge NSString *)messageCF.get();
    [[MockGeolocationProvider shared] setPositionUnavailableErrorWithMessage:messageNS];
}

void TestRunner::setGeolocationPermission(bool allow)
{
    setGeolocationPermissionCommon(allow);
    [(UIDelegate *)[[mainFrame webView] UIDelegate] didSetMockGeolocationPermission];
}

void TestRunner::setIconDatabaseEnabled(bool iconDatabaseEnabled)
{
    [WebView _setIconLoadingEnabled:iconDatabaseEnabled];
}

void TestRunner::setMainFrameIsFirstResponder(bool flag)
{
#if !PLATFORM(IOS_FAMILY)
    NSView *documentView = [[mainFrame frameView] documentView];
    
    NSResponder *firstResponder = flag ? documentView : nil;
    [[[mainFrame webView] window] makeFirstResponder:firstResponder];
#endif
}

void TestRunner::setPrivateBrowsingEnabled(bool privateBrowsingEnabled)
{
    [[[mainFrame webView] preferences] setPrivateBrowsingEnabled:privateBrowsingEnabled];
}

void TestRunner::setAutomaticLinkDetectionEnabled(bool enabled)
{
#if !PLATFORM(IOS_FAMILY)
    [[mainFrame webView] setAutomaticLinkDetectionEnabled:enabled];
#endif
}

void TestRunner::setTabKeyCyclesThroughElements(bool cycles)
{
    [[mainFrame webView] setTabKeyCyclesThroughElements:cycles];
}

#if PLATFORM(IOS_FAMILY)
void TestRunner::setPagePaused(bool paused)
{
    [gWebBrowserView setPaused:paused];
}
#endif

void TestRunner::setUserStyleSheetEnabled(bool flag)
{
    [[[mainFrame webView] preferences] setUserStyleSheetEnabled:flag];
}

void TestRunner::setUserStyleSheetLocation(JSStringRef path)
{
    RetainPtr<CFStringRef> pathCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, path));
    NSURL *url = [NSURL URLWithString:(__bridge NSString *)pathCF.get()];
    [[[mainFrame webView] preferences] setUserStyleSheetLocation:url];
}

void TestRunner::setValueForUser(JSContextRef context, JSValueRef nodeObject, JSStringRef value)
{
    DOMElement *element = [DOMElement _DOMElementFromJSContext:context value:nodeObject];
    if (!element || ![element isKindOfClass:[DOMHTMLInputElement class]])
        return;

    RetainPtr<CFStringRef> valueCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, value));
    [(DOMHTMLInputElement *)element setValueForUser:(__bridge NSString *)valueCF.get()];
}

void TestRunner::dispatchPendingLoadRequests()
{
    [[mainFrame webView] _dispatchPendingLoadRequests];
}

void TestRunner::removeAllCookies()
{
    [WebPreferences _clearNetworkLoaderSession];
}

void TestRunner::removeAllVisitedLinks()
{
    [WebHistory _removeAllVisitedLinks];
}

void TestRunner::setPersistentUserStyleSheetLocation(JSStringRef jsURL)
{
    RetainPtr<CFStringRef> urlString = adoptCF(JSStringCopyCFString(0, jsURL));
    ::setPersistentUserStyleSheetLocation(urlString.get());
}

void TestRunner::clearPersistentUserStyleSheet()
{
    ::setPersistentUserStyleSheetLocation(0);
}

void TestRunner::setWindowIsKey(bool windowIsKey)
{
    m_windowIsKey = windowIsKey;
    [[mainFrame webView] _updateActiveState];
}

void TestRunner::setViewSize(double width, double height)
{
    [[mainFrame webView] setFrameSize:NSMakeSize(width, height)];
}

static void waitUntilDoneWatchdogFired(CFRunLoopTimerRef timer, void* info)
{
    gTestRunner->waitToDumpWatchdogTimerFired();
}

void TestRunner::setWaitToDump(bool waitUntilDone)
{
    m_waitToDump = waitUntilDone;
    if (m_waitToDump && m_timeout && shouldSetWaitToDumpWatchdog())
        setWaitToDumpWatchdog(adoptCF(CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + m_timeout / 1000.0, 0, 0, 0, waitUntilDoneWatchdogFired, NULL)));
}

int TestRunner::windowCount()
{
    return CFArrayGetCount(openWindowsRef);
}

void TestRunner::execCommand(JSStringRef name, JSStringRef value)
{
    RetainPtr<CFStringRef> nameCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, name));
    NSString *nameNS = (__bridge NSString *)nameCF.get();

    RetainPtr<CFStringRef> valueCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, value));
    NSString *valueNS = (__bridge NSString *)valueCF.get();

    [[mainFrame webView] _executeCoreCommandByName:nameNS value:valueNS];
}

bool TestRunner::findString(JSContextRef context, JSStringRef target, JSObjectRef optionsArray)
{
    WebFindOptions options = 0;

    auto length = WTR::arrayLength(context, optionsArray);
    auto targetCFString = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, target));

    for (unsigned i = 0; i < length; ++i) {
        JSValueRef value = JSObjectGetPropertyAtIndex(context, optionsArray, i, 0);
        if (!JSValueIsString(context, value))
            continue;

        auto optionName = adopt(JSValueToStringCopy(context, value, nullptr));

        if (JSStringIsEqualToUTF8CString(optionName.get(), "CaseInsensitive"))
            options |= WebFindOptionsCaseInsensitive;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "AtWordStarts"))
            options |= WebFindOptionsAtWordStarts;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "TreatMedialCapitalAsWordStart"))
            options |= WebFindOptionsTreatMedialCapitalAsWordStart;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "Backwards"))
            options |= WebFindOptionsBackwards;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "WrapAround"))
            options |= WebFindOptionsWrapAround;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "StartInSelection"))
            options |= WebFindOptionsStartInSelection;
    }

    return [[mainFrame webView] findString:(__bridge NSString *)targetCFString.get() options:options];
}

void TestRunner::setCacheModel(int cacheModel)
{
    [[[mainFrame webView] preferences] setCacheModel:(WebCacheModel)cacheModel];
}

bool TestRunner::isCommandEnabled(JSStringRef name)
{
#if !PLATFORM(IOS_FAMILY)
    RetainPtr<CFStringRef> nameCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, name));
    NSString *nameNS = (__bridge NSString *)nameCF.get();

    // Accept command strings with capital letters for first letter without trailing colon.
    if (![nameNS hasSuffix:@":"] && [nameNS length]) {
        nameNS = [[[[nameNS substringToIndex:1] lowercaseString]
            stringByAppendingString:[nameNS substringFromIndex:1]]
            stringByAppendingString:@":"];
    }

    SEL selector = NSSelectorFromString(nameNS);
    RetainPtr<CommandValidationTarget> target = adoptNS([[CommandValidationTarget alloc] initWithAction:selector]);
    id validator = [NSApp targetForAction:selector to:[mainFrame webView] from:target.get()];
    if (!validator)
        return false;
    if (![validator respondsToSelector:selector])
        return false;
    if (![validator respondsToSelector:@selector(validateUserInterfaceItem:)])
        return true;
    return [validator validateUserInterfaceItem:target.get()];
#else
    return false;
#endif
}

void TestRunner::waitForPolicyDelegate()
{
    setWaitToDump(true);
    [policyDelegate setControllerToNotifyDone:this];
    [[mainFrame webView] setPolicyDelegate:policyDelegate.get()];
}

void TestRunner::addOriginAccessAllowListEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains)
{
    RetainPtr<CFStringRef> sourceOriginCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, sourceOrigin));
    NSString *sourceOriginNS = (__bridge NSString *)sourceOriginCF.get();
    RetainPtr<CFStringRef> protocolCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, destinationProtocol));
    NSString *destinationProtocolNS = (__bridge NSString *)protocolCF.get();
    RetainPtr<CFStringRef> hostCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, destinationHost));
    NSString *destinationHostNS = (__bridge NSString *)hostCF.get();
    [WebView _addOriginAccessAllowListEntryWithSourceOrigin:sourceOriginNS destinationProtocol:destinationProtocolNS destinationHost:destinationHostNS allowDestinationSubdomains:allowDestinationSubdomains];
}

void TestRunner::removeOriginAccessAllowListEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains)
{
    RetainPtr<CFStringRef> sourceOriginCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, sourceOrigin));
    NSString *sourceOriginNS = (__bridge NSString *)sourceOriginCF.get();
    RetainPtr<CFStringRef> protocolCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, destinationProtocol));
    NSString *destinationProtocolNS = (__bridge NSString *)protocolCF.get();
    RetainPtr<CFStringRef> hostCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, destinationHost));
    NSString *destinationHostNS = (__bridge NSString *)hostCF.get();
    [WebView _removeOriginAccessAllowListEntryWithSourceOrigin:sourceOriginNS destinationProtocol:destinationProtocolNS destinationHost:destinationHostNS allowDestinationSubdomains:allowDestinationSubdomains];
}

void TestRunner::setScrollbarPolicy(JSStringRef orientation, JSStringRef policy)
{
    // FIXME: implement
}

void TestRunner::addUserScript(JSStringRef source, bool runAtStart, bool allFrames)
{
    RetainPtr<CFStringRef> sourceCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, source));
    NSString *sourceNS = (__bridge NSString *)sourceCF.get();
    [WebView _addUserScriptToGroup:@"org.webkit.DumpRenderTree" world:[WebScriptWorld world] source:sourceNS url:nil includeMatchPatternStrings:nil excludeMatchPatternStrings:nil injectionTime:(runAtStart ? WebInjectAtDocumentStart : WebInjectAtDocumentEnd) injectedFrames:(allFrames ? WebInjectInAllFrames : WebInjectInTopFrameOnly)];
}

void TestRunner::addUserStyleSheet(JSStringRef source, bool allFrames)
{
    RetainPtr<CFStringRef> sourceCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, source));
    NSString *sourceNS = (__bridge NSString *)sourceCF.get();
    [WebView _addUserStyleSheetToGroup:@"org.webkit.DumpRenderTree" world:[WebScriptWorld world] source:sourceNS url:nil includeMatchPatternStrings:nil excludeMatchPatternStrings:nil injectedFrames:(allFrames ? WebInjectInAllFrames : WebInjectInTopFrameOnly)];
}

void TestRunner::showWebInspector()
{
    [[[mainFrame webView] inspector] show:nil];
}

void TestRunner::closeWebInspector()
{
    [[[mainFrame webView] inspector] close:nil];
}

void TestRunner::evaluateInWebInspector(JSStringRef script)
{
    RetainPtr<CFStringRef> scriptCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, script));
    NSString *scriptNS = (__bridge NSString *)scriptCF.get();
    [[[mainFrame webView] inspector] evaluateInFrontend:nil script:scriptNS];
}

JSRetainPtr<JSStringRef> TestRunner::inspectorTestStubURL()
{
#if PLATFORM(IOS_FAMILY)
    return nullptr;
#else
    CFBundleRef inspectorBundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.WebInspectorUI"));
    if (!inspectorBundle)
        return nullptr;

    RetainPtr<CFURLRef> url = adoptCF(CFBundleCopyResourceURL(inspectorBundle, CFSTR("TestStub"), CFSTR("html"), NULL));
    if (!url)
        return nullptr;

    CFStringRef urlString = CFURLGetString(url.get());
    return adopt(JSStringCreateWithCFString(urlString));
#endif
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
        if (it->value == world)
            return it->key;
    }

    return 0;
}

void TestRunner::evaluateScriptInIsolatedWorldAndReturnValue(unsigned worldID, JSObjectRef globalObject, JSStringRef script)
{
    // FIXME: Implement this.
}

void TestRunner::evaluateScriptInIsolatedWorld(unsigned worldID, JSObjectRef globalObject, JSStringRef script)
{
    RetainPtr<CFStringRef> scriptCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, script));
    NSString *scriptNS = (__bridge NSString *)scriptCF.get();

    // A worldID of 0 always corresponds to a new world. Any other worldID corresponds to a world
    // that is created once and cached forever.
    WebScriptWorld *world;
    if (!worldID)
        world = [WebScriptWorld world];
    else {
        RetainPtr<WebScriptWorld>& worldSlot = worldMap().add(worldID, nullptr).iterator->value;
        if (!worldSlot)
            worldSlot = adoptNS([[WebScriptWorld alloc] init]);
        world = worldSlot.get();
    }

    [mainFrame _stringByEvaluatingJavaScriptFromString:scriptNS withGlobalObject:globalObject inScriptWorld:world];
}

@interface APITestDelegate : NSObject <WebFrameLoadDelegate>
{
    bool* m_condition;
}
@end

@implementation APITestDelegate

- (id)initWithCompletionCondition:(bool*)condition
{
    self = [super init];
    if (!self)
        return nil;
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

#if PLATFORM(IOS_FAMILY)

@interface APITestDelegateIPhone : NSObject <WebFrameLoadDelegate>
{
    TestRunner* testRunner;
    RetainPtr<NSData> data;
    RetainPtr<NSURL> baseURL;
    RetainPtr<WebView> webView;
}
- (id)initWithTestRunner:(TestRunner*)testRunner utf8Data:(JSStringRef)data baseURL:(JSStringRef)baseURL;
- (void)run;
@end

@implementation APITestDelegateIPhone

- (id)initWithTestRunner:(TestRunner*)runner utf8Data:(JSStringRef)dataString baseURL:(JSStringRef)baseURLString
{
    self = [super init];
    if (!self)
        return nil;

    testRunner = runner;
    data = [(__bridge NSString *)adoptCF(JSStringCopyCFString(kCFAllocatorDefault, dataString)).get() dataUsingEncoding:NSUTF8StringEncoding];
    baseURL = [NSURL URLWithString:(__bridge NSString *)adoptCF(JSStringCopyCFString(kCFAllocatorDefault, baseURLString)).get()];
    return self;
}

- (void)dealloc
{
    [super dealloc];
}

- (void)run
{
    if (webView)
        return;

    testRunner->setWaitToDump(true);

    WebThreadLock();

    webView = adoptNS([[WebView alloc] initWithFrame:NSZeroRect frameName:@"" groupName:@""]);
    [webView setFrameLoadDelegate:self];
    [[webView mainFrame] loadData:data.get() MIMEType:@"text/html" textEncodingName:@"utf-8" baseURL:baseURL.get()];
}

- (void)_cleanUp
{
    if (!webView)
        return;

    WebThreadLock();

    [webView _clearDelegates];
    [webView close];
    webView = nil;

    testRunner->notifyDone();
}

- (void)webView:(WebView *)sender didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    printf("API Test load failed\n");
    [self _cleanUp];
}

- (void)webView:(WebView *)sender didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    printf("API Test load failed provisional\n");
    [self _cleanUp];
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    printf("API Test load succeeded\n");
    [self _cleanUp];
}

@end

#endif

void TestRunner::apiTestNewWindowDataLoadBaseURL(JSStringRef utf8Data, JSStringRef baseURL)
{
#if PLATFORM(IOS_FAMILY)
    // On iOS this gets called via JavaScript on the WebThread. But since it creates
    // and closes a WebView, it should be run on the main thread. Make the switch
    // from the web thread to the main thread and make the test asynchronous.
    if (WebThreadIsCurrent()) {
        auto dispatcher = adoptNS([[APITestDelegateIPhone alloc] initWithTestRunner:this utf8Data:utf8Data baseURL:baseURL]);
        NSInvocation *invocation = WebThreadMakeNSInvocation(dispatcher.get(), @selector(run));
        WebThreadCallDelegate(invocation);
        return;
    }
#endif

    @autoreleasepool {
        RetainPtr<CFStringRef> utf8DataCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, utf8Data));
        RetainPtr<CFStringRef> baseURLCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, baseURL));

        auto webView = adoptNS([[WebView alloc] initWithFrame:NSZeroRect frameName:@"" groupName:@""]);

        bool done = false;
        auto delegate = adoptNS([[APITestDelegate alloc] initWithCompletionCondition:&done]);
        [webView setFrameLoadDelegate:delegate.get()];

        [[webView mainFrame] loadData:[(__bridge NSString *)utf8DataCF.get() dataUsingEncoding:NSUTF8StringEncoding] MIMEType:@"text/html" textEncodingName:@"utf-8" baseURL:[NSURL URLWithString:(__bridge NSString *)baseURLCF.get()]];

        while (!done) {
            @autoreleasepool {
                [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
            }
        }

#if PLATFORM(IOS_FAMILY)
        [(DumpRenderTree *)[UIApplication sharedApplication] _waitForWebThread];
#endif

        [webView close];
    }
}

void TestRunner::apiTestGoToCurrentBackForwardItem()
{
    WebView *view = [mainFrame webView];
    [view goToBackForwardItem:[[view backForwardList] currentItem]];
}

static NSString *SynchronousLoaderRunLoopMode = @"DumpRenderTreeSynchronousLoaderRunLoopMode";

@interface SynchronousLoader : NSObject <NSURLConnectionDelegate>
{
    RetainPtr<NSString> m_username;
    RetainPtr<NSString> m_password;
    BOOL m_isDone;
}
+ (void)makeRequest:(NSURLRequest *)request withUsername:(NSString *)username password:(NSString *)password;
@end

@implementation SynchronousLoader : NSObject
- (void)dealloc
{
    [super dealloc];
}

- (BOOL)connectionShouldUseCredentialStorage:(NSURLConnection *)connection
{
    return YES;
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)connection:(NSURLConnection *)connection didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
IGNORE_WARNINGS_END
{
    if ([challenge previousFailureCount] == 0) {
        RetainPtr<NSURLCredential> credential = adoptNS([[NSURLCredential alloc]  initWithUser:m_username.get() password:m_password.get() persistence:NSURLCredentialPersistenceForSession]);
        [[challenge sender] useCredential:credential.get() forAuthenticationChallenge:challenge];
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

    auto delegate = adoptNS([[SynchronousLoader alloc] init]);
    delegate.get()->m_username = adoptNS([username copy]);
    delegate.get()->m_password = adoptNS([password copy]);

    auto connection = adoptNS([[NSURLConnection alloc] initWithRequest:request delegate:delegate.get() startImmediately:NO]);
    [connection scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:SynchronousLoaderRunLoopMode];
    [connection start];
    
    while (!delegate.get()->m_isDone)
        [[NSRunLoop currentRunLoop] runMode:SynchronousLoaderRunLoopMode beforeDate:[NSDate distantFuture]];

    [connection cancel];
}

@end

void TestRunner::authenticateSession(JSStringRef url, JSStringRef username, JSStringRef password)
{
    // See <rdar://problem/7880699>.
    RetainPtr<CFStringRef> urlStringCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, url));
    RetainPtr<CFStringRef> usernameCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, username));
    RetainPtr<CFStringRef> passwordCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, password));

    RetainPtr<NSURLRequest> request = adoptNS([[NSURLRequest alloc] initWithURL:[NSURL URLWithString:(__bridge NSString *)urlStringCF.get()]]);

    [SynchronousLoader makeRequest:request.get() withUsername:(__bridge NSString *)usernameCF.get() password:(__bridge NSString *)passwordCF.get()];
}

void TestRunner::abortModal()
{
#if !PLATFORM(IOS_FAMILY)
    [NSApp abortModal];
#endif
}

void TestRunner::setSerializeHTTPLoads(bool serialize)
{
    [WebView _setLoadResourcesSerially:serialize];
}

void TestRunner::setTextDirection(JSStringRef directionName)
{
#if !PLATFORM(IOS_FAMILY)
    if (JSStringIsEqualToUTF8CString(directionName, "ltr"))
        [[mainFrame webView] makeBaseWritingDirectionLeftToRight:0];
    else if (JSStringIsEqualToUTF8CString(directionName, "rtl"))
        [[mainFrame webView] makeBaseWritingDirectionRightToLeft:0];
    else
        ASSERT_NOT_REACHED();
#endif
}

void TestRunner::addChromeInputField()
{
#if !PLATFORM(IOS_FAMILY)
    auto textField = adoptNS([[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 100, 20)]);
    [textField setTag:1];
    [[[[mainFrame webView] window] contentView] addSubview:textField.get()];
    
    [textField setNextKeyView:[mainFrame webView]];
    [[mainFrame webView] setNextKeyView:textField.get()];
#endif
}

void TestRunner::removeChromeInputField()
{
#if !PLATFORM(IOS_FAMILY)
    NSView* textField = [[[[mainFrame webView] window] contentView] viewWithTag:1];
    if (textField) {
        [textField removeFromSuperview];
        focusWebView();
    }
#endif
}

void TestRunner::setTextInChromeInputField(const String&)
{
}

void TestRunner::selectChromeInputField()
{
}

String TestRunner::getSelectedTextInChromeInputField()
{
    return { };
}

void TestRunner::focusWebView()
{
#if !PLATFORM(IOS_FAMILY)
    [[[mainFrame webView] window] makeFirstResponder:[mainFrame webView]];
#endif
}

void TestRunner::setBackingScaleFactor(double backingScaleFactor)
{
#if !PLATFORM(IOS_FAMILY)
    [[mainFrame webView] _setCustomBackingScaleFactor:backingScaleFactor];
#endif
}

void TestRunner::resetPageVisibility()
{
    WebView *webView = [mainFrame webView];
    if ([webView respondsToSelector:@selector(_setVisibilityState:isInitialState:)])
        [webView _setVisibilityState:WebPageVisibilityStateVisible isInitialState:YES];
}

void TestRunner::setPageVisibility(const char* newVisibility)
{
    if (!newVisibility)
        return;

    WebView *webView = [mainFrame webView];
    if (!strcmp(newVisibility, "visible"))
        [webView _setVisibilityState:WebPageVisibilityStateVisible isInitialState:NO];
    else if (!strcmp(newVisibility, "hidden"))
        [webView _setVisibilityState:WebPageVisibilityStateHidden isInitialState:NO];
    else if (!strcmp(newVisibility, "prerender"))
        [webView _setVisibilityState:WebPageVisibilityStatePrerender isInitialState:NO];
}

void TestRunner::grantWebNotificationPermission(JSStringRef jsOrigin)
{
    RetainPtr<CFStringRef> cfOrigin = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, jsOrigin));
    ASSERT([[mainFrame webView] _notificationProvider] == [MockWebNotificationProvider shared]);
    [[MockWebNotificationProvider shared] setWebNotificationOrigin:(__bridge NSString *)cfOrigin.get() permission:TRUE];
}

void TestRunner::denyWebNotificationPermission(JSStringRef jsOrigin)
{
    RetainPtr<CFStringRef> cfOrigin = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, jsOrigin));
    ASSERT([[mainFrame webView] _notificationProvider] == [MockWebNotificationProvider shared]);
    [[MockWebNotificationProvider shared] setWebNotificationOrigin:(__bridge NSString *)cfOrigin.get() permission:FALSE];
}

void TestRunner::removeAllWebNotificationPermissions()
{
    [[MockWebNotificationProvider shared] removeAllWebNotificationPermissions];
}

void TestRunner::simulateWebNotificationClick(JSValueRef jsNotification)
{
    NSString *notificationID = [[mainFrame webView] _notificationIDForTesting:jsNotification];
    m_hasPendingWebNotificationClick = true;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!m_hasPendingWebNotificationClick)
            return;

        [[MockWebNotificationProvider shared] simulateWebNotificationClick:notificationID];
        m_hasPendingWebNotificationClick = false;
    });
}

void TestRunner::simulateLegacyWebNotificationClick(JSStringRef jsTitle)
{
}

static NSString * const WebSubresourcesKey = @"WebSubresources";
static NSString * const WebSubframeArchivesKey = @"WebResourceMIMEType like 'image*'";

unsigned TestRunner::imageCountInGeneralPasteboard() const
{
    NSString *webArchivePboardType = @"Apple Web Archive pasteboard type";
#if PLATFORM(MAC)
    NSData *data = [[NSPasteboard generalPasteboard] dataForType:webArchivePboardType];
#elif PLATFORM(IOS_FAMILY)
    NSData *data = [[UIPasteboard generalPasteboard] valueForPasteboardType:webArchivePboardType];
#endif
    if (!data)
        return 0;
    
    NSError *error = nil;
    id webArchive = [NSPropertyListSerialization propertyListWithData:data options:NSPropertyListImmutable format:NULL error:&error];
    if (error) {
        NSLog(@"Encountered error while serializing Web Archive pasteboard data: %@", error);
        return 0;
    }
    
    NSArray *subItems = [NSArray arrayWithArray:[webArchive objectForKey:WebSubresourcesKey]];
    NSPredicate *predicate = [NSPredicate predicateWithFormat:WebSubframeArchivesKey];
    NSArray *imagesArray = [subItems filteredArrayUsingPredicate:predicate];
    
    if (!imagesArray)
        return 0;
    
    return imagesArray.count;
}


void TestRunner::generateTestReport(JSStringRef message, JSStringRef group)
{
    ASSERT(message);
    auto messageCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, message));
    RetainPtr<CFStringRef> groupCF;
    if (group)
        groupCF = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, group));
    [mainFrame _generateTestReport:(__bridge NSString *)messageCF.get() withGroup:(__bridge NSString *)groupCF.get()];
}

#if PLATFORM(MAC)

bool TestRunner::isSecureEventInputEnabled() const
{
    return dynamic_objc_cast<WebHTMLView>(mainFrame.frameView.documentView)._secureEventInputEnabledForTesting;
}

#endif // PLATFORM(MAC)
