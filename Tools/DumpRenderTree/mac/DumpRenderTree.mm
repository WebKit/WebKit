/*
 * Copyright (C) 2005-2018 Apple Inc. All rights reserved.
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
#import "ResourceLoadDelegate.h"
#import "TestOptions.h"
#import "TestRunner.h"
#import "UIDelegate.h"
#import "WebArchiveDumpSupport.h"
#import "WebCoreTestSupport.h"
#import "WorkQueue.h"
#import "WorkQueueItem.h"
#import <CoreFoundation/CoreFoundation.h>
#import <JavaScriptCore/Options.h>
#import <JavaScriptCore/TestRunnerUtils.h>
#import <WebCore/LogInitialization.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebKit/DOMElement.h>
#import <WebKit/DOMExtensions.h>
#import <WebKit/DOMRange.h>
#import <WebKit/WKCrashReporter.h>
#import <WebKit/WKRetainPtr.h>
#import <WebKit/WKString.h>
#import <WebKit/WKStringCF.h>
#import <WebKit/WebArchive.h>
#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebCache.h>
#import <WebKit/WebCoreStatistics.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDatabaseManagerPrivate.h>
#import <WebKit/WebDeviceOrientationProviderMock.h>
#import <WebKit/WebDocumentPrivate.h>
#import <WebKit/WebEditingDelegate.h>
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
#import <WebKit/WebViewPrivate.h>
#import <getopt.h>
#import <wtf/Assertions.h>
#import <wtf/FastMalloc.h>
#import <wtf/LoggingAccumulator.h>
#import <wtf/WTFObjCRuntimeExtras.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/RetainPtr.h>
#import <wtf/Threading.h>
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
#import "UIKitTestSPI.h"
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

using namespace std;

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
    contentSize.height = CGRound(max(CGRectGetMaxY(newFrame), scrollViewSize.height));
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

static void runTest(const string& testURL);

// Deciding when it's OK to dump out the state is a bit tricky.  All these must be true:
// - There is no load in progress
// - There is no work queued up (see workQueue var, below)
// - waitToDump==NO.  This means either waitUntilDone was never called, or it was called
//       and notifyDone was called subsequently.
// Note that the call to notifyDone and the end of the load can happen in either order.

volatile bool done;

NavigationController* gNavigationController = nullptr;
RefPtr<TestRunner> gTestRunner;

std::optional<TestOptions> mainFrameTestOptions;
WebFrame *mainFrame = nil;
// This is the topmost frame that is loading, during a given load, or nil when no load is 
// in progress.  Usually this is the same as the main frame, but not always.  In the case
// where a frameset is loaded, and then new content is loaded into one of the child frames,
// that child frame is the "topmost frame that is loading".
WebFrame *topLoadingFrame = nil; // !nil iff a load is in progress

#if PLATFORM(MAC)
NSWindow *mainWindow = nil;
#endif

CFMutableSetRef disallowedURLs= nullptr;
static CFRunLoopTimerRef waitToDumpWatchdog;

// Delegates
static FrameLoadDelegate *frameLoadDelegate;
static UIDelegate *uiDelegate;
static EditingDelegate *editingDelegate;
static ResourceLoadDelegate *resourceLoadDelegate;
static HistoryDelegate *historyDelegate;
PolicyDelegate *policyDelegate = nullptr;
DefaultPolicyDelegate *defaultPolicyDelegate = nullptr;
#if PLATFORM(IOS_FAMILY)
static ScrollViewResizerDelegate *scrollViewResizerDelegate;
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
static int showWebView;
static int printTestCount;
static int checkForWorldLeaks;
static BOOL printSeparators;
static RetainPtr<CFStringRef> persistentUserStyleSheetLocation;
static std::set<std::string> allowedHosts;

static WebHistoryItem *prevTestBFItem; // current b/f item at the end of the previous test

#if PLATFORM(IOS_FAMILY)
const CGRect layoutTestViewportRect = { {0, 0}, {static_cast<CGFloat>(TestRunner::viewWidth), static_cast<CGFloat>(TestRunner::viewHeight)} };
DumpRenderTreeBrowserView *gWebBrowserView = nil;
DumpRenderTreeWebScrollView *gWebScrollView = nil;
DumpRenderTreeWindow *gDrtWindow = nil;
#endif

#ifdef __OBJC2__
static void swizzleAllMethods(Class imposter, Class original)
{
    unsigned int imposterMethodCount;
    Method* imposterMethods = class_copyMethodList(imposter, &imposterMethodCount);

    unsigned int originalMethodCount;
    Method* originalMethods = class_copyMethodList(original, &originalMethodCount);

    for (unsigned int i = 0; i < imposterMethodCount; i++) {
        SEL imposterMethodName = method_getName(imposterMethods[i]);

        // Attempt to add the method to the original class.  If it fails, the method already exists and we should
        // instead exchange the implementations.
        if (class_addMethod(original, imposterMethodName, method_getImplementation(imposterMethods[i]), method_getTypeEncoding(imposterMethods[i])))
            continue;

        unsigned int j = 0;
        for (; j < originalMethodCount; j++) {
            SEL originalMethodName = method_getName(originalMethods[j]);
            if (sel_isEqual(imposterMethodName, originalMethodName))
                break;
        }

        // If class_addMethod failed above then the method must exist on the original class.
        ASSERT(j < originalMethodCount);
        method_exchangeImplementations(imposterMethods[i], originalMethods[j]);
    }

    free(imposterMethods);
    free(originalMethods);
}
#endif

static void poseAsClass(const char* imposter, const char* original)
{
    Class imposterClass = objc_getClass(imposter);
    Class originalClass = objc_getClass(original);

#ifndef __OBJC2__
    class_poseAs(imposterClass, originalClass);
#else

    // Swizzle instance methods
    swizzleAllMethods(imposterClass, originalClass);
    // and then class methods
    swizzleAllMethods(object_getClass(imposterClass), object_getClass(originalClass));
#endif
}

void setPersistentUserStyleSheetLocation(CFStringRef url)
{
    persistentUserStyleSheetLocation = url;
}

static bool shouldIgnoreWebCoreNodeLeaks(const string& URLString)
{
    static char* const ignoreSet[] = {
        // Keeping this infrastructure around in case we ever need it again.
    };
    static const int ignoreSetCount = sizeof(ignoreSet) / sizeof(char*);

    for (int i = 0; i < ignoreSetCount; i++) {
        // FIXME: ignore case
        string curIgnore(ignoreSet[i]);
        // Match at the end of the URLString
        if (!URLString.compare(URLString.length() - curIgnore.length(), curIgnore.length(), curIgnore))
            return true;
    }
    return false;
}

#if !PLATFORM(IOS_FAMILY)
static NSSet *allowedFontFamilySet()
{
    static NSSet *fontFamilySet = [[NSSet setWithObjects:
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
        @"Hiragino Sans GB",
        @"Hoefler Text",
        @"Impact",
        @"InaiMathi",
        @"Kai",
        @"Kailasa",
        @"Kokonor",
        @"Krungthep",
        @"KufiStandardGK",
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
        @"Plantagenet Cherokee",
        @"Raanana",
        @"Sathu",
        @"Silom",
        @"Skia",
        @"Songti SC",
        @"Songti TC",
        @"STFangsong",
        @"STHeiti",
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
        nil] retain];

    return fontFamilySet;
}

static NSArray *fontWhitelist()
{
    static NSArray *availableFonts;
    if (availableFonts)
        return availableFonts;

    NSMutableArray *availableFontList = [[NSMutableArray alloc] init];
    for (NSString *fontFamily in allowedFontFamilySet()) {
        NSArray* fontsForFamily = [[NSFontManager sharedFontManager] availableMembersOfFontFamily:fontFamily];
        [availableFontList addObject:fontFamily];
        for (NSArray* fontInfo in fontsForFamily) {
            // Font name is the first entry in the array.
            [availableFontList addObject:[fontInfo objectAtIndex:0]];
        }
    }

    availableFonts = availableFontList;
    return availableFonts;
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
    static const char* fontFileNames[] = {
        "AHEM____.TTF",
        "WebKitWeightWatcher100.ttf",
        "WebKitWeightWatcher200.ttf",
        "WebKitWeightWatcher300.ttf",
        "WebKitWeightWatcher400.ttf",
        "WebKitWeightWatcher500.ttf",
        "WebKitWeightWatcher600.ttf",
        "WebKitWeightWatcher700.ttf",
        "WebKitWeightWatcher800.ttf",
        "WebKitWeightWatcher900.ttf",
        "FontWithFeatures.ttf",
        "FontWithFeatures.otf",
        0
    };

    NSMutableArray *fontURLs = [NSMutableArray array];
    NSURL *resourcesDirectory = [NSURL URLWithString:@"DumpRenderTree.resources" relativeToURL:[[NSBundle mainBundle] executableURL]];
    for (unsigned i = 0; fontFileNames[i]; ++i) {
        NSURL *fontURL = [resourcesDirectory URLByAppendingPathComponent:[NSString stringWithUTF8String:fontFileNames[i]] isDirectory:NO];
        [fontURLs addObject:[fontURL absoluteURL]];
    }

    CFArrayRef errors = 0;
    if (!CTFontManagerRegisterFontsForURLs((CFArrayRef)fontURLs, kCTFontManagerScopeProcess, &errors)) {
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
    CGDataProviderRef data = CGDataProviderCreateWithData(nullptr, fontData, length, nullptr);
    if (!data) {
        fprintf(stderr, "Failed to create CGDataProviderRef for the %s font.\n", sectionName.c_str());
        exit(1);
    }

    CGFontRef cgFont = CGFontCreateWithDataProvider(data);
    CGDataProviderRelease(data);
    if (!cgFont) {
        fprintf(stderr, "Failed to create CGFontRef for the %s font.\n", sectionName.c_str());
        exit(1);
    }

    CFErrorRef error = nullptr;
    CTFontManagerRegisterGraphicsFont(cgFont, &error);
    if (error) {
        fprintf(stderr, "Failed to add CGFont to CoreText for the %s font: %s.\n", sectionName.c_str(), CFStringGetCStringPtr(CFErrorCopyDescription(error), kCFStringEncodingUTF8));
        exit(1);
    }
    CGFontRelease(cgFont);
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
    [webBrowserView setDelegate:scrollViewResizerDelegate];

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
    CGFloat knobLength = max(minKnobSize, static_cast<CGFloat>(round(trackLength * [self knobProportion])));
    CGFloat knobPosition = static_cast<CGFloat>((round([self doubleValue] * (trackLength - knobLength))));

    if (isHorizontal)
        return NSMakeRect(bounds.origin.x + knobPosition, bounds.origin.y, knobLength, bounds.size.height);

    return NSMakeRect(bounds.origin.x, bounds.origin.y +  + knobPosition, bounds.size.width, knobLength);
}

- (void)drawKnob
{
    if (![self isEnabled])
        return;

    NSRect knobRect = [self rectForPart:NSScrollerKnob];

    static NSColor *knobColor = [[NSColor colorWithDeviceRed:0x80 / 255.0 green:0x80 / 255.0 blue:0x80 / 255.0 alpha:1] retain];
    [knobColor set];

    NSRectFill(knobRect);
}

- (void)drawRect:(NSRect)dirtyRect
{
    static NSColor *trackColor = [[NSColor colorWithDeviceRed:0xC0 / 255.0 green:0xC0 / 255.0 blue:0xC0 / 255.0 alpha:1] retain];
    static NSColor *disabledTrackColor = [[NSColor colorWithDeviceRed:0xE0 / 255.0 green:0xE0 / 255.0 blue:0xE0 / 255.0 alpha:1] retain];

    if ([self isEnabled])
        [trackColor set];
    else
        [disabledTrackColor set];

    NSRectFill(dirtyRect);

    [self drawKnob];
}

@end

static void registerMockScrollbars()
{
    [WebDynamicScrollBarsView setCustomScrollerClass:[DRTMockScroller class]];
}
#endif

WebView *createWebViewAndOffscreenWindow()
{
#if !PLATFORM(IOS_FAMILY)
    NSRect rect = NSMakeRect(0, 0, TestRunner::viewWidth, TestRunner::viewHeight);
    WebView *webView = [[WebView alloc] initWithFrame:rect frameName:nil groupName:@"org.webkit.DumpRenderTree"];
#else
    DumpRenderTreeBrowserView *webBrowserView = [[[DumpRenderTreeBrowserView alloc] initWithFrame:layoutTestViewportRect] autorelease];
    [webBrowserView setInputViewObeysDOMFocus:YES];
    WebView *webView = [[webBrowserView webView] retain];
    [webView setGroupName:@"org.webkit.DumpRenderTree"];
#endif

    [webView setUIDelegate:uiDelegate];
    [webView setFrameLoadDelegate:frameLoadDelegate];
    [webView setEditingDelegate:editingDelegate];
    [webView setResourceLoadDelegate:resourceLoadDelegate];
    [webView _setGeolocationProvider:[MockGeolocationProvider shared]];
    [webView _setDeviceOrientationProvider:[WebDeviceOrientationProviderMock shared]];
    [webView _setNotificationProvider:[MockWebNotificationProvider shared]];

    // Register the same schemes that Safari does
    [WebView registerURLSchemeAsLocal:@"feed"];
    [WebView registerURLSchemeAsLocal:@"feeds"];
    [WebView registerURLSchemeAsLocal:@"feedsearch"];

#if PLATFORM(MAC)
    [WebView _setFontWhitelist:fontWhitelist()];
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
    DumpRenderTreeWindow *window = [[DumpRenderTreeWindow alloc] initWithContentRect:windowRect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];
    mainWindow = window;

    [window setColorSpace:[firstScreen colorSpace]];
    [window setCollectionBehavior:NSWindowCollectionBehaviorStationary];
    [[window contentView] addSubview:webView];
    if (showWebView)
        [window orderFront:nil];
    else
        [window orderBack:nil];
    [window setAutodisplay:NO];

    [window startListeningForAcceleratedCompositingChanges];
#else
    DumpRenderTreeWindow *drtWindow = [[DumpRenderTreeWindow alloc] initWithLayer:[webBrowserView layer]];
    [drtWindow setContentView:webView];
    [webBrowserView setWAKWindow:drtWindow];

    [[webView window] makeFirstResponder:[[[webView mainFrame] frameView] documentView]];

    CGRect uiWindowRect = layoutTestViewportRect;
    uiWindowRect.origin.y += [UIApp statusBarHeight];
    UIWindow *uiWindow = [[[UIWindow alloc] initWithFrame:uiWindowRect] autorelease];

    UIViewController *viewController = [[UIViewController alloc] init];
    [uiWindow setRootViewController:viewController];
    [viewController release];

    // The UIWindow and UIWebBrowserView are released when the DumpRenderTreeWindow is closed.
    drtWindow.uiWindow = uiWindow;
    drtWindow.browserView = webBrowserView;

    DumpRenderTreeWebScrollView *scrollView = [[DumpRenderTreeWebScrollView alloc] initWithFrame:layoutTestViewportRect];
    [scrollView addSubview:webBrowserView];

    [viewController.view addSubview:scrollView];
    [scrollView release];

    adjustWebDocumentForStandardViewport(webBrowserView, scrollView);
#endif

#if !PLATFORM(IOS_FAMILY)
    // For reasons that are not entirely clear, the following pair of calls makes WebView handle its
    // dynamic scrollbars properly. Without it, every frame will always have scrollbars.
    NSBitmapImageRep *imageRep = [webView bitmapImageRepForCachingDisplayInRect:[webView bounds]];
    [webView cacheDisplayInRect:[webView bounds] toBitmapImageRep:imageRep];
#else
    [[webView preferences] _setTelephoneNumberParsingEnabled:NO];

    // Initialize the global UIViews, and set the key UIWindow to be painted.
    if (!gWebBrowserView) {
        gWebBrowserView = [webBrowserView retain];
        gWebScrollView = [scrollView retain];
        gDrtWindow = [drtWindow retain];
        [uiWindow makeKeyAndVisible];
        [uiWindow retain];
    }
#endif

    [webView setMediaVolume:0];

    return webView;
}

static void destroyWebViewAndOffscreenWindow(WebView *webView)
{
    ASSERT(webView == [mainFrame webView]);
#if !PLATFORM(IOS_FAMILY)
    NSWindow *window = [webView window];
#endif
    [webView close];
    mainFrame = nil;

#if !PLATFORM(IOS_FAMILY)
    // Work around problem where registering drag types leaves an outstanding
    // "perform selector" on the window, which retains the window. It's a bit
    // inelegant and perhaps dangerous to just blow them all away, but in practice
    // it probably won't cause any trouble (and this is just a test tool, after all).
    [NSObject cancelPreviousPerformRequestsWithTarget:window];

    [window close]; // releases when closed
#else
    UIWindow *uiWindow = [gWebBrowserView window];
    [uiWindow removeFromSuperview];
    [uiWindow release];
#endif

    [webView release];
}

static NSString *libraryPathForDumpRenderTree()
{
    char* dumpRenderTreeTemp = getenv("DUMPRENDERTREE_TEMP");
    if (dumpRenderTreeTemp)
        return [[NSFileManager defaultManager] stringWithFileSystemRepresentation:dumpRenderTreeTemp length:strlen(dumpRenderTreeTemp)];
    else
        return [@"~/Library/Application Support/DumpRenderTree" stringByExpandingTildeInPath];
}

static void enableExperimentalFeatures(WebPreferences* preferences)
{
    // FIXME: SpringTimingFunction
    [preferences setGamepadsEnabled:YES];
    [preferences setLinkPreloadEnabled:YES];
    [preferences setMediaPreloadingEnabled:YES];
    // FIXME: InputEvents
    [preferences setFetchAPIKeepAliveEnabled:YES];
    [preferences setWebAnimationsEnabled:YES];
    [preferences setWebGL2Enabled:YES];
    [preferences setWebMetalEnabled:YES];
    // FIXME: AsyncFrameScrollingEnabled
    [preferences setWebAuthenticationEnabled:NO];
    [preferences setCacheAPIEnabled:NO];
    [preferences setReadableByteStreamAPIEnabled:YES];
    [preferences setWritableStreamAPIEnabled:YES];
    preferences.encryptedMediaAPIEnabled = YES;
    [preferences setAccessibilityObjectModelEnabled:YES];
    [preferences setAriaReflectionEnabled:YES];
    [preferences setVisualViewportAPIEnabled:YES];
    [preferences setColorFilterEnabled:YES];
    [preferences setServerTimingEnabled:YES];
    [preferences setIntersectionObserverEnabled:YES];
    preferences.sourceBufferChangeTypeEnabled = YES;
    [preferences setCSSOMViewScrollingAPIEnabled:YES];
    [preferences setMediaRecorderEnabled:YES];
}

// Called before each test.
static void resetWebPreferencesToConsistentValues()
{
    WebPreferences *preferences = [WebPreferences standardPreferences];
    enableExperimentalFeatures(preferences);

    [preferences setNeedsStorageAccessFromFileURLsQuirk: NO];
    [preferences setAllowUniversalAccessFromFileURLs:YES];
    [preferences setAllowFileAccessFromFileURLs:YES];
    [preferences setStandardFontFamily:@"Times"];
    [preferences setFixedFontFamily:@"Courier"];
    [preferences setSerifFontFamily:@"Times"];
    [preferences setSansSerifFontFamily:@"Helvetica"];
    [preferences setCursiveFontFamily:@"Apple Chancery"];
    [preferences setFantasyFontFamily:@"Papyrus"];
    [preferences setPictographFontFamily:@"Apple Color Emoji"];
    [preferences setDefaultFontSize:16];
    [preferences setDefaultFixedFontSize:13];
    [preferences setMinimumFontSize:0];
    [preferences setDefaultTextEncodingName:@"ISO-8859-1"];
    [preferences setJavaEnabled:NO];
    [preferences setJavaScriptEnabled:YES];
    [preferences setEditableLinkBehavior:WebKitEditableLinkOnlyLiveWithShiftKey];
#if !PLATFORM(IOS_FAMILY)
    [preferences setTabsToLinks:NO];
#endif
    [preferences setDOMPasteAllowed:YES];
#if !PLATFORM(IOS_FAMILY)
    [preferences setShouldPrintBackgrounds:YES];
#endif
    [preferences setCacheModel:WebCacheModelDocumentBrowser];
    [preferences setXSSAuditorEnabled:NO];
    [preferences setExperimentalNotificationsEnabled:NO];
    [preferences setPlugInsEnabled:YES];
#if !PLATFORM(IOS_FAMILY)
    [preferences setTextAreasAreResizable:YES];
#endif

    [preferences setPrivateBrowsingEnabled:NO];
    [preferences setAuthorAndUserStylesEnabled:YES];
    [preferences setShrinksStandaloneImagesToFit:YES];
    [preferences setJavaScriptCanOpenWindowsAutomatically:YES];
    [preferences setJavaScriptCanAccessClipboard:YES];
    [preferences setOfflineWebApplicationCacheEnabled:YES];
    [preferences setDeveloperExtrasEnabled:NO];
    [preferences setJavaScriptRuntimeFlags:WebKitJavaScriptRuntimeFlagsAllEnabled];
    [preferences setLoadsImagesAutomatically:YES];
    [preferences setLoadsSiteIconsIgnoringImageLoadingPreference:NO];
    [preferences setFrameFlattening:WebKitFrameFlatteningDisabled];
    [preferences setAsyncFrameScrollingEnabled:NO];
    [preferences setSpatialNavigationEnabled:NO];
    [preferences setMetaRefreshEnabled:YES];

    if (persistentUserStyleSheetLocation) {
        [preferences setUserStyleSheetLocation:[NSURL URLWithString:(__bridge NSString *)persistentUserStyleSheetLocation.get()]];
        [preferences setUserStyleSheetEnabled:YES];
    } else
        [preferences setUserStyleSheetEnabled:NO];

    [preferences setMediaPlaybackAllowsInline:YES];
    [preferences setVideoPlaybackRequiresUserGesture:NO];
    [preferences setAudioPlaybackRequiresUserGesture:NO];
    [preferences setMediaDataLoadsAutomatically:YES];
    [preferences setInvisibleAutoplayNotPermitted:NO];
    [preferences setSubpixelAntialiasedLayerTextEnabled:NO];

#if PLATFORM(IOS_FAMILY)
    // Enable the tracker before creating the first WebView will
    // cause initialization to use the correct database paths.
    [preferences setStorageTrackerEnabled:YES];
#endif

    [preferences _setTextAutosizingEnabled:NO];

    // The back/forward cache is causing problems due to layouts during transition from one page to another.
    // So, turn it off for now, but we might want to turn it back on some day.
    [preferences setUsesPageCache:NO];
    [preferences setAcceleratedCompositingEnabled:YES];
#if USE(CA)
    [preferences setCanvasUsesAcceleratedDrawing:YES];
    [preferences setAcceleratedDrawingEnabled:useAcceleratedDrawing];
#endif
    [preferences setUsePreHTML5ParserQuirks:NO];
    [preferences setAsynchronousSpellCheckingEnabled:NO];
#if !PLATFORM(IOS_FAMILY)
    ASSERT([preferences mockScrollbarsEnabled]);
#endif

    [preferences setWebAudioEnabled:YES];
    [preferences setMediaSourceEnabled:YES];
    [preferences setSourceBufferChangeTypeEnabled:YES];

    [preferences setShadowDOMEnabled:YES];
    [preferences setCustomElementsEnabled:YES];

    [preferences setDataTransferItemsEnabled:YES];
    [preferences setCustomPasteboardDataEnabled:YES];

    [preferences setWebGL2Enabled:YES];
    [preferences setWebMetalEnabled:YES];

    [preferences setDownloadAttributeEnabled:YES];
    [preferences setDirectoryUploadEnabled:YES];

    [preferences setHiddenPageDOMTimerThrottlingEnabled:NO];
    [preferences setHiddenPageCSSAnimationSuspensionEnabled:NO];

    [preferences setMediaDevicesEnabled:YES];

    [preferences setLargeImageAsyncDecodingEnabled:NO];

    [preferences setModernMediaControlsEnabled:YES];
    [preferences setResourceTimingEnabled:YES];
    [preferences setUserTimingEnabled:YES];

    [preferences setCacheAPIEnabled:NO];
    preferences.mediaCapabilitiesEnabled = YES;

    preferences.selectionAcrossShadowBoundariesEnabled = YES;

    [WebPreferences _clearNetworkLoaderSession];
    [WebPreferences _setCurrentNetworkLoaderSessionCookieAcceptPolicy:NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain];
}

static void setWebPreferencesForTestOptions(const TestOptions& options)
{
    WebPreferences *preferences = [WebPreferences standardPreferences];

    preferences.attachmentElementEnabled = options.enableAttachmentElement;
    preferences.acceleratedDrawingEnabled = options.useAcceleratedDrawing;
    preferences.menuItemElementEnabled = options.enableMenuItemElement;
    preferences.modernMediaControlsEnabled = options.enableModernMediaControls;
    preferences.webAuthenticationEnabled = options.enableWebAuthentication;
    preferences.isSecureContextAttributeEnabled = options.enableIsSecureContextAttribute;
    preferences.inspectorAdditionsEnabled = options.enableInspectorAdditions;
    preferences.allowCrossOriginSubresourcesToAskForCredentials = options.allowCrossOriginSubresourcesToAskForCredentials;
    preferences.webAnimationsCSSIntegrationEnabled = options.enableWebAnimationsCSSIntegration;
    preferences.colorFilterEnabled = options.enableColorFilter;
    preferences.selectionAcrossShadowBoundariesEnabled = options.enableSelectionAcrossShadowBoundaries;
    preferences.webGPUEnabled = options.enableWebGPU;
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
        WebKitFullScreenEnabledPreferenceKey: @YES,
        WebKitAllowsInlineMediaPlaybackPreferenceKey: @YES,
        WebKitInlineMediaPlaybackRequiresPlaysInlineAttributeKey: @NO,
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
    gNavigationController = [[NavigationController alloc] init];
    frameLoadDelegate = [[FrameLoadDelegate alloc] init];
    uiDelegate = [[UIDelegate alloc] init];
    editingDelegate = [[EditingDelegate alloc] init];
    resourceLoadDelegate = [[ResourceLoadDelegate alloc] init];
    policyDelegate = [[PolicyDelegate alloc] init];
    historyDelegate = [[HistoryDelegate alloc] init];
    defaultPolicyDelegate = [[DefaultPolicyDelegate alloc] init];
#if PLATFORM(IOS_FAMILY)
    scrollViewResizerDelegate = [[ScrollViewResizerDelegate alloc] init];
#endif
}

// ObjC++ doens't seem to let me pass NSObject*& sadly.
static inline void releaseAndZero(NSObject** object)
{
    [*object release];
    *object = nil;
}

static void releaseGlobalControllers()
{
    releaseAndZero(&gNavigationController);
    releaseAndZero(&frameLoadDelegate);
    releaseAndZero(&editingDelegate);
    releaseAndZero(&resourceLoadDelegate);
    releaseAndZero(&uiDelegate);
    releaseAndZero(&policyDelegate);
#if PLATFORM(IOS_FAMILY)
    releaseAndZero(&scrollViewResizerDelegate);
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
        {"show-webview", no_argument, &showWebView, YES},
        {"print-test-count", no_argument, &printTestCount, YES},
        {"world-leaks", no_argument, &checkForWorldLeaks, NO},
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
        }
    }
}

static void addTestPluginsToPluginSearchPath(const char* executablePath)
{
#if !PLATFORM(IOS_FAMILY)
    NSString *pwd = [[NSString stringWithUTF8String:executablePath] stringByDeletingLastPathComponent];
    [WebPluginDatabase setAdditionalWebPlugInPaths:[NSArray arrayWithObject:pwd]];
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
    if (!strcmp("#CHECK FOR WORLD LEAKS", command)) {
        // DumpRenderTree does not support checking for world leaks.
        WTF::String result("\n");
        printf("Content-Type: text/plain\n");
        printf("Content-Length: %u\n", result.length());
        fwrite(result.utf8().data(), 1, result.length(), stdout);
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
#if !PLATFORM(IOS_FAMILY)
    poseAsClass("DumpRenderTreePasteboard", "NSPasteboard");
    poseAsClass("DumpRenderTreeEvent", "NSEvent");
#else
    poseAsClass("DumpRenderTreeEvent", "GSEvent");

    // Override the implementation of +[UIKeyboard isInHardwareKeyboardMode] to ensure that test runs are deterministic
    // regardless of whether a hardware keyboard is attached. We intentionally never restore the original implementation.
    method_setImplementation(class_getClassMethod([UIKeyboard class], @selector(isInHardwareKeyboardMode)), reinterpret_cast<IMP>(overrideIsInHardwareKeyboardMode));
#endif

    [[WebPreferences standardPreferences] setAutosaves:NO];

    // +[WebPreferences _switchNetworkLoaderToNewTestingSession] calls +[NSURLCache sharedURLCache], which initializes a default cache on disk.
    // Making the shared cache memory-only avoids touching the file system.
    RetainPtr<NSURLCache> sharedCache =
        adoptNS([[NSURLCache alloc] initWithMemoryCapacity:1024 * 1024
                                      diskCapacity:0
                                          diskPath:nil]);
    [NSURLCache setSharedURLCache:sharedCache.get()];

    [WebPreferences _switchNetworkLoaderToNewTestingSession];

#if !PLATFORM(IOS_FAMILY)
    adjustFonts();
    registerMockScrollbars();

    // The mock scrollbars setting cannot be modified after creating a view, so we have to do it now.
    [[WebPreferences standardPreferences] setMockScrollbarsEnabled:YES];
#else
    activateFontsIOS();
#endif

    allocateGlobalControllers();

#if PLATFORM(MAC)
    NSActivityOptions options = (NSActivityUserInitiatedAllowingIdleSystemSleep | NSActivityLatencyCritical) & ~(NSActivitySuddenTerminationDisabled | NSActivityAutomaticTerminationDisabled);
    static id assertion = [[[NSProcessInfo processInfo] beginActivityWithOptions:options reason:@"DumpRenderTree should not be subject to process suppression"] retain];
    ASSERT_UNUSED(assertion, assertion);
#endif
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

    if (forceComplexText)
        [WebView _setAlwaysUsesComplexTextCodePath:YES];

#if USE(APPKIT)
    [NSSound _setAlertType:0];
#endif

    [[NSURLCache sharedURLCache] removeAllCachedResponses];
    [WebCache empty];

    [NSURLRequest setAllowsAnyHTTPSCertificate:YES forHost:@"localhost"];
    [NSURLRequest setAllowsAnyHTTPSCertificate:YES forHost:@"127.0.0.1"];
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

    destroyWebViewAndOffscreenWindow([mainFrame webView]);

    releaseGlobalControllers();

#if !PLATFORM(IOS_FAMILY)
    [DumpRenderTreePasteboard releaseLocalPasteboards];
#endif

    // FIXME: This should be moved onto TestRunner and made into a HashSet
    if (disallowedURLs) {
        CFRelease(disallowedURLs);
        disallowedURLs = 0;
    }

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
    dispatch_async(dispatch_get_main_queue(), ^{
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

#if PLATFORM(IOS_FAMILY)
    _UIApplicationLoadWebKit();
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
    const vector<char>& dataVector = gTestRunner->audioResult();

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

static NSString *dumpFramesAsText(WebFrame *frame)
{
    DOMDocument *document = [frame DOMDocument];
    DOMElement *documentElement = [document documentElement];

    if (!documentElement)
        return @"";

    NSMutableString *result = [[[NSMutableString alloc] init] autorelease];

    // Add header for all but the main frame.
    if ([frame parentFrame])
        result = [NSMutableString stringWithFormat:@"\n--------\nFrame: '%@'\n--------\n", [frame name]];

    NSString *innerText = [documentElement innerText];
    // We use WKStringGetUTF8CStringNonStrict() to convert innerText to a WK String since
    // WKStringGetUTF8CStringNonStrict() can handle dangling surrogates and the NSString
    // conversion methods cannot. After the conversion to a buffer, we turn that buffer into
    // a CFString via fromUTF8WithLatin1Fallback().createCFString() which can be appended to
    // the result without any conversion.
    WKRetainPtr<WKStringRef> stringRef(AdoptWK, WKStringCreateWithCFString((__bridge CFStringRef)innerText));
    size_t bufferSize = WKStringGetMaximumUTF8CStringSize(stringRef.get());
    auto buffer = std::make_unique<char[]>(bufferSize);
    size_t stringLength = WKStringGetUTF8CStringNonStrict(stringRef.get(), buffer.get(), bufferSize);
    [result appendFormat:@"%@\n", String::fromUTF8WithLatin1Fallback(buffer.get(), stringLength - 1).createCFString().get()];

    if (gTestRunner->dumpChildFramesAsText()) {
        NSArray *kids = [frame childFrames];
        if (kids) {
            for (unsigned i = 0; i < [kids count]; i++)
                [result appendString:dumpFramesAsText([kids objectAtIndex:i])];
        }
    }

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

    NSPrintInfo *printInfo = [[NSPrintInfo alloc] initWithDictionary:printInfoDict];
    [printInfo setHorizontalPagination:NSAutoPagination];
    [printInfo setVerticalPagination:NSAutoPagination];
    [printInfo setVerticallyCentered:NO];

    NSPrintOperation *printOperation = [NSPrintOperation printOperationWithView:[frame frameView] printInfo:printInfo];
    [printOperation setShowPanels:NO];
    [printOperation runOperation];

    [printInfo release];

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
    NSMutableArray *itemsToPrint = [[NSMutableArray alloc] init];
    for (int i = [bfList forwardListCount]; i > 0; i--) {
        WebHistoryItem *item = [bfList itemAtIndex:i];
        // something is wrong if the item from the last test is in the forward part of the b/f list
        assert(item != prevTestBFItem);
        [itemsToPrint addObject:item];
    }

    assert([bfList currentItem] != prevTestBFItem);
    [itemsToPrint addObject:[bfList currentItem]];
    int currentItemIndex = [itemsToPrint count] - 1;

    for (int i = -1; i >= -[bfList backListCount]; i--) {
        WebHistoryItem *item = [bfList itemAtIndex:i];
        if (item == prevTestBFItem)
            break;
        [itemsToPrint addObject:item];
    }

    for (int i = [itemsToPrint count]-1; i >= 0; i--)
        dumpHistoryItem([itemsToPrint objectAtIndex:i], 8, i == currentItemIndex);

    [itemsToPrint release];
    printf("===============================================\n");
}

#if !PLATFORM(IOS_FAMILY)
static void changeWindowScaleIfNeeded(const char* testPathOrUR)
{
    WTF::String localPathOrUrl = String(testPathOrUR);
    float currentScaleFactor = [[[mainFrame webView] window] backingScaleFactor];
    float requiredScaleFactor = 1;
    if (localPathOrUrl.containsIgnoringASCIICase("/hidpi-3x-"))
        requiredScaleFactor = 3;
    else if (localPathOrUrl.containsIgnoringASCIICase("/hidpi-"))
        requiredScaleFactor = 2;
    if (currentScaleFactor == requiredScaleFactor)
        return;
    // When the new scale factor is set on the window first, WebView doesn't see it as a new scale and stops propagating the behavior change to WebCore::Page.
    gTestRunner->setBackingScaleFactor(requiredScaleFactor);
    [[[mainFrame webView] window] _setWindowResolution:requiredScaleFactor displayIfChanged:YES];
}
#endif

static void sizeWebViewForCurrentTest()
{
    [uiDelegate resetWindowOrigin];

    // W3C SVG tests expect to be 480x360
    bool isSVGW3CTest = (gTestRunner->testURL().find("svg/W3C-SVG-1.1") != string::npos);
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
        CFRunLoopTimerInvalidate(waitToDumpWatchdog);
        CFRelease(waitToDumpWatchdog);
        waitToDumpWatchdog = 0;
    }
}

void setWaitToDumpWatchdog(CFRunLoopTimerRef timer)
{
    ASSERT(timer);
    ASSERT(shouldSetWaitToDumpWatchdog());
    waitToDumpWatchdog = timer;
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), waitToDumpWatchdog, kCFRunLoopCommonModes);
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

    updateDisplay();

    invalidateAnyPreviousWaitToDumpWatchdog();
    ASSERT(!gTestRunner->hasPendingWebNotificationClick());

    if (dumpTree) {
        NSString *resultString = nil;
        NSData *resultData = nil;
        NSString *resultMimeType = @"text/plain";

        if ([[[mainFrame dataSource] _responseMIMEType] isEqualToString:@"text/plain"]) {
            gTestRunner->setDumpAsText(true);
            gTestRunner->setGeneratePixelResults(false);
        }
        if (gTestRunner->dumpAsAudio()) {
            resultData = dumpAudio();
            resultMimeType = @"audio/wav";
        } else if (gTestRunner->dumpAsText()) {
            resultString = dumpFramesAsText(mainFrame);
        } else if (gTestRunner->dumpAsPDF()) {
            resultData = dumpFrameAsPDF(mainFrame);
            resultMimeType = @"application/pdf";
        } else if (gTestRunner->dumpDOMAsWebArchive()) {
            WebArchive *webArchive = [[mainFrame DOMDocument] webArchive];
            resultString = CFBridgingRelease(WebCoreTestSupport::createXMLStringFromWebArchiveData((__bridge CFDataRef)[webArchive data]));
            resultMimeType = @"application/x-webarchive";
        } else if (gTestRunner->dumpSourceAsWebArchive()) {
            WebArchive *webArchive = [[mainFrame dataSource] webArchive];
            resultString = CFBridgingRelease(WebCoreTestSupport::createXMLStringFromWebArchiveData((__bridge CFDataRef)[webArchive data]));
            resultMimeType = @"application/x-webarchive";
        } else
            resultString = [mainFrame renderTreeAsExternalRepresentationForPrinting:gTestRunner->isPrinting()];

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
    return strstr(pathOrURL, "loading/");
}

static bool shouldLogHistoryDelegates(const char* pathOrURL)
{
    return strstr(pathOrURL, "globalhistory/");
}

static bool shouldDumpAsText(const char* pathOrURL)
{
    return strstr(pathOrURL, "dumpAsText/");
}

static bool shouldEnableDeveloperExtras(const char* pathOrURL)
{
    return true;
}

#if PLATFORM(IOS_FAMILY)
static bool shouldMakeViewportFlexible(const char* pathOrURL)
{
    return strstr(pathOrURL, "viewport/") && !strstr(pathOrURL, "visual-viewport/");
}
#endif

static void setJSCOptions(const TestOptions& options)
{
    static WTF::StringBuilder savedOptions;

    if (!savedOptions.isEmpty()) {
        JSC::Options::setOptions(savedOptions.toString().ascii().data());
        savedOptions.clear();
    }

    if (options.jscOptions.length()) {
        JSC::Options::dumpAllOptionsInALine(savedOptions);
        JSC::Options::setOptions(options.jscOptions.c_str());
    }
}

static void resetWebViewToConsistentStateBeforeTesting(const TestOptions& options)
{
    WebView *webView = [mainFrame webView];

#if PLATFORM(IOS_FAMILY)
    adjustWebDocumentForStandardViewport(gWebBrowserView, gWebScrollView);
    [webView _setAllowsMessaging:YES];
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
    [webView setPolicyDelegate:defaultPolicyDelegate];
    [policyDelegate setPermissive:NO];
    [policyDelegate setControllerToNotifyDone:0];
    [uiDelegate resetToConsistentStateBeforeTesting:options];
    [frameLoadDelegate resetToConsistentState];
#if !PLATFORM(IOS_FAMILY)
    [webView _setDashboardBehavior:WebDashboardBehaviorUseBackwardCompatibilityMode to:NO];
#endif
    [webView _clearMainFrameName];
    [[webView undoManager] removeAllActions];
    [WebView _removeAllUserContentFromGroup:[webView groupName]];
#if !PLATFORM(IOS_FAMILY)
    [[webView window] setAutodisplay:NO];
#endif
    [webView setTracksRepaints:NO];

    [WebCache clearCachedCredentials];

    resetWebPreferencesToConsistentValues();
    setWebPreferencesForTestOptions(options);
#if PLATFORM(MAC)
    [webView setWantsLayer:options.layerBackedWebView];
#endif

    TestRunner::setSerializeHTTPLoads(false);
    TestRunner::setAllowsAnySSLCertificate(false);

    setlocale(LC_ALL, "");

    if (gTestRunner) {
        gTestRunner->resetPageVisibility();
        WebCoreTestSupport::resetInternalsObject([mainFrame globalContext]);
        // in the case that a test using the chrome input field failed, be sure to clean up for the next test
        gTestRunner->removeChromeInputField();
    }

#if !PLATFORM(IOS_FAMILY)
    if (WebCore::Frame* frame = [webView _mainCoreFrame])
        WebCoreTestSupport::clearWheelEventTestTrigger(*frame);
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
    [WebView _resetOriginAccessWhitelists];

    [[MockGeolocationProvider shared] stopTimer];
    [[MockWebNotificationProvider shared] reset];

#if !PLATFORM(IOS_FAMILY)
    // Clear the contents of the general pasteboard
    [[NSPasteboard generalPasteboard] declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
#endif

    setJSCOptions(options);

    [mainFrame _clearOpener];

#if PLATFORM(MAC)
    [LayoutTestSpellChecker uninstallAndReset];
#endif

    resetAccumulatedLogs();
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
    dispatch_semaphore_t delegateSemaphore = dispatch_semaphore_create(0);
    WebThreadRun(^{
        dispatch_semaphore_signal(delegateSemaphore);
    });

    while (dispatch_semaphore_wait(delegateSemaphore, DISPATCH_TIME_NOW)) {
        @autoreleasepool {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
        }
    }

    WebThreadLock();

    dispatch_release(delegateSemaphore);
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

static void runTest(const string& inputLine)
{
    ASSERT(!inputLine.empty());

    TestCommand command = parseInputLine(inputLine);
    const string& pathOrURL = command.pathOrURL;
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

    NSString *informationString = [@"CRASHING TEST: " stringByAppendingString:testPath];
    WebKit::setCrashReportApplicationSpecificInformation((__bridge CFStringRef)informationString);

    TestOptions options { [url isFileURL] ? [url fileSystemRepresentation] : pathOrURL, command.absolutePath };

    if (!mainFrameTestOptions || !options.webViewIsCompatibleWithOptions(mainFrameTestOptions.value())) {
        if (mainFrame)
            destroyWebViewAndOffscreenWindow([mainFrame webView]);
        WebView *pristineWebView = createWebViewAndOffscreenWindow();
        mainFrame = [pristineWebView mainFrame];
    }
    mainFrameTestOptions = options;

    resetWebViewToConsistentStateBeforeTesting(options);

    const char* testURL([[url absoluteString] UTF8String]);

#if !PLATFORM(IOS_FAMILY)
    changeWindowScaleIfNeeded(testURL);
#endif

    gTestRunner = TestRunner::create(testURL, command.expectedPixelHash);
    gTestRunner->setAllowedHosts(allowedHosts);
    gTestRunner->setCustomTimeout(command.timeout);
    gTestRunner->setDumpJSConsoleLogInStdErr(command.dumpJSConsoleLogInStdErr || options.dumpJSConsoleLogInStdErr);
    topLoadingFrame = nil;
#if !PLATFORM(IOS_FAMILY)
    ASSERT(!draggingInfo); // the previous test should have called eventSender.mouseUp to drop!
    releaseAndZero(&draggingInfo);
#endif
    done = NO;

    sizeWebViewForCurrentTest();
    gTestRunner->setIconDatabaseEnabled(false);
    gTestRunner->clearAllApplicationCaches();

    if (disallowedURLs)
        CFSetRemoveAllValues(disallowedURLs);
    if (shouldLogFrameLoadDelegates(pathOrURL.c_str()))
        gTestRunner->setDumpFrameLoadCallbacks(true);

    if (shouldLogHistoryDelegates(pathOrURL.c_str()))
        [[mainFrame webView] setHistoryDelegate:historyDelegate];
    else
        [[mainFrame webView] setHistoryDelegate:nil];

    if (shouldEnableDeveloperExtras(pathOrURL.c_str())) {
        gTestRunner->setDeveloperExtrasEnabled(true);
        if (shouldDumpAsText(pathOrURL.c_str())) {
            gTestRunner->setDumpAsText(true);
            gTestRunner->setGeneratePixelResults(false);
        }
    }

#if PLATFORM(IOS_FAMILY)
    if (shouldMakeViewportFlexible(pathOrURL.c_str()))
        adjustWebDocumentForFlexibleViewport(gWebBrowserView, gWebScrollView);
#endif

    if ([WebHistory optionalSharedHistory])
        [WebHistory setOptionalSharedHistory:nil];

    lastMousePosition = NSZeroPoint;
    lastClickPosition = NSZeroPoint;

    [prevTestBFItem release];
    prevTestBFItem = [[[[mainFrame webView] backForwardList] currentItem] retain];

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
        if (shouldEnableDeveloperExtras(pathOrURL.c_str())) {
            gTestRunner->closeWebInspector();
            gTestRunner->setDeveloperExtrasEnabled(false);
        }

#if PLATFORM(MAC)
        // Make sure the WebView is parented, since the test may have unparented it.
        WebView *webView = [mainFrame webView];
        if (![webView superview])
            [[mainWindow contentView] addSubview:webView];
#endif

        if (gTestRunner->closeRemainingWindowsWhenComplete()) {
            NSArray* array = [DumpRenderTreeWindow openWindows];

            unsigned count = [array count];
            for (unsigned i = 0; i < count; i++) {
                NSWindow *window = [array objectAtIndex:i];

                // Don't try to close the main window
                if (window == [[mainFrame webView] window])
                    continue;

#if !PLATFORM(IOS_FAMILY)
                WebView *webView = [[[window contentView] subviews] objectAtIndex:0];
#else
                ASSERT([[window contentView] isKindOfClass:[WebView class]]);
                WebView *webView = (WebView *)[window contentView];
#endif

                [webView close];
                [window close];
            }
        }

        resetWebViewToConsistentStateBeforeTesting(options);

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
