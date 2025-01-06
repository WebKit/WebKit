/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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
#import "RenderThemeCocoa.h"

#import "AttachmentLayout.h"
#import "CaretRectComputation.h"
#import "DrawGlyphsRecorder.h"
#import "FloatRoundedRect.h"
#import "FontCacheCoreText.h"
#import "GraphicsContextCG.h"
#import "HTMLInputElement.h"
#import "ImageBuffer.h"
#import "Page.h"
#import "RenderProgress.h"
#import "RenderText.h"
#import "UserAgentScripts.h"
#import "UserAgentStyleSheets.h"
#import <CoreGraphics/CoreGraphics.h>
#import <algorithm>
#import <pal/spi/cf/CoreTextSPI.h>
#import <pal/spi/cocoa/FeatureFlagsSPI.h>
#import <wtf/Language.h>

#if ENABLE(APPLE_PAY)
#import "ApplePayButtonPart.h"
#import "ApplePayLogoSystemImage.h"
#endif

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIFont.h>
#endif

#if ENABLE(VIDEO)
#import "LocalizedStrings.h"
#import <wtf/BlockObjCExceptions.h>
#endif

#if ENABLE(APPLE_PAY)
#import <pal/cocoa/PassKitSoftLink.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import <pal/ios/UIKitSoftLink.h>
#endif

@interface WebCoreRenderThemeBundle : NSObject
@end

@implementation WebCoreRenderThemeBundle
@end

