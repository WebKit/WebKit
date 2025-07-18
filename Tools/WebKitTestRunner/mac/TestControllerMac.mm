/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "TestController.h"

#import "LayoutTestSpellChecker.h"
#import "PlatformWebView.h"
#import "PoseAsClass.h"
#import "TestCommand.h"
#import "TestInvocation.h"
#import "TestRunnerWKWebView.h"
#import "WPTFunctions.h"
#import "WebKitTestRunnerPasteboard.h"
#import <WebCore/RunLoopObserver.h>
#import <WebKit/WKContextPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKStringCF.h>
#import <WebKit/WKURLCF.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <mach-o/dyld.h>
#import <pal/spi/mac/NSApplicationSPI.h>

@interface NSMenu ()
- (id)_menuImpl;
@end

@interface NSSound ()
+ (void)_setAlertType:(NSUInteger)alertType;
@end

@interface WKTRSessionDelegate : NSObject <NSURLSessionDataDelegate>
@end
@implementation WKTRSessionDelegate
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler
{
    completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
}
@end

namespace WTR {

void TestController::notifyDone()
{
}

static PlatformWindow wtr_NSApplication_keyWindow(id self, SEL _cmd)
{
    return WTR::PlatformWebView::keyWindow();
}

static Class menuImplClass()
{
    static dispatch_once_t onceToken;
    static Class menuImplClass;
    dispatch_once(&onceToken, ^{
        auto menu = adoptNS([NSMenu new]);
        menuImplClass = [[menu _menuImpl] class];
    });
    return menuImplClass;
}

static __weak NSMenu *gCurrentPopUpMenu = nil;
static void setSwizzledPopUpMenu(NSMenu *menu)
{
    if (gCurrentPopUpMenu == menu)
        return;

    if ([menu.delegate respondsToSelector:@selector(menuWillOpen:)])
        [menu.delegate menuWillOpen:menu];

    gCurrentPopUpMenu = menu;

    dispatch_async(dispatch_get_main_queue(), ^{
        [[NSNotificationCenter defaultCenter] postNotificationName:NSMenuDidBeginTrackingNotification object:nil];
    });
}

static void swizzledPopUpContextMenu(Class, SEL, NSMenu *menu, NSEvent *event, NSView *)
{
    ASSERT(event);
    setSwizzledPopUpMenu(menu);
}

static void swizzledPopUpMenu(id, SEL, NSMenu *menu, NSPoint, CGFloat, NSView *, NSInteger, NSFont *, NSUInteger, NSDictionary *)
{
    setSwizzledPopUpMenu(menu);
}

static void swizzledCancelTracking(NSMenu *menu, SEL)
{
    if (menu != gCurrentPopUpMenu)
        return;

    gCurrentPopUpMenu = nil;

    if ([menu.delegate respondsToSelector:@selector(menuDidClose:)])
        [menu.delegate menuDidClose:menu];

    dispatch_async(dispatch_get_main_queue(), ^{
        [[NSNotificationCenter defaultCenter] postNotificationName:NSMenuDidEndTrackingNotification object:nil];
    });
}

void TestController::platformInitialize(const Options& options)
{
    poseAsClass("WebKitTestRunnerPasteboard", "NSPasteboard");
    poseAsClass("WebKitTestRunnerEvent", "NSEvent");
    
    cocoaPlatformInitialize(options);

    if (!m_defaultAppAccentColor)
        m_defaultAppAccentColor = NSApp._effectiveAccentColor;

    [NSSound _setAlertType:0];

    Method keyWindowMethod = class_getInstanceMethod(objc_getClass("NSApplication"), @selector(keyWindow));

    ASSERT(keyWindowMethod);
    if (!keyWindowMethod) {
        NSLog(@"Failed to swizzle the \"keyWindowMethod\" method on NSApplication");
        return;
    }
    
    method_setImplementation(keyWindowMethod, (IMP)wtr_NSApplication_keyWindow);

    static InstanceMethodSwizzler cancelTrackingSwizzler { NSMenu.class, @selector(cancelTracking), reinterpret_cast<IMP>(swizzledCancelTracking) };
    static ClassMethodSwizzler menuPopUpSwizzler { NSMenu.class, @selector(popUpContextMenu:withEvent:forView:), reinterpret_cast<IMP>(swizzledPopUpContextMenu) };
    static InstanceMethodSwizzler menuImplPopUpSwizzler {
        menuImplClass(),
        NSSelectorFromString(@"popUpMenu:atLocation:width:forView:withSelectedItem:withFont:withFlags:withOptions:"),
        reinterpret_cast<IMP>(swizzledPopUpMenu)
    };
}

void TestController::platformDestroy()
{
    [WebKitTestRunnerPasteboard releaseLocalPasteboards];
}

void TestController::initializeInjectedBundlePath()
{
    NSString *nsBundlePath = [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"WebKitTestRunnerInjectedBundle.bundle"];
    m_injectedBundlePath.adopt(WKStringCreateWithCFString((__bridge CFStringRef)nsBundlePath));
}

void TestController::initializeTestPluginDirectory()
{
    m_testPluginDirectory.adopt(WKStringCreateWithCFString((__bridge CFStringRef)[[NSBundle mainBundle] bundlePath]));
}

bool TestController::platformResetStateToConsistentValues(const TestOptions& options)
{
    cocoaResetStateToConsistentValues(options);

    if (RetainPtr webView = m_mainWebView ? m_mainWebView->platformView() : nil) {
        auto newObscuredInsetTop = options.obscuredInsetTop();
        auto newObscuredInsetLeft = options.obscuredInsetLeft();
        auto obscuredInset = [webView _obscuredContentInsets];
        if (obscuredInset.top != newObscuredInsetTop || obscuredInset.left != newObscuredInsetLeft) {
            obscuredInset.top = newObscuredInsetTop;
            obscuredInset.left = newObscuredInsetLeft;
            [webView _setObscuredContentInsets:obscuredInset immediate:YES];
        }
    }

    if (m_defaultAppAccentColor && ![NSApp._effectiveAccentColor isEqual:m_defaultAppAccentColor.get()])
        NSApp._accentColor = m_defaultAppAccentColor.get();

    while ([NSApp nextEventMatchingMask:NSEventMaskGesture | NSEventMaskScrollWheel untilDate:nil inMode:NSDefaultRunLoopMode dequeue:YES]) {
        // Clear out (and ignore) any pending gesture and scroll wheel events.
    }

    bool runLoopObserverFired = false;
    auto observer = makeUnique<WebCore::RunLoopObserver>(WebCore::RunLoopObserver::WellKnownOrder::ActivityStateChange, [&] {
        runLoopObserverFired = true;
    }, WebCore::RunLoopObserver::Type::OneShot);
    observer->schedule();
    while (!runLoopObserverFired)
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];

