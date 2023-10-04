/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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

#import "AGXCompilerService.h"
#import "DOMURL.h"
#import "DeprecatedGlobalSettings.h"
#import "DictionaryLookup.h"
#import "Document.h"
#import "EventHandler.h"
#import "HTMLMediaElement.h"
#import "HitTestResult.h"
#import "MediaPlayerPrivate.h"
#import "Range.h"
#import "SharedBuffer.h"
#import "SimpleRange.h"
#import "UTIUtilities.h"
#import <AVFoundation/AVPlayer.h>

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
    return [_string attributedSubstringFromRange:range];
}

@end

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

namespace WebCore {

String Internals::userVisibleString(const DOMURL& url)
{
    return WTF::userVisibleString(url.href());
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
    auto* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

    document->updateLayout(LayoutOptions::IgnorePendingStylesheets);

    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::DisallowUserAgentShadowContent, HitTestRequest::Type::AllowChildFrameContent };
    
    auto* localFrame = dynamicDowncast<LocalFrame>(document->frame()->mainFrame());
    if (!localFrame)
        return nullptr; 

    auto result = localFrame->eventHandler().hitTestResultAtPoint(IntPoint(x, y), hitType);
    auto range = DictionaryLookup::rangeAtHitTestResult(result);
    if (!range)
        return nullptr;

    return RefPtr<Range> { createLiveRange(std::get<SimpleRange>(*range)) };
}

void Internals::setUsesOverlayScrollbars(bool enabled)
{
    WebCore::DeprecatedGlobalSettings::setUsesOverlayScrollbars(enabled);

    ScrollerStyle::setUseOverlayScrollbars(enabled);

    ScrollbarTheme& theme = ScrollbarTheme::theme();
    if (theme.isMockTheme())
        return;

    static_cast<ScrollbarThemeMac&>(theme).preferencesChanged();

    NSScrollerStyle style = enabled ? NSScrollerStyleOverlay : NSScrollerStyleLegacy;
    [NSScrollerImpPair _updateAllScrollerImpPairsForNewRecommendedScrollerStyle:style];
}

#endif

#if ENABLE(VIDEO)
double Internals::privatePlayerVolume(const HTMLMediaElement& element)
{
    auto corePlayer = element.player();
    if (!corePlayer)
        return 0;
    auto player = corePlayer->objCAVFoundationAVPlayer();
    if (!player)
        return 0;
    return [player volume];
}

bool Internals::privatePlayerMuted(const HTMLMediaElement& element)
{
    auto corePlayer = element.player();
    if (!corePlayer)
        return false;
    auto player = corePlayer->objCAVFoundationAVPlayer();
    if (!player)
        return false;
    return [player isMuted];
}
#endif

String Internals::encodedPreferenceValue(const String& domain, const String& key)
{
    auto userDefaults = adoptNS([[NSUserDefaults alloc] initWithSuiteName:domain]);
    id value = [userDefaults objectForKey:key];
    auto data = retainPtr([NSKeyedArchiver archivedDataWithRootObject:value requiringSecureCoding:YES error:nullptr]);
    return [data base64EncodedStringWithOptions:0];
}

String Internals::getUTIFromTag(const String& tagClass, const String& tag, const String& conformingToUTI)
{
    return UTIFromTag(tagClass, tag, conformingToUTI);
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

        return { [[PAL::getDDScannerResultClass() resultsFromCoreResults:results.get()] firstObject] };
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

    return adoptNS((id)[[FakeImageAnalysisResult alloc] initWithString:fullText.toString()]);
}

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

} // namespace WebCore