namespace WebCore {

constexpr int kThumbnailBorderStrokeWidth = 1;
constexpr int kThumbnailBorderCornerRadius = 1;
constexpr int kVisibleBackgroundImageWidth = 1;
constexpr int kMultipleThumbnailShrinkSize = 2;

static inline bool canShowCapsLockIndicator()
{
#if HAVE(ACCELERATED_TEXT_INPUT)
    if (redesignedTextCursorEnabled())
        return false;
#endif
    return true;
}

RenderThemeCocoa& RenderThemeCocoa::singleton()
{
    return static_cast<RenderThemeCocoa&>(RenderTheme::singleton());
}

void RenderThemeCocoa::purgeCaches()
{
#if ENABLE(VIDEO) && ENABLE(MODERN_MEDIA_CONTROLS)
    m_mediaControlsLocalizedStringsScript.clearImplIfNotShared();
    m_mediaControlsScript.clearImplIfNotShared();
    m_mediaControlsStyleSheet.clearImplIfNotShared();
#endif // ENABLE(VIDEO) && ENABLE(MODERN_MEDIA_CONTROLS)

    RenderTheme::purgeCaches();
}

bool RenderThemeCocoa::shouldHaveCapsLockIndicator(const HTMLInputElement& element) const
{
    return canShowCapsLockIndicator() && element.isPasswordField();
}

Color RenderThemeCocoa::pictureFrameColor(const RenderObject& buttonRenderer)
{
    return systemColor(CSSValueAppleSystemControlBackground, buttonRenderer.styleColorOptions());
}

void RenderThemeCocoa::paintFileUploadIconDecorations(const RenderObject&, const RenderObject& buttonRenderer, const PaintInfo& paintInfo, const IntRect& rect, Icon* icon, FileUploadDecorations fileUploadDecorations)
{
    GraphicsContextStateSaver stateSaver(paintInfo.context());

    IntSize cornerSize(kThumbnailBorderCornerRadius, kThumbnailBorderCornerRadius);

    auto pictureFrameColor = this->pictureFrameColor(buttonRenderer);

    auto thumbnailPictureFrameRect = rect;
    auto thumbnailRect = rect;
    thumbnailRect.contract(2 * kThumbnailBorderStrokeWidth, 2 * kThumbnailBorderStrokeWidth);
    thumbnailRect.move(kThumbnailBorderStrokeWidth, kThumbnailBorderStrokeWidth);

    if (fileUploadDecorations == FileUploadDecorations::MultipleFiles) {
        // Smaller thumbnails for multiple selection appearance.
        thumbnailPictureFrameRect.contract(kMultipleThumbnailShrinkSize, kMultipleThumbnailShrinkSize);
        thumbnailRect.contract(kMultipleThumbnailShrinkSize, kMultipleThumbnailShrinkSize);

        // Background picture frame and simple background icon with a gradient matching the button.
        auto backgroundImageColor = buttonRenderer.style().visitedDependentColor(CSSPropertyBackgroundColor);
        paintInfo.context().fillRoundedRect(FloatRoundedRect(thumbnailPictureFrameRect, cornerSize, cornerSize, cornerSize, cornerSize), pictureFrameColor);
        paintInfo.context().fillRect(thumbnailRect, backgroundImageColor);

        // Move the rects for the Foreground picture frame and icon.
        auto inset = kVisibleBackgroundImageWidth + kThumbnailBorderStrokeWidth;
        thumbnailPictureFrameRect.move(inset, inset);
        thumbnailRect.move(inset, inset);
    }

    // Foreground picture frame and icon.
    paintInfo.context().fillRoundedRect(FloatRoundedRect(thumbnailPictureFrameRect, cornerSize, cornerSize, cornerSize, cornerSize), pictureFrameColor);
    icon->paint(paintInfo.context(), thumbnailRect);
}

Seconds RenderThemeCocoa::animationRepeatIntervalForProgressBar(const RenderProgress& renderer) const
{
    return renderer.page().preferredRenderingUpdateInterval();
}

#if ENABLE(APPLE_PAY)

static const auto applePayButtonMinimumWidth = 140;
static const auto applePayButtonPlainMinimumWidth = 100;
static const auto applePayButtonMinimumHeight = 30;

void RenderThemeCocoa::adjustApplePayButtonStyle(RenderStyle& style, const Element*) const
{
    if (style.applePayButtonType() == ApplePayButtonType::Plain)
        style.setMinWidth(Length(applePayButtonPlainMinimumWidth, LengthType::Fixed));
    else
        style.setMinWidth(Length(applePayButtonMinimumWidth, LengthType::Fixed));
    style.setMinHeight(Length(applePayButtonMinimumHeight, LengthType::Fixed));

    if (!style.hasExplicitlySetBorderRadius()) {
        auto cornerRadius = PKApplePayButtonDefaultCornerRadius;
        style.setBorderRadius({ { cornerRadius, LengthType::Fixed }, { cornerRadius, LengthType::Fixed } });
    }
}

#endif // ENABLE(APPLE_PAY)

#if ENABLE(VIDEO) && ENABLE(MODERN_MEDIA_CONTROLS)

String RenderThemeCocoa::mediaControlsStyleSheet()
{
    if (m_mediaControlsStyleSheet.isEmpty())
        m_mediaControlsStyleSheet = StringImpl::createWithoutCopying(ModernMediaControlsUserAgentStyleSheet);
    return m_mediaControlsStyleSheet;
}

Vector<String, 2> RenderThemeCocoa::mediaControlsScripts()
{
    // FIXME: Localized strings are not worth having a script. We should make it JSON data etc. instead.
    if (m_mediaControlsLocalizedStringsScript.isEmpty()) {
        NSBundle *bundle = [NSBundle bundleForClass:[WebCoreRenderThemeBundle class]];
        m_mediaControlsLocalizedStringsScript = [NSString stringWithContentsOfFile:[bundle pathForResource:@"modern-media-controls-localized-strings" ofType:@"js"] encoding:NSUTF8StringEncoding error:nil];
    }

    if (m_mediaControlsScript.isEmpty())
        m_mediaControlsScript = StringImpl::createWithoutCopying(ModernMediaControlsJavaScript);

    return {
        m_mediaControlsLocalizedStringsScript,
        m_mediaControlsScript,
    };
}

String RenderThemeCocoa::mediaControlsBase64StringForIconNameAndType(const String& iconName, const String& iconType)
{
    NSString *directory = @"modern-media-controls/images";
    NSBundle *bundle = [NSBundle bundleForClass:[WebCoreRenderThemeBundle class]];
    return [[NSData dataWithContentsOfFile:[bundle pathForResource:iconName ofType:iconType inDirectory:directory]] base64EncodedStringWithOptions:0];
}

String RenderThemeCocoa::mediaControlsFormattedStringForDuration(const double durationInSeconds)
{
    if (!std::isfinite(durationInSeconds))
        return WEB_UI_STRING("indefinite time", "accessibility help text for an indefinite media controller time value");

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (!m_durationFormatter) {
        m_durationFormatter = adoptNS([NSDateComponentsFormatter new]);
        m_durationFormatter.get().unitsStyle = NSDateComponentsFormatterUnitsStyleFull;
        m_durationFormatter.get().allowedUnits = NSCalendarUnitHour | NSCalendarUnitMinute | NSCalendarUnitSecond;
        m_durationFormatter.get().formattingContext = NSFormattingContextStandalone;
        m_durationFormatter.get().maximumUnitCount = 2;
    }
    return [m_durationFormatter stringFromTimeInterval:durationInSeconds];
    END_BLOCK_OBJC_EXCEPTIONS
}

#endif // ENABLE(VIDEO) && ENABLE(MODERN_MEDIA_CONTROLS)

static inline FontSelectionValue cssWeightOfSystemFont(CTFontRef font)
{
    auto resultRef = adoptCF(static_cast<CFNumberRef>(CTFontCopyAttribute(font, kCTFontCSSWeightAttribute)));
    float result = 0;
    if (resultRef && CFNumberGetValue(resultRef.get(), kCFNumberFloatType, &result))
        return FontSelectionValue(result);

    auto traits = adoptCF(CTFontCopyTraits(font));
    resultRef = static_cast<CFNumberRef>(CFDictionaryGetValue(traits.get(), kCTFontWeightTrait));
    CFNumberGetValue(resultRef.get(), kCFNumberFloatType, &result);
    // These numbers were experimentally gathered from weights of the system font.
    static constexpr std::array weightThresholds { -0.6f, -0.365f, -0.115f, 0.130f, 0.235f, 0.350f, 0.5f, 0.7f };
    for (unsigned i = 0; i < weightThresholds.size(); ++i) {
        if (result < weightThresholds[i])
            return FontSelectionValue((static_cast<int>(i) + 1) * 100);
    }
    return FontSelectionValue(900);
}

#if ENABLE(ATTACHMENT_ELEMENT)

int RenderThemeCocoa::attachmentBaseline(const RenderAttachment& attachment) const
{
    AttachmentLayout layout(attachment, AttachmentLayoutStyle::NonSelected);
    return layout.baseline;
}

void RenderThemeCocoa::paintAttachmentText(GraphicsContext& context, AttachmentLayout* layout)
{
    DrawGlyphsRecorder recorder(context, 1, DrawGlyphsRecorder::DeriveFontFromContext::Yes);

    for (const auto& line : layout->lines)
        recorder.drawNativeText(line.font.get(), CTFontGetSize(line.font.get()), line.line.get(), line.rect);
}

#endif

Color RenderThemeCocoa::platformSpellingMarkerColor(OptionSet<StyleColorOptions> options) const
{
    auto useDarkMode = options.contains(StyleColorOptions::UseDarkAppearance);
    return useDarkMode ? SRGBA<uint8_t> { 255, 140, 140, 217 } : SRGBA<uint8_t> { 255, 59, 48, 191 };
}

Color RenderThemeCocoa::platformDictationAlternativesMarkerColor(OptionSet<StyleColorOptions> options) const
{
    auto useDarkMode = options.contains(StyleColorOptions::UseDarkAppearance);
    return useDarkMode ? SRGBA<uint8_t> { 40, 145, 255, 217 } : SRGBA<uint8_t> { 0, 122, 255, 191 };
}

Color RenderThemeCocoa::platformGrammarMarkerColor(OptionSet<StyleColorOptions> options) const
{
    auto useDarkMode = options.contains(StyleColorOptions::UseDarkAppearance);
#if ENABLE(POST_EDITING_GRAMMAR_CHECKING)
    static bool useBlueForGrammar = false;
    static std::once_flag flag;
    std::call_once(flag, [] {
        useBlueForGrammar = os_feature_enabled(TextComposer, PostEditing) && os_feature_enabled(TextComposer, PostEditingUseBlueDots);
    });

    if (useBlueForGrammar)
        return useDarkMode ? SRGBA<uint8_t> { 40, 145, 255, 217 } : SRGBA<uint8_t> { 0, 122, 255, 191 };
#endif
    return useDarkMode ? SRGBA<uint8_t> { 50, 215, 75, 217 } : SRGBA<uint8_t> { 25, 175, 50, 191 };
}

}
