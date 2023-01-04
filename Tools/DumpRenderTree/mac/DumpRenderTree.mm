/*
 * Copyright (C) 2005-2022 Apple Inc. All rights reserved.
 *           (C) 2007 Graham Dennis (graham.dennis@gmail.com)
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

#import "AccessibilityController.h"
#import "DefaultPolicyDelegate.h"
#import "DumpRenderTreeDraggingInfo.h"
#import "DumpRenderTreePasteboard.h"
#import "DumpRenderTreeWindow.h"
#import "EditingDelegate.h"
#import "EventSendingController.h"
#import "FrameLoadDelegate.h"
#import "HistoryDelegate.h"
#import "JavaScriptThreading.h"
#import "LayoutTestSpellChecker.h"
#import "MockGeolocationProvider.h"
#import "MockWebNotificationProvider.h"
#import "NavigationController.h"
#import "ObjCPlugin.h"
#import "ObjCPluginFunction.h"
#import "PixelDumpSupport.h"
#import "PolicyDelegate.h"
#import "PoseAsClass.h"
#import "ResourceLoadDelegate.h"
#import "TestCommand.h"
#import "TestFeatures.h"
#import "TestOptions.h"
#import "TestRunner.h"
#import "UIDelegate.h"
#import "WebArchiveDumpSupport.h"
#import "WebCoreTestSupport.h"
#import "WorkQueue.h"
#import "WorkQueueItem.h"
#import <CoreFoundation/CoreFoundation.h>
#import <JavaScriptCore/InitializeThreading.h>
#import <JavaScriptCore/JSCConfig.h>
#import <JavaScriptCore/Options.h>
#import <JavaScriptCore/TestRunnerUtils.h>
#import <WebCore/LogInitialization.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebKit/DOMElement.h>
#import <WebKit/DOMExtensions.h>
#import <WebKit/DOMRange.h>
#import <WebKit/WebArchive.h>
#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebCache.h>
#import <WebKit/WebCoreStatistics.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDatabaseManagerPrivate.h>
#import <WebKit/WebDeviceOrientationProviderMock.h>
#import <WebKit/WebDocumentPrivate.h>
#import <WebKit/WebEditingDelegate.h>
#import <WebKit/WebFeature.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebHistory.h>
#import <WebKit/WebHistoryItemPrivate.h>
#import <WebKit/WebInspector.h>
#import <WebKit/WebKitNSStringExtras.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebPreferenceKeysPrivate.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebStorageManagerPrivate.h>
#import <WebKit/WebView.h>
#import <WebKit/WebViewPrivate.h>
#import <getopt.h>
#import <objc/runtime.h>
#import <wtf/Assertions.h>
#import <wtf/FastMalloc.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/RetainPtr.h>
#import <wtf/Threading.h>
#import <wtf/UniqueArray.h>
#import <wtf/WorkQueue.h>
#import <wtf/cocoa/CrashReporter.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/StringBuilder.h>
#import <wtf/text/WTFString.h>

#if !PLATFORM(IOS_FAMILY)
#import <Carbon/Carbon.h>
#import <WebKit/WebDynamicScrollBarsView.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import "DumpRenderTreeBrowserView.h"
#import "IOSLayoutTestCommunication.h"
#import "UIKitSPI.h"
#import <QuartzCore/QuartzCore.h>
#import <WebKit/WAKWindow.h>
#import <WebKit/WebCoreThread.h>
#import <WebKit/WebCoreThreadRun.h>
#import <WebKit/WebDOMOperations.h>
#import <fcntl.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#endif

extern "C" {
#import <mach-o/getsect.h>
}

static RetainPtr<NSString> toNS(const std::string& string)
{
    return adoptNS([[NSString alloc] initWithUTF8String:string.c_str()]);
}

#if !PLATFORM(IOS_FAMILY)
@interface DumpRenderTreeApplication : NSApplication
@end

@interface DumpRenderTreeEvent : NSEvent
@end
#else
@interface ScrollViewResizerDelegate : NSObject
@end

@implementation ScrollViewResizerDelegate
- (void)view:(UIWebDocumentView *)view didSetFrame:(CGRect)newFrame oldFrame:(CGRect)oldFrame asResultOfZoom:(BOOL)wasResultOfZoom
{
    UIView *scrollView = [view superview];
    while (![scrollView isKindOfClass:[UIWebScrollView class]])
        scrollView = [scrollView superview];

    ASSERT(scrollView && [scrollView isKindOfClass:[UIWebScrollView class]]);
    const CGSize scrollViewSize = [scrollView bounds].size;
    CGSize contentSize = newFrame.size;
    contentSize.height = CGRound(std::max(CGRectGetMaxY(newFrame), scrollViewSize.height));
    [(UIWebScrollView *)scrollView setContentSize:contentSize];
}
@end
#endif

@interface NSURLRequest (PrivateThingsWeShouldntReallyUse)
+(void)setAllowsAnyHTTPSCertificate:(BOOL)allow forHost:(NSString *)host;
@end

#if USE(APPKIT)
@interface NSSound ()
+ (void)_setAlertType:(NSUInteger)alertType;
@end
#endif

@interface WebView ()
- (BOOL)_flushCompositingChanges;
@end

#if !PLATFORM(IOS_FAMILY)
@interface WebView (WebViewInternalForTesting)
- (WebCore::Frame*)_mainCoreFrame;
@end
#endif

static void runTest(const std::string& testURL);

// Deciding when it's OK to dump out the state is a bit tricky.  All these must be true:
// - There is no load in progress
// - There is no work queued up (see workQueue var, below)
// - waitToDump==NO.  This means either waitUntilDone was never called, or it was called
//       and notifyDone was called subsequently.
// Note that the call to notifyDone and the end of the load can happen in either order.

volatile bool done;

RetainPtr<NavigationController> gNavigationController;
RefPtr<TestRunner> gTestRunner;

std::optional<WTR::TestOptions> mainFrameTestOptions;
WebFrame *mainFrame = nil;
// This is the topmost frame that is loading, during a given load, or nil when no load is 
// in progress.  Usually this is the same as the main frame, but not always.  In the case
// where a frameset is loaded, and then new content is loaded into one of the child frames,
// that child frame is the "topmost frame that is loading".
WebFrame *topLoadingFrame = nil; // !nil iff a load is in progress

#if PLATFORM(MAC)
RetainPtr<NSWindow> mainWindow;
#endif

RetainPtr<CFMutableSetRef> disallowedURLs = nullptr;
static RetainPtr<CFRunLoopTimerRef> waitToDumpWatchdog;

static RetainPtr<WebView>& globalWebView()
{
    static NeverDestroyed<RetainPtr<WebView>> globalWebView;
    return globalWebView;
}

// Delegates
static RetainPtr<FrameLoadDelegate>& frameLoadDelegate()
{
    static NeverDestroyed<RetainPtr<FrameLoadDelegate>> frameLoadDelegate;
    return frameLoadDelegate;
}

static RetainPtr<UIDelegate>& uiDelegate()
{
    static NeverDestroyed<RetainPtr<UIDelegate>> uiDelegate;
    return uiDelegate;
}

static RetainPtr<EditingDelegate>& editingDelegate()
{
    static NeverDestroyed<RetainPtr<EditingDelegate>> editingDelegate;
    return editingDelegate;
}

static RetainPtr<ResourceLoadDelegate>& resourceLoadDelegate()
{
    static NeverDestroyed<RetainPtr<ResourceLoadDelegate>> resourceLoadDelegate;
    return resourceLoadDelegate;
}

static RetainPtr<HistoryDelegate>& historyDelegate()
{
    static NeverDestroyed<RetainPtr<HistoryDelegate>> historyDelegate;
    return historyDelegate;
}

RetainPtr<PolicyDelegate> policyDelegate;
RetainPtr<DefaultPolicyDelegate> defaultPolicyDelegate;

#if PLATFORM(IOS_FAMILY)
static RetainPtr<ScrollViewResizerDelegate>& scrollViewResizerDelegate()
{
    static NeverDestroyed<RetainPtr<ScrollViewResizerDelegate>> scrollViewResizerDelegate;
    return scrollViewResizerDelegate;
}
#endif

static int dumpPixelsForAllTests;
static bool dumpPixelsForCurrentTest;
static int threaded;
static int dumpTree = YES;
static int useTimeoutWatchdog = YES;
static int forceComplexText;
static int useAcceleratedDrawing;
static int gcBetweenTests;
static int allowAnyHTTPSCertificateForAllowedHosts;
static int enableAllExperimentalFeatures = YES;
static int showWebView;
static int printTestCount;
static int checkForWorldLeaks;
static BOOL printSeparators;
static std::set<std::string> allowedHosts;
static std::set<std::string> localhostAliases;
static std::string webCoreLogging;

static RetainPtr<CFStringRef>& persistentUserStyleSheetLocation()
{
    static NeverDestroyed<RetainPtr<CFStringRef>> persistentUserStyleSheetLocation;
    return persistentUserStyleSheetLocation;
}

static RetainPtr<WebHistoryItem>& prevTestBFItem()
{
    static NeverDestroyed<RetainPtr<WebHistoryItem>> _prevTestBFItem; // current b/f item at the end of the previous test.
    return _prevTestBFItem;
}

#if PLATFORM(IOS_FAMILY)
const CGRect layoutTestViewportRect = { {0, 0}, {static_cast<CGFloat>(TestRunner::viewWidth), static_cast<CGFloat>(TestRunner::viewHeight)} };
RetainPtr<DumpRenderTreeBrowserView> gWebBrowserView;
RetainPtr<DumpRenderTreeWebScrollView> gWebScrollView;
RetainPtr<DumpRenderTreeWindow> gDrtWindow;
#endif

void setPersistentUserStyleSheetLocation(CFStringRef url)
{
    persistentUserStyleSheetLocation() = url;
}

static bool shouldIgnoreWebCoreNodeLeaks(const std::string& urlString)
{
    static char* const ignoreSet[] = {
        // Keeping this infrastructure around in case we ever need it again.
    };
    static const int ignoreSetCount = sizeof(ignoreSet) / sizeof(char*);

    for (int i = 0; i < ignoreSetCount; i++) {
        // FIXME: ignore case
        std::string curIgnore(ignoreSet[i]);
        // Match at the end of the urlString.
        if (!urlString.compare(urlString.length() - curIgnore.length(), curIgnore.length(), curIgnore))
            return true;
    }
    return false;
}

#if !PLATFORM(IOS_FAMILY)
static NSSet *allowedFontFamilySet()
{
    static NeverDestroyed<RetainPtr<NSSet>> fontFamilySet = [NSSet setWithObjects:
        @"Ahem",
        @"Al Bayan",
        @"American Typewriter",
        @"Andale Mono",
        @"Apple Braille",
        @"Apple Color Emoji",
        @"Apple Chancery",
        @"Apple Garamond BT",
        @"Apple LiGothic",
        @"Apple LiSung",
        @"Apple Symbols",
        @"AppleGothic",
        @"AppleMyungjo",
        @"Arial Black",
        @"Arial Hebrew",
        @"Arial Narrow",
        @"Arial Rounded MT Bold",
        @"Arial Unicode MS",
        @"Arial",
        @"Avenir Next",
        @"Ayuthaya",
        @"Baghdad",
        @"Baskerville",
        @"BiauKai",
        @"Big Caslon",
        @"Brush Script MT",
        @"Chalkboard",
        @"Chalkduster",
        @"Charcoal CY",
        @"Cochin",
        @"Comic Sans MS",
        @"Copperplate",
        @"Corsiva Hebrew",
        @"Courier New",
        @"Courier",
        @"DecoType Naskh",
        @"Devanagari MT",
        @"Didot",
        @"Euphemia UCAS",
        @"Futura",
        @"GB18030 Bitmap",
        @"Geeza Pro",
        @"Geneva CY",
        @"Geneva",
        @"Georgia",
        @"Gill Sans",
        @"Gujarati MT",
        @"GungSeo",
        @"Gurmukhi MT",
        @"HeadLineA",
        @"Hei",
        @"Heiti SC",
        @"Heiti TC",
        @"Helvetica CY",
        @"Helvetica Neue",
        @"Helvetica",
        @"Herculanum",
        @"Hiragino Kaku Gothic Pro",
        @"Hiragino Kaku Gothic ProN",
        @"Hiragino Kaku Gothic Std",
        @"Hiragino Kaku Gothic StdN",
        @"Hiragino Maru Gothic Pro",
        @"Hiragino Maru Gothic ProN",
        @"Hiragino Mincho Pro",
        @"Hiragino Mincho ProN",
        @"Hiragino Sans",
        @"Hiragino Sans GB",
        @"Hoefler Text",
        @"Impact",
        @"InaiMathi",
        @"Kai",
        @"Kailasa",
        @"Kokonor",
        @"Krungthep",
        @"KufiStandardGK",
        @"Lao Sangam MN",
        @"LastResort",
        @"LiHei Pro",
        @"LiSong Pro",
        @"Lucida Grande",
        @"Marker Felt",
        @"Menlo",
        @"Microsoft Sans Serif",
        @"Monaco",
        @"Mshtakan",
        @"Nadeem",
        @"New Peninim MT",
        @"Optima",
        @"Osaka",
        @"Palatino",
        @"Papyrus",
        @"PCMyungjo",
        @"PilGi",
        @"PingFang HK",
        @"PingFang SC",
        @"PingFang TC",
        @"Plantagenet Cherokee",
        @"Raanana",
        @"Sathu",
        @"Silom",
        @"Skia",
        @"Snell Roundhand",
        @"Songti SC",
        @"Songti TC",
        @"STFangsong",
        @"STHeiti",
        @"STIX Two Math",
        @"STIX Two Text",
        @"STIXGeneral",
        @"STIXSizeOneSym",
        @"STKaiti",
        @"STSong",
        @"Symbol",
        @"Tahoma",
        @"Thonburi",
        @"Times New Roman",
        @"Times",
        @"Trebuchet MS",
        @"Verdana",
        @"Webdings",
        @"WebKit WeightWatcher",
        @"FontWithFeaturesOTF",
        @"FontWithFeaturesTTF",
        @"Wingdings 2",
        @"Wingdings 3",
        @"Wingdings",
        @"Zapf Dingbats",
        @"Zapfino",
        nil];

    return fontFamilySet.get().get();
}

static NSArray *fontAllowList()
{
    static NeverDestroyed availableFonts = [] {
        auto availableFonts = adoptNS([[NSMutableArray alloc] init]);
        for (NSString *fontFamily in allowedFontFamilySet()) {
            NSArray* fontsForFamily = [[NSFontManager sharedFontManager] availableMembersOfFontFamily:fontFamily];
            [availableFonts addObject:fontFamily];
            for (NSArray* fontInfo in fontsForFamily) {
                // Font name is the first entry in the array.
                [availableFonts addObject:[fontInfo objectAtIndex:0]];
            }
        }
        return availableFonts;
    }();
    return availableFonts.get().get();
}

// Activating system copies of these fonts overrides any others that could be preferred, such as ones
// in /Library/Fonts/Microsoft, and which don't always have the same metrics.
// FIXME: Switch to a solution from <rdar://problem/19553550> once it's available.
static void activateSystemCoreWebFonts()
{
    NSArray *coreWebFontNames = @[
        @"Andale Mono",
        @"Arial",
        @"Arial Black",
        @"Comic Sans MS",
        @"Courier New",
        @"Georgia",
        @"Impact",
        @"Times New Roman",
        @"Trebuchet MS",
        @"Verdana",
        @"Webdings"
    ];

    NSArray *fontFiles = [[NSFileManager defaultManager] contentsOfDirectoryAtURL:[NSURL fileURLWithPath:@"/Library/Fonts" isDirectory:YES]
        includingPropertiesForKeys:@[NSURLFileResourceTypeKey, NSURLNameKey] options:0 error:0];

    for (NSURL *fontURL in fontFiles) {
        NSString *resourceType;
        NSString *fileName;
        if (![fontURL getResourceValue:&resourceType forKey:NSURLFileResourceTypeKey error:0]
            || ![fontURL getResourceValue:&fileName forKey:NSURLNameKey error:0])
            continue;
        if (![resourceType isEqualToString:NSURLFileResourceTypeRegular])
            continue;

        // Activate all font variations, such as Arial Bold Italic.ttf. This algorithm is not 100% precise, as it
        // also activates e.g. Arial Unicode, which is not a variation of Arial.
        for (NSString *coreWebFontName in coreWebFontNames) {
            if ([fileName hasPrefix:coreWebFontName]) {
                CTFontManagerRegisterFontsForURL((CFURLRef)fontURL, kCTFontManagerScopeProcess, 0);
                break;
            }
        }
    }
}

static void activateTestingFonts()
{
    constexpr NSString *fontFileNames[] = {
        @"AHEM____.TTF",
        @"WebKitWeightWatcher100.ttf",
        @"WebKitWeightWatcher200.ttf",
        @"WebKitWeightWatcher300.ttf",
        @"WebKitWeightWatcher400.ttf",
        @"WebKitWeightWatcher500.ttf",
        @"WebKitWeightWatcher600.ttf",
        @"WebKitWeightWatcher700.ttf",
        @"WebKitWeightWatcher800.ttf",
        @"WebKitWeightWatcher900.ttf",
        @"FontWithFeatures.ttf",
        @"FontWithFeatures.otf",
    };

    NSURL *resourcesDirectory = [NSURL URLWithString:@"DumpRenderTree.resources" relativeToURL:[[NSBundle mainBundle] executableURL]];
    auto fontURLs = createNSArray(fontFileNames, [&] (NSString *name) {
        return [resourcesDirectory URLByAppendingPathComponent:name isDirectory:NO].absoluteURL;
    });

    CFArrayRef errors = nullptr;
    if (!CTFontManagerRegisterFontsForURLs((CFArrayRef)fontURLs.get(), kCTFontManagerScopeProcess, &errors)) {
        NSLog(@"Failed to activate fonts: %@", errors);
        CFRelease(errors);
        exit(1);
    }
}

static void adjustFonts()
{
    activateSystemCoreWebFonts();
    activateTestingFonts();
}

#else

static void activateFontIOS(const uint8_t* fontData, unsigned long length, std::string sectionName)
{
    auto data = adoptCF(CGDataProviderCreateWithData(nullptr, fontData, length, nullptr));
    if (!data) {
        fprintf(stderr, "Failed to create CGDataProviderRef for the %s font.\n", sectionName.c_str());
        exit(1);
    }

    auto cgFont = adoptCF(CGFontCreateWithDataProvider(data.get()));
    if (!cgFont) {
        fprintf(stderr, "Failed to create CGFontRef for the %s font.\n", sectionName.c_str());
        exit(1);
    }

    CFErrorRef error = nullptr;
    CTFontManagerRegisterGraphicsFont(cgFont.get(), &error);
    if (error) {
        fprintf(stderr, "Failed to add CGFont to CoreText for the %s font: %s.\n", sectionName.c_str(), CFStringGetCStringPtr(CFErrorCopyDescription(error), kCFStringEncodingUTF8));
        exit(1);
    }
}

static void activateFontsIOS()
{
    // __asm() requires a string literal, so we can't do this as either local variables or template parameters.
#define fontData(sectionName) \
{ \
    extern const uint8_t start##sectionName __asm("section$start$__DATA$" # sectionName); \
    extern const uint8_t end##sectionName __asm("section$end$__DATA$" # sectionName); \
    activateFontIOS(&start##sectionName, &end##sectionName - &start##sectionName, #sectionName); \
}
    fontData(Ahem);
    fontData(WeightWatcher100);
    fontData(WeightWatcher200);
    fontData(WeightWatcher300);
    fontData(WeightWatcher400);
    fontData(WeightWatcher500);
    fontData(WeightWatcher600);
    fontData(WeightWatcher700);
    fontData(WeightWatcher800);
    fontData(WeightWatcher900);
    fontData(FWFTTF);
    fontData(FWFOTF);
}
#endif // !PLATFORM(IOS_FAMILY)


#if PLATFORM(IOS_FAMILY)
static void adjustWebDocumentForFlexibleViewport(UIWebBrowserView *webBrowserView, UIWebScrollView *scrollView)
{
    // These values match MobileSafari's, see -[TabDocument _createDocumentView].
    [webBrowserView setMinimumScale:0.25f forDocumentTypes:UIEveryDocumentMask];
    [webBrowserView setMaximumScale:5.0f forDocumentTypes:UIEveryDocumentMask];
    [webBrowserView setInitialScale:UIWebViewScalesToFitScale forDocumentTypes:UIEveryDocumentMask];
    [webBrowserView setViewportSize:CGSizeMake(UIWebViewStandardViewportWidth, UIWebViewGrowsAndShrinksToFitHeight) forDocumentTypes:UIEveryDocumentMask];

    // Adjust the viewport view and viewport to have similar behavior
    // as the browser.
    [(DumpRenderTreeBrowserView *)webBrowserView setScrollingUsesUIWebScrollView:YES];
    [webBrowserView setDelegate:scrollViewResizerDelegate().get()];

    CGRect screenBounds = [UIScreen mainScreen].bounds;
    CGRect viewportRect = CGRectMake(0, 0, screenBounds.size.width, screenBounds.size.height);

    [scrollView setBounds:viewportRect];
    [scrollView setFrame:viewportRect];

    [webBrowserView setMinimumSize:viewportRect.size];
    [webBrowserView setAutoresizes:YES];
    [webBrowserView setFrame:screenBounds];

    // This just affects text autosizing.
    [mainFrame _setVisibleSize:CGSizeMake(screenBounds.size.width, screenBounds.size.height)];
}

static void adjustWebDocumentForStandardViewport(UIWebBrowserView *webBrowserView, UIWebScrollView *scrollView)
{
    [webBrowserView setMinimumScale:1.0f forDocumentTypes:UIEveryDocumentMask];
    [webBrowserView setMaximumScale:5.0f forDocumentTypes:UIEveryDocumentMask];
    [webBrowserView setInitialScale:1.0f forDocumentTypes:UIEveryDocumentMask];

    [(DumpRenderTreeBrowserView *)webBrowserView setScrollingUsesUIWebScrollView:NO];
    [webBrowserView setDelegate: nil];

    [scrollView setBounds:layoutTestViewportRect];
    [scrollView setFrame:layoutTestViewportRect];

    [webBrowserView setMinimumSize:layoutTestViewportRect.size];
    [webBrowserView setAutoresizes:NO];
    [webBrowserView setFrame:layoutTestViewportRect];

    // This just affects text autosizing.
    [mainFrame _setVisibleSize:CGSizeMake(layoutTestViewportRect.size.width, layoutTestViewportRect.size.height)];

}
#endif

#if !PLATFORM(IOS_FAMILY)
@interface DRTMockScroller : NSScroller
@end

@implementation DRTMockScroller

- (NSRect)rectForPart:(NSScrollerPart)partCode
{
    if (partCode != NSScrollerKnob)
        return [super rectForPart:partCode];

    NSRect frameRect = [self frame];
    NSRect bounds = [self bounds];
    BOOL isHorizontal = frameRect.size.width > frameRect.size.height;
    CGFloat trackLength = isHorizontal ? bounds.size.width : bounds.size.height;
    CGFloat minKnobSize = isHorizontal ? bounds.size.height : bounds.size.width;
    CGFloat knobLength = std::max(minKnobSize, static_cast<CGFloat>(std::round(trackLength * [self knobProportion])));
    CGFloat knobPosition = static_cast<CGFloat>((std::round([self doubleValue] * (trackLength - knobLength))));

    if (isHorizontal)
        return NSMakeRect(bounds.origin.x + knobPosition, bounds.origin.y, knobLength, bounds.size.height);

    return NSMakeRect(bounds.origin.x, bounds.origin.y +  + knobPosition, bounds.size.width, knobLength);
}

- (void)drawKnob
{
    if (![self isEnabled])
        return;

    NSRect knobRect = [self rectForPart:NSScrollerKnob];

    static NeverDestroyed<RetainPtr<NSColor>> knobColor = [NSColor colorWithDeviceRed:0x80 / 255.0 green:0x80 / 255.0 blue:0x80 / 255.0 alpha:1];
    [knobColor.get() set];

    NSRectFill(knobRect);
}

- (void)drawRect:(NSRect)dirtyRect
{
    static NeverDestroyed<RetainPtr<NSColor>> trackColor = [NSColor colorWithDeviceRed:0xC0 / 255.0 green:0xC0 / 255.0 blue:0xC0 / 255.0 alpha:1];
    static NeverDestroyed<RetainPtr<NSColor>> disabledTrackColor = [NSColor colorWithDeviceRed:0xE0 / 255.0 green:0xE0 / 255.0 blue:0xE0 / 255.0 alpha:1];

    if ([self isEnabled])
        [trackColor.get() set];
    else
        [disabledTrackColor.get() set];

    NSRectFill(dirtyRect);

    [self drawKnob];
}

@end

static void registerMockScrollbars()
{
    [WebDynamicScrollBarsView setCustomScrollerClass:[DRTMockScroller class]];
}
#endif

RetainPtr<WebView> createWebViewAndOffscreenWindow()
{
#if !PLATFORM(IOS_FAMILY)
    NSRect rect = NSMakeRect(0, 0, TestRunner::viewWidth, TestRunner::viewHeight);
    auto webView = adoptNS([[WebView alloc] initWithFrame:rect frameName:nil groupName:@"org.webkit.DumpRenderTree"]);
#else
    auto webBrowserView = adoptNS([[DumpRenderTreeBrowserView alloc] initWithFrame:layoutTestViewportRect]);
    [webBrowserView setInputViewObeysDOMFocus:YES];
    auto webView = retainPtr([webBrowserView webView]);
    [webView setGroupName:@"org.webkit.DumpRenderTree"];
#endif

    [webView setUIDelegate:uiDelegate().get()];
    [webView setFrameLoadDelegate:frameLoadDelegate().get()];
    [webView setEditingDelegate:editingDelegate().get()];
    [webView setResourceLoadDelegate:resourceLoadDelegate().get()];
    [webView _setGeolocationProvider:[MockGeolocationProvider shared]];
    [webView _setDeviceOrientationProvider:[WebDeviceOrientationProviderMock shared]];
    [webView _setNotificationProvider:[MockWebNotificationProvider shared]];

    // Register the same schemes that Safari does
    [WebView registerURLSchemeAsLocal:@"feed"];
    [WebView registerURLSchemeAsLocal:@"feeds"];
    [WebView registerURLSchemeAsLocal:@"feedsearch"];

#if PLATFORM(MAC)
    [webView setWindowOcclusionDetectionEnabled:NO];
    [WebView _setFontAllowList:fontAllowList()];
#endif

#if !PLATFORM(IOS_FAMILY)
    [webView setContinuousSpellCheckingEnabled:YES];
    [webView setAutomaticQuoteSubstitutionEnabled:NO];
    [webView setAutomaticLinkDetectionEnabled:NO];
    [webView setAutomaticDashSubstitutionEnabled:NO];
    [webView setAutomaticTextReplacementEnabled:NO];
    [webView setAutomaticSpellingCorrectionEnabled:YES];
    [webView setGrammarCheckingEnabled:YES];

    [webView setDefersCallbacks:NO];
    [webView setInteractiveFormValidationEnabled:YES];
    [webView setValidationMessageTimerMagnification:-1];

    // To make things like certain NSViews, dragging, and plug-ins work, put the WebView a window, but put it off-screen so you don't see it.
    // Put it at -10000, -10000 in "flipped coordinates", since WebCore and the DOM use flipped coordinates.
    NSScreen *firstScreen = [[NSScreen screens] firstObject];
    NSRect windowRect = (showWebView) ? NSOffsetRect(rect, 100, 100) : NSOffsetRect(rect, -10000, [firstScreen frame].size.height - rect.size.height + 10000);
    mainWindow = adoptNS([[DumpRenderTreeWindow alloc] initWithContentRect:windowRect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES]);
    [mainWindow setReleasedWhenClosed:NO];
    [mainWindow setColorSpace:[firstScreen colorSpace]];
    [mainWindow setCollectionBehavior:NSWindowCollectionBehaviorStationary];
    [[mainWindow contentView] addSubview:webView.get()];
    if (showWebView)
        [mainWindow orderFront:nil];
    else
        [mainWindow orderBack:nil];
    [mainWindow setAutodisplay:NO];

    [(DumpRenderTreeWindow *)mainWindow.get() startListeningForAcceleratedCompositingChanges];
#else
    auto drtWindow = adoptNS([[DumpRenderTreeWindow alloc] initWithLayer:[webBrowserView layer]]);
    [drtWindow setContentView:webView.get()];
    [webBrowserView setWAKWindow:drtWindow.get()];

    [[webView window] makeFirstResponder:[[[webView mainFrame] frameView] documentView]];

    CGRect uiWindowRect = layoutTestViewportRect;
    uiWindowRect.origin.y += [UIApp statusBarHeight];
    auto uiWindow = adoptNS([[UIWindow alloc] initWithFrame:uiWindowRect]);

    auto viewController = adoptNS([[UIViewController alloc] init]);
    [uiWindow setRootViewController:viewController.get()];

    // The UIWindow and UIWebBrowserView are released when the DumpRenderTreeWindow is closed.
    drtWindow.get().uiWindow = uiWindow.get();
    drtWindow.get().browserView = webBrowserView.get();

    auto scrollView = adoptNS([[DumpRenderTreeWebScrollView alloc] initWithFrame:layoutTestViewportRect]);
    [scrollView addSubview:webBrowserView.get()];

    [[viewController view] addSubview:scrollView.get()];

    adjustWebDocumentForStandardViewport(webBrowserView.get(), scrollView.get());
#endif

#if !PLATFORM(IOS_FAMILY)
    // For reasons that are not entirely clear, the following pair of calls makes WebView handle its
    // dynamic scrollbars properly. Without it, every frame will always have scrollbars.
    NSBitmapImageRep *imageRep = [webView bitmapImageRepForCachingDisplayInRect:[webView bounds]];
    [webView cacheDisplayInRect:[webView bounds] toBitmapImageRep:imageRep];
#else
    // Initialize the global UIViews, and set the key UIWindow to be painted.
    if (!gWebBrowserView) {
        gWebBrowserView = WTFMove(webBrowserView);
        gWebScrollView = WTFMove(scrollView);
        gDrtWindow = WTFMove(drtWindow);
        [uiWindow makeKeyAndVisible];
        [uiWindow retain];
    }
#endif

    [webView setMediaVolume:0];
    return webView;
}

static void destroyGlobalWebViewAndOffscreenWindow()
{
    if (!mainFrame && !globalWebView())
        return;

    ASSERT(globalWebView() == [mainFrame webView]);

#if !PLATFORM(IOS_FAMILY)
    NSWindow *window = [globalWebView() window];
#endif
    [globalWebView() close];
    mainFrame = nil;
    globalWebView() = nil;

#if !PLATFORM(IOS_FAMILY)
    // Work around problem where registering drag types leaves an outstanding
    // "perform selector" on the window, which retains the window. It's a bit
    // inelegant and perhaps dangerous to just blow them all away, but in practice
    // it probably won't cause any trouble (and this is just a test tool, after all).
    [NSObject cancelPreviousPerformRequestsWithTarget:window];

    [window close];
    mainWindow = nil;
#else
    auto uiWindow = adoptNS([gWebBrowserView window]);
    [uiWindow removeFromSuperview];
#endif
}

static void createGlobalWebViewAndOffscreenWindow()
{
    destroyGlobalWebViewAndOffscreenWindow();
    globalWebView() = createWebViewAndOffscreenWindow();
    mainFrame = [globalWebView() mainFrame];
}

static NSString *libraryPathForDumpRenderTree()
{
    char* dumpRenderTreeTemp = getenv("DUMPRENDERTREE_TEMP");
    if (dumpRenderTreeTemp)
        return [[NSFileManager defaultManager] stringWithFileSystemRepresentation:dumpRenderTreeTemp length:strlen(dumpRenderTreeTemp)];
    else
        return [@"~/Library/Application Support/DumpRenderTree" stringByExpandingTildeInPath];
}

static void setWebPreferencesForTestOptions(WebPreferences *preferences, const WTR::TestOptions& options)
{
    [preferences _batchUpdatePreferencesInBlock:^(WebPreferences *preferences) {
        [preferences _resetForTesting];

        if (enableAllExperimentalFeatures) {
            for (WebFeature *feature in [WebPreferences _experimentalFeatures]) {
                // FIXME: ShowModalDialogEnabled and NeedsSiteSpecificQuirks are `developer` settings which should not be enabled by default, but are currently lumped in with the other user-visible features. rdar://103648153
                // FIXME: BeaconAPIEnabled and LocalFileContentSniffingEnabled These are `stable` settings but should be turned off in WebKitLegacy.
                if (![feature.key isEqualToString:@"ShowModalDialogEnabled"]
                    && ![feature.key isEqualToString:@"NeedsSiteSpecificQuirks"]
                    && ![feature.key isEqualToString:@"BeaconAPIEnabled"]
                    && ![feature.key isEqualToString:@"LocalFileContentSniffingEnabled"]) {
                    [preferences _setEnabled:YES forFeature:feature];
                }
            }
        }

        
        if (persistentUserStyleSheetLocation()) {
            preferences.userStyleSheetLocation = [NSURL URLWithString:(__bridge NSString *)persistentUserStyleSheetLocation().get()];
            preferences.userStyleSheetEnabled = YES;
        } else
            preferences.userStyleSheetEnabled = NO;

        preferences.acceleratedDrawingEnabled = useAcceleratedDrawing;
        preferences.editableLinkBehavior = WebKitEditableLinkOnlyLiveWithShiftKey;
        preferences.frameFlattening = WebKitFrameFlatteningDisabled;
        preferences.cacheModel = WebCacheModelDocumentBrowser;

        preferences.privateBrowsingEnabled = options.useEphemeralSession();

        for (const auto& [key, value] : options.boolWebPreferenceFeatures())
            [preferences _setBoolPreferenceForTestingWithValue:value forKey:toNS(WTR::TestOptions::toWebKitLegacyPreferenceKey(key)).get()];

        for (const auto& [key, value] : options.doubleWebPreferenceFeatures())
            [preferences _setDoublePreferenceForTestingWithValue:value forKey:toNS(WTR::TestOptions::toWebKitLegacyPreferenceKey(key)).get()];

        for (const auto& [key, value] : options.uint32WebPreferenceFeatures())
            [preferences _setUInt32PreferenceForTestingWithValue:value forKey:toNS(WTR::TestOptions::toWebKitLegacyPreferenceKey(key)).get()];

        for (const auto& [key, value] : options.stringWebPreferenceFeatures())
            [preferences _setStringPreferenceForTestingWithValue:toNS(value).get() forKey:toNS(WTR::TestOptions::toWebKitLegacyPreferenceKey(key)).get()];

        // FIXME: Tests currently expect this to always be false in WebKitLegacy testing - https://bugs.webkit.org/show_bug.cgi?id=222864.
        [preferences _setBoolPreferenceForTestingWithValue:NO forKey:@"WebKitLayoutFormattingContextEnabled"];
    }];

    [WebPreferences _clearNetworkLoaderSession:^{ }];
    [WebPreferences _setCurrentNetworkLoaderSessionCookieAcceptPolicy:NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain];
}

// Called once on DumpRenderTree startup.
static void setDefaultsToConsistentValuesForTesting()
{
#if PLATFORM(IOS_FAMILY)
    WebThreadLock();
#endif

    static const int NoFontSmoothing = 0;
    static const int BlueTintedAppearance = 1;

    NSString *libraryPath = libraryPathForDumpRenderTree();

    NSDictionary *dict = @{
        @"AppleKeyboardUIMode": @1,
        @"AppleAntiAliasingThreshold": @4,
        @"AppleFontSmoothing": @(NoFontSmoothing),
        @"AppleAquaColorVariant": @(BlueTintedAppearance),
        @"AppleHighlightColor": @"0.709800 0.835300 1.000000",
        @"AppleOtherHighlightColor":@"0.500000 0.500000 0.500000",
        @"AppleLanguages": @[ @"en" ],
        WebKitEnableFullDocumentTeardownPreferenceKey: @YES,
        @"UseWebKitWebInspector": @YES,
#if !PLATFORM(IOS_FAMILY)
        @"NSPreferredSpellServerLanguage": @"en_US",
        @"NSUserDictionaryReplacementItems": @[],
        @"NSTestCorrectionDictionary": @{
            @"notationl": @"notational",
            @"mesage": @"message",
            @"wouldn": @"would",
            @"wellcome": @"welcome",
            @"hellolfworld": @"hello\nworld"
        },
#endif
        @"AppleScrollBarVariant": @"DoubleMax",
#if !PLATFORM(IOS_FAMILY)
        @"NSScrollAnimationEnabled": @NO,
#endif
        @"NSScrollViewUseLegacyScrolling": @YES,
        @"NSOverlayScrollersEnabled": @NO,
        @"AppleShowScrollBars": @"Always",
        @"NSButtonAnimationsEnabled": @NO, // Ideally, we should find a way to test animations, but for now, make sure that the dumped snapshot matches actual state.
        @"NSWindowDisplayWithRunLoopObserver": @YES, // Temporary workaround, see <rdar://problem/20351297>.
        @"AppleEnableSwipeNavigateWithScrolls": @YES,
        @"com.apple.swipescrolldirection": @1,
    };

    [[NSUserDefaults standardUserDefaults] setValuesForKeysWithDictionary:dict];

    NSDictionary *processInstanceDefaults = @{
        WebDatabaseDirectoryDefaultsKey: [libraryPath stringByAppendingPathComponent:@"Databases"],
        WebStorageDirectoryDefaultsKey: [libraryPath stringByAppendingPathComponent:@"LocalStorage"],
        WebKitLocalCacheDefaultsKey: [libraryPath stringByAppendingPathComponent:@"LocalCache"],
        WebKitResourceLoadStatisticsDirectoryDefaultsKey: [libraryPath stringByAppendingPathComponent:@"LocalStorage"],
    };

    [[NSUserDefaults standardUserDefaults] setVolatileDomain:processInstanceDefaults forName:NSArgumentDomain];
}

static void allocateGlobalControllers()
{
    // FIXME: We should remove these and move to the ObjC standard [Foo sharedInstance] model
    gNavigationController = adoptNS([[NavigationController alloc] init]);
    frameLoadDelegate() = adoptNS([[FrameLoadDelegate alloc] init]);
    uiDelegate() = adoptNS([[UIDelegate alloc] init]);
    editingDelegate() = adoptNS([[EditingDelegate alloc] init]);
    resourceLoadDelegate() = adoptNS([[ResourceLoadDelegate alloc] init]);
    policyDelegate = adoptNS([[PolicyDelegate alloc] init]);
    historyDelegate() = adoptNS([[HistoryDelegate alloc] init]);
    defaultPolicyDelegate = adoptNS([[DefaultPolicyDelegate alloc] init]);
#if PLATFORM(IOS_FAMILY)
    scrollViewResizerDelegate() = adoptNS([[ScrollViewResizerDelegate alloc] init]);
#endif
}

static void releaseGlobalControllers()
{
    gNavigationController = nil;
    frameLoadDelegate() = nil;
    editingDelegate() = nil;
    resourceLoadDelegate() = nil;
    uiDelegate() = nil;
    historyDelegate() = nil;
    policyDelegate = nil;
    defaultPolicyDelegate = nil;
#if PLATFORM(IOS_FAMILY)
    scrollViewResizerDelegate() = nil;
#endif
}

static void initializeGlobalsFromCommandLineOptions(int argc, const char *argv[])
{
    struct option options[] = {
        {"notree", no_argument, &dumpTree, NO},
        {"pixel-tests", no_argument, &dumpPixelsForAllTests, YES},
        {"tree", no_argument, &dumpTree, YES},
        {"threaded", no_argument, &threaded, YES},
        {"complex-text", no_argument, &forceComplexText, YES},
        {"accelerated-drawing", no_argument, &useAcceleratedDrawing, YES},
        {"gc-between-tests", no_argument, &gcBetweenTests, YES},
        {"no-timeout", no_argument, &useTimeoutWatchdog, NO},
        {"allowed-host", required_argument, nullptr, 'a'},
        {"allow-any-certificate-for-allowed-hosts", no_argument, &allowAnyHTTPSCertificateForAllowedHosts, YES},
        {"no-enable-all-experimental-features", no_argument, &enableAllExperimentalFeatures, NO},
        {"show-webview", no_argument, &showWebView, YES},
        {"print-test-count", no_argument, &printTestCount, YES},
        {"world-leaks", no_argument, &checkForWorldLeaks, NO},
        {"webcore-logging", required_argument, nullptr, 'w'},
        {"localhost-alias", required_argument, nullptr, 'l'},
        {nullptr, 0, nullptr, 0}
    };

    int option;
    while ((option = getopt_long(argc, (char * const *)argv, "", options, nullptr)) != -1) {
        switch (option) {
            case '?':   // unknown or ambiguous option
            case ':':   // missing argument
                exit(1);
                break;
            case 'a': // "allowed-host"
                allowedHosts.insert(optarg);
                break;
            case 'l': // "localhost-alias"
                localhostAliases.insert(optarg);
                allowedHosts.insert(optarg); // localhost is implicitly allowed and so should aliases to it.
                break;
            case 'w': // "webcore-logging"
                webCoreLogging = optarg;
        }
    }
}

static void addTestPluginsToPluginSearchPath(const char* executablePath)
{
#if !PLATFORM(IOS_FAMILY)
    NSString *pwd = [[NSString stringWithUTF8String:executablePath] stringByDeletingLastPathComponent];
    [WebPluginDatabase setAdditionalWebPlugInPaths:@[pwd]];
    [[WebPluginDatabase sharedDatabase] refresh];
#endif
}

static bool useLongRunningServerMode(int argc, const char *argv[])
{
    // This assumes you've already called getopt_long
    return (argc == optind+1 && strcmp(argv[optind], "-") == 0);
}

static bool handleControlCommand(const char* command)
{
    if (!strncmp("#CHECK FOR WORLD LEAKS", command, 22) || !strncmp("#LIST CHILD PROCESSES", command, 21)) {
        // DumpRenderTree does not support checking for world leaks or listing child processes.
        WTF::String result("\n"_s);
        unsigned resultLength = result.length();
        printf("Content-Type: text/plain\n");
        printf("Content-Length: %u\n", resultLength);
        fwrite(result.utf8().data(), 1, resultLength, stdout);
        printf("#EOF\n");
        fprintf(stderr, "#EOF\n");
        fflush(stdout);
        fflush(stderr);
        return true;
    }
    return false;
}

static void runTestingServerLoop()
{
    // When DumpRenderTree run in server mode, we just wait around for file names
    // to be passed to us and read each in turn, passing the results back to the client
    char filenameBuffer[2048];
    unsigned testCount = 0;
    while (fgets(filenameBuffer, sizeof(filenameBuffer), stdin)) {
        char *newLineCharacter = strchr(filenameBuffer, '\n');
        if (newLineCharacter)
            *newLineCharacter = '\0';

        if (strlen(filenameBuffer) == 0)
            continue;

        if (handleControlCommand(filenameBuffer))
            continue;

        runTest(filenameBuffer);

        if (printTestCount) {
            ++testCount;
            printf("\nServer mode has completed %u tests.\n\n", testCount);
        }
    }
}

#if PLATFORM(IOS_FAMILY)
static BOOL overrideIsInHardwareKeyboardMode()
{
    return NO;
}
#endif

static void prepareConsistentTestingEnvironment()
{
#if PLATFORM(MAC)
    poseAsClass("DumpRenderTreePasteboard", "NSPasteboard");
    poseAsClass("DumpRenderTreeEvent", "NSEvent");
#else
    poseAsClass("DumpRenderTreeEvent", "GSEvent");

    // Override the implementation of +[UIKeyboard isInHardwareKeyboardMode] to ensure that test runs are deterministic
    // regardless of whether a hardware keyboard is attached. We intentionally never restore the original implementation.
    method_setImplementation(class_getClassMethod([UIKeyboard class], @selector(isInHardwareKeyboardMode)), reinterpret_cast<IMP>(overrideIsInHardwareKeyboardMode));
#endif

    [[WebPreferences standardPreferences] setAutosaves:NO];

    [WebPreferences _switchNetworkLoaderToNewTestingSession];

#if PLATFORM(MAC)
    adjustFonts();
    registerMockScrollbars();

    // The mock scrollbars setting cannot be modified after creating a view, so we have to do it now.
    [[WebPreferences standardPreferences] setMockScrollbarsEnabled:YES];
#else
    activateFontsIOS();

    // Enable the tracker before creating the first WebView will cause initialization to use the correct database paths.
    [[WebPreferences standardPreferences] setStorageTrackerEnabled:YES];
#endif

    allocateGlobalControllers();

#if PLATFORM(MAC)
    NSActivityOptions options = (NSActivityUserInitiatedAllowingIdleSystemSleep | NSActivityLatencyCritical) & ~(NSActivitySuddenTerminationDisabled | NSActivityAutomaticTerminationDisabled);
    static NeverDestroyed<RetainPtr<id>> assertion = [[NSProcessInfo processInfo] beginActivityWithOptions:options reason:@"DumpRenderTree should not be subject to process suppression"];
    ASSERT_UNUSED(assertion, assertion.get());
#endif

    if (webCoreLogging.length())
        [[NSUserDefaults standardUserDefaults] setValue:[NSString stringWithUTF8String:webCoreLogging.c_str()] forKey:@"WebCoreLogging"];
}

const char crashedMessage[] = "#CRASHED\n";

void writeCrashedMessageOnFatalError(int signalCode)
{
    // Reset the default action for the signal so that we run ReportCrash(8) on pending and
    // subsequent instances of the signal.
    signal(signalCode, SIG_DFL);

    // WRITE(2) and FSYNC(2) are considered safe to call from a signal handler by SIGACTION(2).
    write(STDERR_FILENO, &crashedMessage[0], sizeof(crashedMessage) - 1);
    fsync(STDERR_FILENO);
}

void dumpRenderTree(int argc, const char *argv[])
{
    JSC::Config::configureForTesting();

#if PLATFORM(IOS_FAMILY)
    setUpIOSLayoutTestCommunication();
    [UIApplication sharedApplication].idleTimerDisabled = YES;
    [[UIScreen mainScreen] _setScale:2.0];
#endif

    signal(SIGILL, &writeCrashedMessageOnFatalError);
    signal(SIGFPE, &writeCrashedMessageOnFatalError);
    signal(SIGBUS, &writeCrashedMessageOnFatalError);
    signal(SIGSEGV, &writeCrashedMessageOnFatalError);

    initializeGlobalsFromCommandLineOptions(argc, argv);
    prepareConsistentTestingEnvironment();
    addTestPluginsToPluginSearchPath(argv[0]);

    JSC::initialize();
    WTF::initializeMainThread();
    WebCoreTestSupport::populateJITOperations();

    if (forceComplexText)
        [WebView _setAlwaysUsesComplexTextCodePath:YES];

#if USE(APPKIT)
    [NSSound _setAlertType:0];
#endif

    [[NSURLCache sharedURLCache] removeAllCachedResponses];
    [WebCache empty];

    [NSURLRequest setAllowsAnyHTTPSCertificate:YES forHost:@"localhost"];
    [NSURLRequest setAllowsAnyHTTPSCertificate:YES forHost:@"127.0.0.1"];
    for (auto& localhostAlias : localhostAliases)
        [NSURLRequest setAllowsAnyHTTPSCertificate:YES forHost:[NSString stringWithUTF8String:localhostAlias.c_str()]];

    if (allowAnyHTTPSCertificateForAllowedHosts) {
        for (auto& host : allowedHosts)
            [NSURLRequest setAllowsAnyHTTPSCertificate:YES forHost:[NSString stringWithUTF8String:host.c_str()]];
    }

    if (threaded)
        startJavaScriptThreads();

    if (useLongRunningServerMode(argc, argv)) {
        printSeparators = YES;
        runTestingServerLoop();
    } else {
        printSeparators = optind < argc - 1;
        for (int i = optind; i != argc; ++i)
            runTest(argv[i]);
    }

    if (threaded)
        stopJavaScriptThreads();

    destroyGlobalWebViewAndOffscreenWindow();

    releaseGlobalControllers();

#if !PLATFORM(IOS_FAMILY)
    [DumpRenderTreePasteboard releaseLocalPasteboards];
#endif

    // FIXME: This should be moved onto TestRunner and made into a HashSet
    disallowedURLs = nullptr;

#if PLATFORM(IOS_FAMILY)
    tearDownIOSLayoutTestCommunication();
#endif
}

#if PLATFORM(IOS_FAMILY)
static int _argc;
static const char **_argv;

@implementation DumpRenderTree

- (void)_runDumpRenderTree
{
    dumpRenderTree(_argc, _argv);
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    [self performSelectorOnMainThread:@selector(_runDumpRenderTree) withObject:nil waitUntilDone:NO];
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    RELEASE_ASSERT_NOT_REACHED();
}

- (void)_webThreadEventLoopHasRun
{
    ASSERT(!WebThreadIsCurrent());
    _hasFlushedWebThreadRunQueue = YES;
}

- (void)_webThreadInvoked
{
    ASSERT(WebThreadIsCurrent());
    WorkQueue::main().dispatch([self, retainedSelf = retainPtr(self)] {
        [self _webThreadEventLoopHasRun];
    });
}

// The test can end in response to a delegate callback while there are still methods queued on the Web Thread.
// If we do not ensure the Web Thread has been run, the callback can be done on a WebView that no longer exists.
// To avoid this, _waitForWebThread dispatches a call to the WebThread event loop, actively processing the delegate
// callbacks in the main thread while waiting for the call to be invoked on the Web Thread.
- (void)_waitForWebThread
{
    ASSERT(!WebThreadIsCurrent());
    _hasFlushedWebThreadRunQueue = NO;
    WebThreadRun(^{
        [self _webThreadInvoked];
    });
    while (!_hasFlushedWebThreadRunQueue) {
        @autoreleasepool {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
        }
    }
}

@end
#endif

static bool returningFromMain = false;

void atexitFunction()
{
    if (returningFromMain)
        return;

    NSLog(@"DumpRenderTree is exiting unexpectedly. Generating a crash log.");
    __builtin_trap();
}

int DumpRenderTreeMain(int argc, const char *argv[])
{
    atexit(atexitFunction);

    WTF::setProcessPrivileges(allPrivileges());
    WebCore::NetworkStorageSession::permitProcessToUseCookieAPI(true);
    WebCoreTestSupport::setLinkedOnOrAfterEverythingForTesting();

#if PLATFORM(IOS_FAMILY)
IGNORE_WARNINGS_BEGIN("deprecated-implementations")
    _UIApplicationLoadWebKit();
IGNORE_WARNINGS_END
#endif

    @autoreleasepool {
        setDefaultsToConsistentValuesForTesting(); // Must be called before NSApplication initialization.

#if !PLATFORM(IOS_FAMILY)
        [DumpRenderTreeApplication sharedApplication]; // Force AppKit to init itself

        dumpRenderTree(argc, argv);
#else
        _argc = argc;
        _argv = argv;
        UIApplicationMain(argc, (char**)argv, @"DumpRenderTree", @"DumpRenderTree");
#endif

        [WebCoreStatistics garbageCollectJavaScriptObjects];
        [WebCoreStatistics emptyCache]; // Otherwise SVGImages trigger false positives for Frame/Node counts
        JSC::finalizeStatsAtEndOfTesting();
    }

    returningFromMain = true;
    return 0;
}

static NSInteger compareHistoryItems(id item1, id item2, void *context)
{
    return [[item1 target] caseInsensitiveCompare:[item2 target]];
}

static NSData *dumpAudio()
{
    const auto& dataVector = gTestRunner->audioResult();

    NSData *data = [NSData dataWithBytes:dataVector.data() length:dataVector.size()];
    return data;
}

static void dumpHistoryItem(WebHistoryItem *item, int indent, BOOL current)
{
    int start = 0;
    if (current) {
        printf("curr->");
        start = 6;
    }
    for (int i = start; i < indent; i++)
        putchar(' ');

    NSString *urlString = [item URLString];
    if ([[NSURL URLWithString:urlString] isFileURL]) {
        NSRange range = [urlString rangeOfString:@"/LayoutTests/"];
        urlString = [@"(file test):" stringByAppendingString:[urlString substringFromIndex:(range.length + range.location)]];
    }

    printf("%s", [urlString UTF8String]);
    NSString *target = [item target];
    if (target && [target length] > 0)
        printf(" (in frame \"%s\")", [target UTF8String]);
    if ([item isTargetItem])
        printf("  **nav target**");
    putchar('\n');
    NSArray *kids = [item children];
    if (kids) {
        // must sort to eliminate arbitrary result ordering which defeats reproducible testing
        kids = [kids sortedArrayUsingFunction:&compareHistoryItems context:nil];
        for (unsigned i = 0; i < [kids count]; i++)
            dumpHistoryItem([kids objectAtIndex:i], indent+4, NO);
    }
}

static void dumpFrameScrollPosition(WebFrame *f)
{
    WebScriptObject* scriptObject = [f windowObject];
    NSPoint scrollPosition = NSMakePoint(
        [[scriptObject valueForKey:@"pageXOffset"] floatValue],
        [[scriptObject valueForKey:@"pageYOffset"] floatValue]);
    if (ABS(scrollPosition.x) > 0.00000001 || ABS(scrollPosition.y) > 0.00000001) {
        if ([f parentFrame] != nil)
            printf("frame '%s' ", [[f name] UTF8String]);
        printf("scrolled to %.f,%.f\n", scrollPosition.x, scrollPosition.y);
    }

    if (gTestRunner->dumpChildFrameScrollPositions()) {
        NSArray *kids = [f childFrames];
        if (kids)
            for (unsigned i = 0; i < [kids count]; i++)
                dumpFrameScrollPosition([kids objectAtIndex:i]);
    }
}

static RetainPtr<NSString> dumpFramesAsText(WebFrame *frame)
{
    DOMDocument *document = [frame DOMDocument];
    DOMElement *documentElement = [document documentElement];

    if (!documentElement)
        return @"";

    RetainPtr<NSMutableString> result;

    // Add header for all but the main frame.
    if ([frame parentFrame])
        result = adoptNS([[NSMutableString alloc] initWithFormat:@"\n--------\nFrame: '%@'\n--------\n", [frame name]]);
    else
        result = adoptNS([[NSMutableString alloc] init]);

    NSString *innerText = [documentElement innerText];

    // We use WTF::String::tryGetUTF8 to convert innerText to a UTF8 buffer since
    // it can handle dangling surrogates and the NSString
    // conversion methods cannot. After the conversion to a buffer, we turn that buffer into
    // a CFString via fromUTF8WithLatin1Fallback().createCFString() which can be appended to
    // the result without any conversion.
    if (auto utf8Result = WTF::String(innerText).tryGetUTF8()) {
        auto string = WTFMove(utf8Result.value());
        [result appendFormat:@"%@\n", String::fromUTF8WithLatin1Fallback(string.data(), string.length()).createCFString().get()];
    } else
        [result appendString:@"\n"];

    if (gTestRunner->dumpChildFramesAsText()) {
        NSArray *kids = [frame childFrames];
        if (kids) {
            for (unsigned i = 0; i < [kids count]; i++)
                [result appendString:dumpFramesAsText([kids objectAtIndex:i]).get()];
        }
    }

    // To keep things tidy, strip all trailing spaces: they are not a meaningful part of dumpAsText test output.
    [result replaceOccurrencesOfString:@" +\n" withString:@"\n" options:NSRegularExpressionSearch range:NSMakeRange(0, [result length])];
    [result replaceOccurrencesOfString:@" +$" withString:@"" options:NSRegularExpressionSearch range:NSMakeRange(0, [result length])];

    return result;
}

static NSData *dumpFrameAsPDF(WebFrame *frame)
{
#if !PLATFORM(IOS_FAMILY)
    if (!frame)
        return nil;

    // Sadly we have to dump to a file and then read from that file again
    // +[NSPrintOperation PDFOperationWithView:insideRect:] requires a rect and prints to a single page
    // likewise +[NSView dataWithPDFInsideRect:] also prints to a single continuous page
    // The goal of this function is to test "real" printing across multiple pages.
    // FIXME: It's possible there might be printing SPI to let us print a multi-page PDF to an NSData object
    NSString *path = [libraryPathForDumpRenderTree() stringByAppendingPathComponent:@"test.pdf"];

    NSMutableDictionary *printInfoDict = [NSMutableDictionary dictionaryWithDictionary:[[NSPrintInfo sharedPrintInfo] dictionary]];
    [printInfoDict setObject:NSPrintSaveJob forKey:NSPrintJobDisposition];
    [printInfoDict setObject:path forKey:NSPrintSavePath];

    auto printInfo = adoptNS([[NSPrintInfo alloc] initWithDictionary:printInfoDict]);
    [printInfo setHorizontalPagination:NSAutoPagination];
    [printInfo setVerticalPagination:NSAutoPagination];
    [printInfo setVerticallyCentered:NO];

    NSPrintOperation *printOperation = [NSPrintOperation printOperationWithView:[frame frameView] printInfo:printInfo.get()];
    [printOperation setShowPanels:NO];
    [printOperation runOperation];

    NSData *pdfData = [NSData dataWithContentsOfFile:path];
    [[NSFileManager defaultManager] removeFileAtPath:path handler:nil];

    return pdfData;
#else
    return nil;
#endif
}

static void dumpBackForwardListForWebView(WebView *view)
{
    printf("\n============== Back Forward List ==============\n");
    WebBackForwardList *bfList = [view backForwardList];

    // Print out all items in the list after prevTestBFItem, which was from the previous test
    // Gather items from the end of the list, the print them out from oldest to newest
    auto itemsToPrint = adoptNS([[NSMutableArray alloc] init]);
    for (int i = [bfList forwardListCount]; i > 0; i--) {
        WebHistoryItem *item = [bfList itemAtIndex:i];
        // something is wrong if the item from the last test is in the forward part of the b/f list
        assert(item != prevTestBFItem());
        [itemsToPrint addObject:item];
    }

    assert([bfList currentItem] != prevTestBFItem());
    [itemsToPrint addObject:[bfList currentItem]];
    int currentItemIndex = [itemsToPrint count] - 1;

    for (int i = -1; i >= -[bfList backListCount]; i--) {
        WebHistoryItem *item = [bfList itemAtIndex:i];
        if (item == prevTestBFItem())
            break;
        [itemsToPrint addObject:item];
    }

    for (int i = [itemsToPrint count]-1; i >= 0; i--)
        dumpHistoryItem([itemsToPrint objectAtIndex:i], 8, i == currentItemIndex);

    printf("===============================================\n");
}

#if !PLATFORM(IOS_FAMILY)
static void changeWindowScaleIfNeeded(const char* testPathOrURL)
{
    auto localPathOrURL = String::fromUTF8(testPathOrURL);
    float currentScaleFactor = [[[mainFrame webView] window] backingScaleFactor];
    float requiredScaleFactor = 1;
    if (localPathOrURL.containsIgnoringASCIICase("/hidpi-3x-"_s))
        requiredScaleFactor = 3;
    else if (localPathOrURL.containsIgnoringASCIICase("/hidpi-"_s))
        requiredScaleFactor = 2;
    if (currentScaleFactor == requiredScaleFactor)
        return;
    // When the new scale factor is set on the window first, WebView doesn't see it as a new scale and stops propagating the behavior change to WebCore::Page.
    gTestRunner->setBackingScaleFactor(requiredScaleFactor);
    NSWindow *window = [[mainFrame webView] window];
    if ([window respondsToSelector:@selector(_setWindowResolution:)])
        [window _setWindowResolution:requiredScaleFactor];
    else
        [window _setWindowResolution:requiredScaleFactor displayIfChanged:YES];
}
#endif

static void sizeWebViewForCurrentTest()
{
    [uiDelegate() resetWindowOrigin];

    // W3C SVG tests expect to be 480x360
    bool isSVGW3CTest = (gTestRunner->testURL().find("svg/W3C-SVG-1.1") != std::string::npos);
    NSSize frameSize = isSVGW3CTest ? NSMakeSize(TestRunner::w3cSVGViewWidth, TestRunner::w3cSVGViewHeight) : NSMakeSize(TestRunner::viewWidth, TestRunner::viewHeight);
    [[mainFrame webView] setFrameSize:frameSize];
    [[mainFrame frameView] setFrame:NSMakeRect(0, 0, frameSize.width, frameSize.height)];
}

static const char *methodNameStringForFailedTest()
{
    const char *errorMessage;
    if (gTestRunner->dumpAsText())
        errorMessage = "[documentElement innerText]";
    else if (gTestRunner->dumpDOMAsWebArchive())
        errorMessage = "[[mainFrame DOMDocument] webArchive]";
    else if (gTestRunner->dumpSourceAsWebArchive())
        errorMessage = "[[mainFrame dataSource] webArchive]";
    else
        errorMessage = "[mainFrame renderTreeAsExternalRepresentation]";

    return errorMessage;
}

static void dumpBackForwardListForAllWindows()
{
    for (NSWindow *window in [DumpRenderTreeWindow openWindows]) {
#if !PLATFORM(IOS_FAMILY)
        WebView *webView = [[[window contentView] subviews] objectAtIndex:0];
#else
        ASSERT([[window contentView] isKindOfClass:[WebView class]]);
        WebView *webView = (WebView *)[window contentView];
#endif
        dumpBackForwardListForWebView(webView);
    }
}

static void invalidateAnyPreviousWaitToDumpWatchdog()
{
    if (waitToDumpWatchdog) {
        CFRunLoopTimerInvalidate(waitToDumpWatchdog.get());
        waitToDumpWatchdog = nullptr;
    }
}

void setWaitToDumpWatchdog(RetainPtr<CFRunLoopTimerRef>&& timer)
{
    ASSERT(timer);
    ASSERT(shouldSetWaitToDumpWatchdog());
    waitToDumpWatchdog = WTFMove(timer);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), waitToDumpWatchdog.get(), kCFRunLoopCommonModes);
}

bool shouldSetWaitToDumpWatchdog()
{
    return !waitToDumpWatchdog && useTimeoutWatchdog;
}

static void updateDisplay()
{
    WebView *webView = [mainFrame webView];
#if PLATFORM(IOS_FAMILY)
    [gWebBrowserView layoutIfNeeded]; // Re-enables tile painting, which was disabled when committing the frame load.
    [gDrtWindow layoutTilesNow];
    [webView _flushCompositingChanges];
#else
    [webView _forceRepaintForTesting];
    if ([webView _isUsingAcceleratedCompositing])
        [webView display];
    else
        [webView displayIfNeeded];
#endif
}

void dump()
{
#if PLATFORM(IOS_FAMILY)
    WebThreadLock();
#endif

    if (done)
        return;

    updateDisplay();

    invalidateAnyPreviousWaitToDumpWatchdog();
    ASSERT(!gTestRunner->hasPendingWebNotificationClick());

    if (dumpTree) {
        RetainPtr<NSString> resultString;
        NSData *resultData = nil;
        NSString *resultMimeType = @"text/plain";

        if ([[[mainFrame dataSource] _responseMIMEType] isEqualToString:@"text/plain"]) {
            gTestRunner->setDumpAsText(true);
            gTestRunner->setGeneratePixelResults(false);
        }
        if (gTestRunner->dumpAsAudio()) {
            resultData = dumpAudio();
            resultMimeType = @"audio/wav";
        } else if (gTestRunner->dumpAsText() || gTestRunner->dumpChildFramesAsText()) {
            resultString = dumpFramesAsText(mainFrame);
        } else if (gTestRunner->dumpAsPDF()) {
            resultData = dumpFrameAsPDF(mainFrame);
            resultMimeType = @"application/pdf";
        } else if (gTestRunner->dumpDOMAsWebArchive()) {
            WebArchive *webArchive = [[mainFrame DOMDocument] webArchive];
            resultString = bridge_cast(WebCoreTestSupport::createXMLStringFromWebArchiveData(bridge_cast([webArchive data])));
            resultMimeType = @"application/x-webarchive";
        } else if (gTestRunner->dumpSourceAsWebArchive()) {
            WebArchive *webArchive = [[mainFrame dataSource] webArchive];
            resultString = bridge_cast(WebCoreTestSupport::createXMLStringFromWebArchiveData(bridge_cast([webArchive data])));
            resultMimeType = @"application/x-webarchive";
        } else if (gTestRunner->isPrinting())
            resultString = [mainFrame renderTreeAsExternalRepresentationForPrinting];
        else
            resultString = [mainFrame renderTreeAsExternalRepresentationWithOptions:gTestRunner->renderTreeDumpOptions()];

        if (resultString && !resultData)
            resultData = [resultString dataUsingEncoding:NSUTF8StringEncoding];

        printf("Content-Type: %s\n", [resultMimeType UTF8String]);

        WTF::FastMallocStatistics mallocStats = WTF::fastMallocStatistics();
        printf("DumpMalloc: %li\n", mallocStats.committedVMBytes);

        if (gTestRunner->dumpAsAudio())
            printf("Content-Length: %lu\n", static_cast<unsigned long>([resultData length]));

        if (resultData) {
            fwrite([resultData bytes], 1, [resultData length], stdout);

            if (!gTestRunner->dumpAsText() && !gTestRunner->dumpDOMAsWebArchive() && !gTestRunner->dumpSourceAsWebArchive() && !gTestRunner->dumpAsAudio())
                dumpFrameScrollPosition(mainFrame);

            if (gTestRunner->dumpBackForwardList())
                dumpBackForwardListForAllWindows();
        } else
            printf("ERROR: nil result from %s", methodNameStringForFailedTest());

        // Stop the watchdog thread before we leave this test to make sure it doesn't
        // fire in between tests causing the next test to fail.
        // This is a speculative fix for: https://bugs.webkit.org/show_bug.cgi?id=32339
        invalidateAnyPreviousWaitToDumpWatchdog();

        if (printSeparators)
            puts("#EOF");       // terminate the content block
    }

    if (dumpPixelsForCurrentTest && gTestRunner->generatePixelResults())
        // FIXME: when isPrinting is set, dump the image with page separators.
        dumpWebViewAsPixelsAndCompareWithExpected(gTestRunner->expectedPixelHash());

    puts("#EOF");   // terminate the (possibly empty) pixels block
    fflush(stdout);

    done = YES;
    CFRunLoopStop(CFRunLoopGetMain());
}

static bool shouldLogFrameLoadDelegates(const char* pathOrURL)
{
    return strstr(pathOrURL, "loading/") && !strstr(pathOrURL, "://localhost");
}

static bool shouldLogHistoryDelegates(const char* pathOrURL)
{
    return strstr(pathOrURL, "globalhistory/");
}

static bool shouldDumpAsText(const char* pathOrURL)
{
    return strstr(pathOrURL, "dumpAsText/");
}

#if PLATFORM(IOS_FAMILY)
static bool shouldMakeViewportFlexible(const char* pathOrURL)
{
    return strstr(pathOrURL, "viewport/") && !strstr(pathOrURL, "visual-viewport/");
}
#endif

static bool shouldUseEphemeralSession(const char* pathOrURL)
{
    return strstr(pathOrURL, "w3c/IndexedDB-private-browsing");
}

static void setJSCOptions(const WTR::TestOptions& options)
{
    static WTF::StringBuilder savedOptions;

    if (!savedOptions.isEmpty()) {
        JSC::Options::setOptions(savedOptions.toString().ascii().data());
        savedOptions.clear();
    }

    if (options.jscOptions().length()) {
        JSC::Options::dumpAllOptionsInALine(savedOptions);
        JSC::Options::setOptions(options.jscOptions().c_str());
    }
}

enum class ResetTime { BeforeTest, AfterTest };
static void resetWebViewToConsistentState(const WTR::TestOptions& options, ResetTime resetTime)
{
    setJSCOptions(options);

    WebView *webView = [mainFrame webView];

#if PLATFORM(IOS_FAMILY)
    adjustWebDocumentForStandardViewport(gWebBrowserView.get(), gWebScrollView.get());
    [webView _setAllowsMessaging:YES];
    [[UIScreen mainScreen] _setScale:2.0];
#endif
    [webView setEditable:NO];
    [(EditingDelegate *)[webView editingDelegate] setAcceptsEditing:YES];
    [webView makeTextStandardSize:nil];
    [webView resetPageZoom:nil];
    [webView _scaleWebView:1.0 atOrigin:NSZeroPoint];
#if !PLATFORM(IOS_FAMILY)
    [webView _setCustomBackingScaleFactor:0];
#endif
    [webView setTabKeyCyclesThroughElements:YES];
    [webView setPolicyDelegate:defaultPolicyDelegate.get()];
    [policyDelegate setPermissive:NO];
    [policyDelegate setControllerToNotifyDone:0];
    [uiDelegate() resetToConsistentStateBeforeTesting:options];
    [frameLoadDelegate() resetToConsistentState];
    [webView _clearMainFrameName];
    [[webView undoManager] removeAllActions];
    [WebView _removeAllUserContentFromGroup:[webView groupName]];
#if !PLATFORM(IOS_FAMILY)
    [[webView window] setAutodisplay:NO];
#endif
    [webView setTracksRepaints:NO];

    [WebCache clearCachedCredentials];

    setWebPreferencesForTestOptions(webView.preferences, options);
#if PLATFORM(MAC)
    [webView setWantsLayer:options.layerBackedWebView()];
#endif

    TestRunner::setSerializeHTTPLoads(false);
    TestRunner::setAllowsAnySSLCertificate(true);

    setlocale(LC_ALL, "");

    ASSERT(gTestRunner);
    gTestRunner->resetPageVisibility();
    // In the case that a test using the chrome input field failed, be sure to clean up for the next test.
    gTestRunner->removeChromeInputField();

    // We do not do this before running the test since it may eagerly create the global JS context
    // if we have not loaded anything yet.
    if (resetTime == ResetTime::AfterTest)
        WebCoreTestSupport::resetInternalsObject([mainFrame globalContext]);

#if !PLATFORM(IOS_FAMILY)
    if (WebCore::Frame* frame = [webView _mainCoreFrame])
        WebCoreTestSupport::clearWheelEventTestMonitor(*frame);
#endif

#if !PLATFORM(IOS_FAMILY)
    [webView setContinuousSpellCheckingEnabled:YES];
    [webView setAutomaticQuoteSubstitutionEnabled:NO];
    [webView setAutomaticLinkDetectionEnabled:NO];
    [webView setAutomaticDashSubstitutionEnabled:NO];
    [webView setAutomaticTextReplacementEnabled:NO];
    [webView setAutomaticSpellingCorrectionEnabled:YES];
    [webView setGrammarCheckingEnabled:YES];

    [WebView _setUsesTestModeFocusRingColor:YES];
#endif
    [WebView _resetOriginAccessAllowLists];

    [[MockGeolocationProvider shared] stopTimer];
    [[MockWebNotificationProvider shared] reset];

#if !PLATFORM(IOS_FAMILY)
    // Clear the contents of the general pasteboard
    [[NSPasteboard generalPasteboard] declareTypes:@[NSStringPboardType] owner:nil];
#endif

    WebCoreTestSupport::setAdditionalSupportedImageTypesForTesting(String::fromLatin1(options.additionalSupportedImageTypes().c_str()));

    [mainFrame _clearOpener];

#if PLATFORM(MAC)
    [LayoutTestSpellChecker uninstallAndReset];
#endif

    WebCoreTestSupport::clearAllLogChannelsToAccumulate();
    WebCoreTestSupport::initializeLogChannelsIfNecessary();
}

#if PLATFORM(IOS_FAMILY)
// Work around <rdar://problem/9909073> WebKit's method of calling delegates on
// the main thread is not thread safe. If the web thread is attempting to call
// out to a delegate method on the main thread, we want to spin the main thread
// run loop until the delegate method completes before taking the web thread
// lock to prevent potentially re-entering WebCore.
static void WebThreadLockAfterDelegateCallbacksHaveCompleted()
{
    auto delegateSemaphore = adoptOSObject(dispatch_semaphore_create(0));
    WebThreadRun(^{
        dispatch_semaphore_signal(delegateSemaphore.get());
    });

    while (dispatch_semaphore_wait(delegateSemaphore.get(), DISPATCH_TIME_NOW)) {
        @autoreleasepool {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
        }
    }

    WebThreadLock();
}
#endif

static NSURL *computeTestURL(NSString *pathOrURLString, NSString **relativeTestPath)
{
    *relativeTestPath = nil;

    if ([pathOrURLString hasPrefix:@"http://"] || [pathOrURLString hasPrefix:@"https://"] || [pathOrURLString hasPrefix:@"file://"])
        return [NSURL URLWithString:pathOrURLString];

    NSString *absolutePath = [[[NSURL fileURLWithPath:pathOrURLString] absoluteURL] path];

    NSRange layoutTestsRange = [absolutePath rangeOfString:@"/LayoutTests/"];
    if (layoutTestsRange.location == NSNotFound)
        return [NSURL fileURLWithPath:absolutePath];

    *relativeTestPath = [absolutePath substringFromIndex:NSMaxRange(layoutTestsRange)];
    return [NSURL fileURLWithPath:absolutePath];
}

static WTR::TestOptions testOptionsForTest(const WTR::TestCommand& command)
{
    WTR::TestFeatures features = WTR::TestOptions::defaults();
    WTR::merge(features, WTR::hardcodedFeaturesBasedOnPathForTest(command));
    WTR::merge(features, WTR::featureDefaultsFromTestHeaderForTest(command, WTR::TestOptions::keyTypeMapping()));

    return WTR::TestOptions { WTFMove(features) };
}

static void runTest(const std::string& inputLine)
{
    ASSERT(!inputLine.empty());

    auto command = WTR::parseInputLine(inputLine);
    auto pathOrURL = command.pathOrURL;
    dumpPixelsForCurrentTest = command.shouldDumpPixels || dumpPixelsForAllTests;

    NSString *pathOrURLString = [NSString stringWithUTF8String:pathOrURL.c_str()];
    if (!pathOrURLString) {
        fprintf(stderr, "Failed to parse \"%s\" as UTF-8\n", pathOrURL.c_str());
        return;
    }

    NSString *testPath;
    NSURL *url = computeTestURL(pathOrURLString, &testPath);
    if (!url) {
        fprintf(stderr, "Failed to parse \"%s\" as a URL\n", pathOrURL.c_str());
        return;
    }
    if (!testPath)
        testPath = [url absoluteString];

    auto message = makeString("CRASHING TEST: ", testPath.UTF8String);
    WTF::setCrashLogMessage(message.utf8().data());

    auto options = testOptionsForTest(command);

    if (!mainFrameTestOptions || !options.webViewIsCompatibleWithOptions(mainFrameTestOptions.value()))
        createGlobalWebViewAndOffscreenWindow();

    mainFrameTestOptions = options;

    const char* testURL([[url absoluteString] UTF8String]);
    gTestRunner = TestRunner::create(testURL, command.expectedPixelHash);
    gTestRunner->setAllowAnyHTTPSCertificateForAllowedHosts(allowAnyHTTPSCertificateForAllowedHosts);
    gTestRunner->setAllowedHosts(allowedHosts);
    gTestRunner->setLocalhostAliases(localhostAliases);
    gTestRunner->setCustomTimeout(command.timeout.milliseconds());
    gTestRunner->setDumpJSConsoleLogInStdErr(command.dumpJSConsoleLogInStdErr || options.dumpJSConsoleLogInStdErr());

    resetWebViewToConsistentState(options, ResetTime::BeforeTest);

#if !PLATFORM(IOS_FAMILY)
    changeWindowScaleIfNeeded(testURL);
#endif

    topLoadingFrame = nil;
#if !PLATFORM(IOS_FAMILY)
    ASSERT(!draggingInfo); // the previous test should have called eventSender.mouseUp to drop!
    draggingInfo = nil;
#endif
    done = NO;

    sizeWebViewForCurrentTest();
    gTestRunner->setIconDatabaseEnabled(false);
    gTestRunner->clearAllApplicationCaches();

    gTestRunner->clearAllDatabases();
    gTestRunner->clearNotificationPermissionState();

    if (disallowedURLs)
        CFSetRemoveAllValues(disallowedURLs.get());
    if (shouldLogFrameLoadDelegates(pathOrURL.c_str()))
        gTestRunner->setDumpFrameLoadCallbacks(true);

    if (shouldLogHistoryDelegates(pathOrURL.c_str()))
        [[mainFrame webView] setHistoryDelegate:historyDelegate().get()];
    else
        [[mainFrame webView] setHistoryDelegate:nil];

    if (shouldDumpAsText(pathOrURL.c_str())) {
        gTestRunner->setDumpAsText(true);
        gTestRunner->setGeneratePixelResults(false);
    }

#if PLATFORM(IOS_FAMILY)
    if (shouldMakeViewportFlexible(pathOrURL.c_str()))
        adjustWebDocumentForFlexibleViewport(gWebBrowserView.get(), gWebScrollView.get());
#endif

    if (shouldUseEphemeralSession(pathOrURL.c_str()))
        [[[mainFrame webView] preferences] setPrivateBrowsingEnabled:YES];

    if ([WebHistory optionalSharedHistory])
        [WebHistory setOptionalSharedHistory:nil];

    lastMousePosition = NSZeroPoint;
    lastClickPosition = NSZeroPoint;

    prevTestBFItem() = [[[mainFrame webView] backForwardList] currentItem];

    auto& workQueue = DRT::WorkQueue::singleton();
    workQueue.clear();
    workQueue.setFrozen(false);

    bool ignoreWebCoreNodeLeaks = shouldIgnoreWebCoreNodeLeaks(testURL);
    if (ignoreWebCoreNodeLeaks)
        [WebCoreStatistics startIgnoringWebCoreNodeLeaks];

    @autoreleasepool {
        [mainFrame loadRequest:[NSURLRequest requestWithURL:url]];
    }

    while (!done) {
        @autoreleasepool {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 10, false);
        }
    }

#if PLATFORM(IOS_FAMILY)
    [(DumpRenderTree *)UIApp _waitForWebThread];
    WebThreadLockAfterDelegateCallbacksHaveCompleted();
#endif

    @autoreleasepool {
        [EventSendingController clearSavedEvents];
        [[mainFrame webView] setSelectedDOMRange:nil affinity:NSSelectionAffinityDownstream];

        workQueue.clear();

        // If the test page could have possibly opened the Web Inspector frontend,
        // then try to close it in case it was accidentally left open.
        gTestRunner->closeWebInspector();

#if PLATFORM(MAC)
        // Make sure the WebView is parented, since the test may have unparented it.
        WebView *webView = [mainFrame webView];
        if (![webView superview])
            [[mainWindow contentView] addSubview:webView];
#endif

        for (NSWindow *window in DumpRenderTreeWindow.openWindows) {
            // Don't try to close the main window
            if (window == mainFrame.webView.window)
                continue;

#if !PLATFORM(IOS_FAMILY)
            WebView *webView = [window.contentView.subviews objectAtIndex:0];
#else
            ASSERT([window.contentView isKindOfClass:WebView.class]);
            WebView *webView = (WebView *)window.contentView;
#endif

            [webView close];
            [window close];
        }

        resetWebViewToConsistentState(options, ResetTime::AfterTest);

        // Loading an empty request synchronously replaces the document with a blank one, which is necessary
        // to stop timers, WebSockets and other activity that could otherwise spill output into next test's results.
        [mainFrame loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@""]]];
    }

    // We should only have our main window left open when we're done
    ASSERT(CFArrayGetCount(openWindowsRef) == 1);
    ASSERT(CFArrayGetValueAtIndex(openWindowsRef, 0) == (__bridge CFTypeRef)[[mainFrame webView] window]);

    gTestRunner->cleanup();
    gTestRunner = nullptr;

#if PLATFORM(MAC)
    [DumpRenderTreeDraggingInfo clearAllFilePromiseReceivers];
#endif

    if (ignoreWebCoreNodeLeaks)
        [WebCoreStatistics stopIgnoringWebCoreNodeLeaks];

    if (gcBetweenTests)
        [WebCoreStatistics garbageCollectJavaScriptObjects];
    
    JSC::waitForVMDestruction();

    fputs("#EOF\n", stderr);
    fflush(stderr);
}

void displayWebView()
{
#if !PLATFORM(IOS_FAMILY)
    WebView *webView = [mainFrame webView];
    [webView display];
#else
    [gDrtWindow layoutTilesNow];
    [gDrtWindow setNeedsDisplayInRect:[gDrtWindow frame]];
    [CATransaction flush];
#endif
}

void displayAndTrackRepaintsWebView()
{
    displayWebView();
#if !PLATFORM(IOS_FAMILY)
    // FIXME: Tracking repaints is not specific to Mac. We should enable such support on iOS.
    WebView *webView = [mainFrame webView];
    [webView setTracksRepaints:YES];
    [webView resetTrackedRepaints];
#endif
}

#if !PLATFORM(IOS_FAMILY)
@implementation DumpRenderTreeEvent

+ (NSPoint)mouseLocation
{
    return [[[mainFrame webView] window] convertBaseToScreen:lastMousePosition];
}

@end

@implementation DumpRenderTreeApplication

- (BOOL)isRunning
{
    // <rdar://problem/7686123> Java plug-in freezes unless NSApplication is running
    return YES;
}

@end
#endif
