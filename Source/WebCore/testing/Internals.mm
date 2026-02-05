/*
 * Copyright (C) 2015-2024 Apple Inc. All rights reserved.
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
#import "Internals.h"

#import "DOMURL.h"
#import "DeprecatedGlobalSettings.h"
#import "DictionaryLookup.h"
#import "DocumentView.h"
#import "EventHandler.h"
#import "FrameDestructionObserverInlines.h"
#import "HTMLMediaElement.h"
#import "HitTestResult.h"
#import "LocalFrameInlines.h"
#import "LocalFrameView.h"
#import "Logging.h"
#import "MediaPlayerPrivate.h"
#import "Range.h"
#import "SharedBuffer.h"
#import "SimpleRange.h"
#import "UTIUtilities.h"
#import <AVFoundation/AVPlayer.h>
#import <wtf/BlockPtr.h>
#import <wtf/darwin/DispatchExtras.h>

#if PLATFORM(MAC)
#import "NSScrollerImpDetails.h"
#import "ScrollbarThemeMac.h"
#import <pal/spi/mac/NSScrollerImpSPI.h>
#endif

#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/spi/darwin/SandboxSPI.h>
#import <wtf/unicode/CharacterNames.h>

#if PLATFORM(IOS_FAMILY)
#import <pal/ios/UIKitSoftLink.h>
#endif

#if ENABLE(DATA_DETECTION)
#import <pal/cocoa/DataDetectorsCoreSoftLink.h>
#endif

#import <pal/cocoa/VisionKitCoreSoftLink.h>

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

@interface FakeImageAnalysisResult : NSObject
- (instancetype)initWithString:(NSString *)fullText;
@end

static NSRange clampRange(NSRange rangeToClamp, NSRange extentRange)
{
    NSUInteger minRange1 = rangeToClamp.location;
    NSUInteger maxRange1 = NSMaxRange(rangeToClamp);

    NSUInteger minRange2 = extentRange.location;
    NSUInteger maxRange2 = NSMaxRange(extentRange);

    NSUInteger minCommon = clampTo(minRange1, minRange2, maxRange2);
    NSUInteger maxCommon = clampTo(maxRange1, minRange2, maxRange2);

    if (rangeToClamp.location == NSNotFound) {
        minCommon = maxRange2;
        maxCommon = maxRange2;
    } else if (minCommon > maxCommon)
        std::swap(minCommon, maxCommon);

    NSRange result = NSMakeRange(minCommon, maxCommon - minCommon);

    if (extentRange.location == NSNotFound)
        result = NSMakeRange(NSNotFound, 0);

    return result;
}

@implementation FakeImageAnalysisResult {
    RetainPtr<NSAttributedString> _string;
}

- (instancetype)initWithString:(NSString *)string
{
    if (!(self = [super init]))
        return nil;

    _string = adoptNS([[NSMutableAttributedString alloc] initWithString:string]);
    return self;
}

- (NSAttributedString *)_attributedStringForRange:(NSRange)range
{
    NSRange validRange = NSMakeRange(0, [_string length]);
    NSRange safeRange = clampRange(range, validRange);
    return [_string attributedSubstringFromRange:safeRange];
}

@end

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/WebCoreInternalsAdditions.mm>)
#import <WebKitAdditions/WebCoreInternalsAdditions.mm>
#endif

namespace WebCore {

String Internals::userVisibleString(const DOMURL& url)
{
    return WTF::userVisibleString(url.href().createNSURL().get());
}

bool Internals::userPrefersContrast() const
{
#if PLATFORM(IOS_FAMILY)
    return PAL::softLink_UIKit_UIAccessibilityDarkerSystemColorsEnabled();
#else
    return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldIncreaseContrast];
#endif
}

bool Internals::userPrefersReducedMotion() const
{
#if PLATFORM(IOS_FAMILY)
    return PAL::softLink_UIKit_UIAccessibilityIsReduceMotionEnabled();
#else
    return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
#endif
}

#if PLATFORM(MAC)

ExceptionOr<RefPtr<Range>> Internals::rangeForDictionaryLookupAtLocation(int x, int y)
{
    RefPtr document = contextDocument();
    if (!document || !document->frame())
        return Exception { ExceptionCode::InvalidAccessError };

    document->updateLayout(LayoutOptions::IgnorePendingStylesheets);

    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::DisallowUserAgentShadowContent, HitTestRequest::Type::AllowChildFrameContent };
    
    RefPtr localFrame = dynamicDowncast<LocalFrame>(document->frame()->mainFrame());
    if (!localFrame)
        return nullptr; 

    auto result = localFrame->eventHandler().hitTestResultAtPoint(IntPoint(x, y), hitType);
    auto range = DictionaryLookup::rangeAtHitTestResult(result);
    if (!range)
        return nullptr;

    return RefPtr<Range> { createLiveRange(*range) };
}

void Internals::setUsesOverlayScrollbars(bool enabled)
{
    WebCore::DeprecatedGlobalSettings::setUsesOverlayScrollbars(enabled);

    ScrollerStyle::setUseOverlayScrollbars(enabled);

    ScrollbarTheme& theme = ScrollbarTheme::theme();
    if (theme.isMockTheme())
        return;

    downcast<ScrollbarThemeMac>(theme).preferencesChanged();

    NSScrollerStyle style = enabled ? NSScrollerStyleOverlay : NSScrollerStyleLegacy;
    [NSScrollerImpPair _updateAllScrollerImpPairsForNewRecommendedScrollerStyle:style];

    RefPtr document = contextDocument();
    if (!document || !document->frame())
        return;

    RefPtr localFrame = dynamicDowncast<LocalFrame>(document->frame()->mainFrame());
    if (!localFrame)
        return;

    protect(localFrame->view())->scrollbarStyleDidChange();
}

#endif

#if ENABLE(VIDEO)
double Internals::privatePlayerVolume(const HTMLMediaElement& element)
{
    RefPtr corePlayer = element.player();
    if (!corePlayer)
        return 0;
    RetainPtr player = corePlayer->objCAVFoundationAVPlayer();
    if (!player)
        return 0;
    return [player volume];
}

bool Internals::privatePlayerMuted(const HTMLMediaElement& element)
{
    RefPtr corePlayer = element.player();
    if (!corePlayer)
        return false;
    RetainPtr player = corePlayer->objCAVFoundationAVPlayer();
    if (!player)
        return false;
    return [player isMuted];
}
#endif

String Internals::encodedPreferenceValue(const String& domain, const String& key)
{
    RetainPtr userDefaults = adoptNS([[NSUserDefaults alloc] initWithSuiteName:domain.createNSString().get()]);
    RetainPtr value = [userDefaults objectForKey:key.createNSString().get()];
    RetainPtr data = retainPtr([NSKeyedArchiver archivedDataWithRootObject:value.get() requiringSecureCoding:YES error:nullptr]);
    return [data base64EncodedStringWithOptions:0];
}

bool Internals::isRemoteUIAppForAccessibility()
{
#if PLATFORM(MAC)
    return [NSAccessibilityRemoteUIElement isRemoteUIApp];
#else
    return false;
#endif
}

bool Internals::hasSandboxIOKitOpenAccessToClass(const String& process, const String& ioKitClass)
{
    UNUSED_PARAM(process); // TODO: add support for getting PID of other WebKit processes.
    pid_t pid = getpid();

    return !sandbox_check(pid, "iokit-open", static_cast<enum sandbox_filter_type>(SANDBOX_FILTER_IOKIT_CONNECTION | SANDBOX_CHECK_NO_REPORT), ioKitClass.utf8().data());
}

#if ENABLE(DATA_DETECTION)

DDScannerResult *Internals::fakeDataDetectorResultForTesting()
{
    static NeverDestroyed result = []() -> RetainPtr<DDScannerResult> {
        auto scanner = adoptCF(PAL::softLink_DataDetectorsCore_DDScannerCreate(DDScannerTypeStandard, 0, nullptr));
        auto stringToScan = CFSTR("webkit.org");
        auto query = adoptCF(PAL::softLink_DataDetectorsCore_DDScanQueryCreateFromString(kCFAllocatorDefault, stringToScan, CFRangeMake(0, CFStringGetLength(stringToScan))));
        if (!PAL::softLink_DataDetectorsCore_DDScannerScanQuery(scanner.get(), query.get()))
            return nil;

        auto results = adoptCF(PAL::softLink_DataDetectorsCore_DDScannerCopyResultsWithOptions(scanner.get(), DDScannerCopyResultsOptionsNoOverlap));
        if (!CFArrayGetCount(results.get()))
            return nil;

        return { [[PAL::getDDScannerResultClassSingleton() resultsFromCoreResults:results.get()] firstObject] };
    }();
    return result->get();
}

#endif // ENABLE(DATA_DETECTION)

RefPtr<SharedBuffer> Internals::pngDataForTesting()
{
    NSBundle *webCoreBundle = [NSBundle bundleForClass:NSClassFromString(@"WebCoreBundleFinder")];
    return SharedBuffer::createWithContentsOfFile([webCoreBundle pathForResource:@"missingImage" ofType:@"png"]);
}

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

RetainPtr<VKCImageAnalysis> Internals::fakeImageAnalysisResultForTesting(const Vector<ImageOverlayLine>& lines)
{
    if (lines.isEmpty())
        return { };

    StringBuilder fullText;
    for (auto& line : lines) {
        for (auto& text : line.children) {
            if (text.hasLeadingWhitespace)
                fullText.append(space);
            fullText.append(text.text);
        }
        if (line.hasTrailingNewline)
            fullText.append(newlineCharacter);
    }

    return adoptNS((id)[[FakeImageAnalysisResult alloc] initWithString:fullText.createNSString().get()]);
}

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
bool Internals::emitWebCoreLogs(unsigned logCount, bool useMainThread) const
{
    auto blockPtr = makeBlockPtr([logCount] {
        for (unsigned i = 0; i < logCount; i++)
            RELEASE_LOG_FORWARDABLE(Testing, WEBCORE_TEST_LOG, i);
    });
    if (useMainThread)
        dispatch_async(mainDispatchQueueSingleton(), blockPtr.get());
    else
        dispatch_async(globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), blockPtr.get());
    return true;
}

bool Internals::emitLogs(const String& logString, unsigned logCount, bool useMainThread) const
{
    auto blockPtr = makeBlockPtr([logString, logCount] {
        for (unsigned i = 0; i < logCount; i++)
            RELEASE_LOG(Testing, "%s", logString.utf8().data());
    });
    if (useMainThread)
        dispatch_async(mainDispatchQueueSingleton(), blockPtr.get());
    else
        dispatch_async(globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), blockPtr.get());
    return true;
}
#endif // ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)

} // namespace WebCore