    return true;
}

static bool shouldEnableAsyncOverflowScrolling(const std::string& pathOrURL)
{
    return isWebPlatformTestURL({ { }, String::fromUTF8(pathOrURL.c_str()) });
}

TestFeatures TestController::platformSpecificFeatureDefaultsForTest(const TestCommand& command) const
{
    TestFeatures features;
    features.boolTestRunnerFeatures.insert({ "useThreadedScrolling", true });
    if (shouldEnableAsyncOverflowScrolling(command.pathOrURL))
        features.boolWebPreferenceFeatures.insert({ "AsyncOverflowScrollingEnabled", true });
    return features;
}

#if ENABLE(CONTENT_EXTENSIONS)
void TestController::configureContentExtensionForTest(const TestInvocation& test)
{
    if (!test.urlContains("contentextensions/"_s))
        return;

    auto testURL = adoptCF(WKURLCopyCFURL(kCFAllocatorDefault, test.url()));
    NSURL *filterURL = [(__bridge NSURL *)testURL.get() URLByAppendingPathExtension:@"json"];

    __block RetainPtr<NSString> contentExtensionString;
    __block bool doneFetchingContentExtension = false;
    auto delegate = adoptNS([WKTRSessionDelegate new]);
    NSURLSession *session = [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration ephemeralSessionConfiguration] delegate:delegate.get() delegateQueue:[NSOperationQueue mainQueue]];
    NSURLSessionDataTask *task = [session dataTaskWithRequest:[NSURLRequest requestWithURL:filterURL] completionHandler:^(NSData * data, NSURLResponse *response, NSError *error) {
        ASSERT(data);
        ASSERT(response);
        ASSERT(!error);
        contentExtensionString = adoptNS([[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);
        doneFetchingContentExtension = true;
    }];
    [task resume];
    platformRunUntil(doneFetchingContentExtension, noTimeout);

    __block bool doneCompiling = false;

    RetainPtr<NSURL> tempDir;
    if (const char* dumpRenderTreeTemp = libraryPathForTesting()) {
        String temporaryFolder = String::fromUTF8(dumpRenderTreeTemp);
        tempDir = adoptNS([[NSURL alloc] initFileURLWithPath:[temporaryFolder.createNSString() stringByAppendingPathComponent:@"ContentExtensions"] isDirectory:YES]);
    } else
        tempDir = adoptNS([[NSURL alloc] initFileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"ContentExtensions"] isDirectory:YES]);

    [[WKContentRuleListStore storeWithURL:tempDir.get()] compileContentRuleListForIdentifier:@"TestContentExtensions" encodedContentRuleList:contentExtensionString.get() completionHandler:^(WKContentRuleList *list, NSError *error)
    {
        if (!error)
            [mainWebView()->platformView().configuration.userContentController addContentRuleList:list];
        else
            NSLog(@"%@", [error helpAnchor]);
        doneCompiling = true;
    }];
    platformRunUntil(doneCompiling, noTimeout);
}
#endif

void TestController::platformConfigureViewForTest(const TestInvocation& test)
{
}

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
        @"Helvetica2",
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

static NSSet *systemHiddenFontFamilySet()
{
    static NeverDestroyed<RetainPtr<NSSet>> fontFamilySet = [NSSet setWithObjects:
        @".LucidaGrandeUI",
        nil];

    return fontFamilySet.get().get();
}

static WKRetainPtr<WKArrayRef> generateFontAllowList()
{
    auto result = adoptWK(WKMutableArrayCreate());
    for (NSString *fontFamily in allowedFontFamilySet()) {
        NSArray *fontsForFamily = [[NSFontManager sharedFontManager] availableMembersOfFontFamily:fontFamily];
        auto familyInFont = adoptWK(WKStringCreateWithUTF8CString([fontFamily UTF8String]));
        WKArrayAppendItem(result.get(), familyInFont.get());
        for (NSArray *fontInfo in fontsForFamily) {
            // Font name is the first entry in the array.
            auto fontName = adoptWK(WKStringCreateWithUTF8CString([[fontInfo objectAtIndex:0] UTF8String]));
            WKArrayAppendItem(result.get(), fontName.get());
        }
    }

    for (NSString *hiddenFontFamily in systemHiddenFontFamilySet())
        WKArrayAppendItem(result.get(), adoptWK(WKStringCreateWithUTF8CString([hiddenFontFamily UTF8String])).get());

    return result;
}

void TestController::platformInitializeContext()
{
    // Testing uses a private session, which is memory only. However creating one instantiates a shared NSURLCache,
    // and if we haven't created one yet, the default one will be created on disk.
    // Making the shared cache memory-only avoids touching the file system.
    auto sharedCache =
        adoptNS([[NSURLCache alloc] initWithMemoryCapacity:1024 * 1024
                                      diskCapacity:0
                                          diskPath:nil]);
    [NSURLCache setSharedURLCache:sharedCache.get()];

    WKContextSetFontAllowList(m_context.get(), generateFontAllowList().get());
}

void TestController::setHidden(bool hidden)
{
    NSWindow *window = [mainWebView()->platformView() window];
    if (!window)
        return;

    if (hidden)
        [window orderOut:nil];
    else
        [window makeKeyAndOrderFront:nil];
}

void TestController::runModal(PlatformWebView* view)
{
    NSWindow *window = [view->platformView() window];
    if (!window)
        return;
    [NSApp runModalForWindow:window];
}

void TestController::abortModal()
{
    [NSApp abortModal];
}

const char* TestController::platformLibraryPathForTesting()
{
    static NeverDestroyed<RetainPtr<NSString>> platformLibraryPath;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        platformLibraryPath.get() = [@"~/Library/Application Support/DumpRenderTree" stringByExpandingTildeInPath];
    });
    return [platformLibraryPath.get() UTF8String];
}

} // namespace WTR
