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
#import "ColorBlending.h"
#import "DateComponents.h"
#import "DrawGlyphsRecorder.h"
#import "FloatRoundedRect.h"
#import "FontCacheCoreText.h"
#import "GraphicsContextCG.h"
#import "HTMLButtonElement.h"
#import "HTMLDataListElement.h"
#import "HTMLInputElement.h"
#import "HTMLMeterElement.h"
#import "HTMLOptionElement.h"
#import "HTMLSelectElement.h"
#import "ImageBuffer.h"
#import "LocalizedDateCache.h"
#import "NodeRenderStyle.h"
#import "Page.h"
#import "RenderButton.h"
#import "RenderMenulist.h"
#import "RenderMeter.h"
#import "RenderProgress.h"
#import "RenderSlider.h"
#import "RenderText.h"
#import "Theme.h"
#import "TypedElementDescendantIteratorInlines.h"
#import "UserAgentParts.h"
#import "UserAgentScripts.h"
#import "UserAgentStyleSheets.h"
#import <CoreGraphics/CoreGraphics.h>
#import <algorithm>
#import <pal/spi/cf/CoreTextSPI.h>
#import <pal/spi/cocoa/FeatureFlagsSPI.h>
#import <pal/system/ios/UserInterfaceIdiom.h>
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

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/RenderThemeCocoaAdditionsBefore.mm>
#else

namespace WebCore {

constexpr auto logicalSwitchHeight = 31.f;
constexpr auto logicalSwitchWidth = 51.f;

static bool renderThemePaintSwitchThumb(OptionSet<ControlStyle::State>, const RenderObject&, const PaintInfo&, const FloatRect&, const Color&)
{
    return true;
}

static bool renderThemePaintSwitchTrack(OptionSet<ControlStyle::State>, const RenderObject&, const PaintInfo&, const FloatRect&)
{
    return true;
}

static Vector<String> additionalMediaControlsStyleSheets(const HTMLMediaElement&)
{
    return { };
}

} // namespace WebCore

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

void RenderThemeCocoa::inflateRectForControlRenderer(const RenderObject& renderer, FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (inflateRectForControlRendererForVectorBasedControls(renderer, rect))
        return;
#endif

    RenderTheme::inflateRectForControlRenderer(renderer, rect);
}

void RenderThemeCocoa::purgeCaches()
{
#if ENABLE(VIDEO)
    m_mediaControlsLocalizedStringsScript.clearImplIfNotShared();
    m_mediaControlsScript.clearImplIfNotShared();
    m_mediaControlsStyleSheet.clearImplIfNotShared();
#endif // ENABLE(VIDEO)

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

void RenderThemeCocoa::paintFileUploadIconDecorations(const RenderObject&, const RenderObject& buttonRenderer, const PaintInfo& paintInfo, const FloatRect& rect, Icon* icon, FileUploadDecorations fileUploadDecorations)
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

static constexpr auto applePayButtonMinimumWidth = 140.0;
static constexpr auto applePayButtonPlainMinimumWidth = 100.0;
static constexpr auto applePayButtonMinimumHeight = 30.0;

void RenderThemeCocoa::adjustApplePayButtonStyle(RenderStyle& style, const Element*) const
{
    if (style.applePayButtonType() == ApplePayButtonType::Plain)
        style.setMinWidth(Style::MinimumSize::Fixed { applePayButtonPlainMinimumWidth });
    else
        style.setMinWidth(Style::MinimumSize::Fixed { applePayButtonMinimumWidth });
    style.setMinHeight(Style::MinimumSize::Fixed { applePayButtonMinimumHeight });

    if (!style.hasExplicitlySetBorderRadius()) {
        auto cornerRadius = PKApplePayButtonDefaultCornerRadius;
        style.setBorderRadius({ { cornerRadius, LengthType::Fixed }, { cornerRadius, LengthType::Fixed } });
    }
}

#endif // ENABLE(APPLE_PAY)

#if ENABLE(VIDEO)

Vector<String> RenderThemeCocoa::mediaControlsStyleSheets(const HTMLMediaElement& mediaElement)
{
    if (m_mediaControlsStyleSheet.isEmpty())
        m_mediaControlsStyleSheet = StringImpl::createWithoutCopying(ModernMediaControlsUserAgentStyleSheet);

    auto mediaControlsStyleSheets = Vector<String>::from(m_mediaControlsStyleSheet);
    mediaControlsStyleSheets.appendVector(additionalMediaControlsStyleSheets(mediaElement));

    return mediaControlsStyleSheets;
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
    return [[NSData dataWithContentsOfFile:[bundle pathForResource:iconName.createNSString().get() ofType:iconType.createNSString().get() inDirectory:directory]] base64EncodedStringWithOptions:0];
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

#endif // ENABLE(VIDEO)

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
    DrawGlyphsRecorder recorder(context, 1, DrawGlyphsRecorder::DeriveFontFromContext::Yes, DrawGlyphsRecorder::DrawDecomposedGlyphs::No);

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

Color RenderThemeCocoa::controlTintColor(const RenderStyle& style, OptionSet<StyleColorOptions> options) const
{
    if (!style.hasAutoAccentColor())
        return style.usedAccentColor(options);

#if PLATFORM(MAC)
    auto cssColorValue = CSSValueAppleSystemControlAccent;
#else
    auto cssColorValue = CSSValueAppleSystemBlue;
#endif
    return systemColor(cssColorValue, options | StyleColorOptions::UseSystemAppearance);
}

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/RenderThemeCocoaAdditions.mm>
#endif

void RenderThemeCocoa::adjustCheckboxStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustCheckboxStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustCheckboxStyle(style, element);
}

bool RenderThemeCocoa::paintCheckbox(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintCheckboxForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintCheckbox(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustRadioStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustRadioStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustRadioStyle(style, element);
}

bool RenderThemeCocoa::paintRadio(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintRadioForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintRadio(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustButtonStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustButtonStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustButtonStyle(style, element);
}

bool RenderThemeCocoa::paintButton(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintButtonForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintButton(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustColorWellStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustColorWellStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustColorWellStyle(style, element);
}

void RenderThemeCocoa::adjustColorWellSwatchStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustColorWellSwatchStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustColorWellSwatchStyle(style, element);
}

void RenderThemeCocoa::adjustColorWellSwatchOverlayStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustColorWellSwatchOverlayStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustColorWellSwatchOverlayStyle(style, element);
}

void RenderThemeCocoa::adjustColorWellSwatchWrapperStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustColorWellSwatchWrapperStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustColorWellSwatchWrapperStyle(style, element);
}

bool RenderThemeCocoa::paintColorWellSwatch(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintColorWellSwatchForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintColorWellSwatch(box, paintInfo, rect);
}

bool RenderThemeCocoa::paintColorWell(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintColorWellForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintColorWell(box, paintInfo, rect);
}

void RenderThemeCocoa::paintColorWellDecorations(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintColorWellDecorationsForVectorBasedControls(box, paintInfo, rect))
        return;
#endif

    RenderTheme::paintColorWellDecorations(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustInnerSpinButtonStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustInnerSpinButtonStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustInnerSpinButtonStyle(style, element);
}

bool RenderThemeCocoa::paintInnerSpinButton(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintInnerSpinButtonStyleForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintInnerSpinButton(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustTextFieldStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustTextFieldStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustTextFieldStyle(style, element);
}

bool RenderThemeCocoa::paintTextField(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintTextFieldForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintTextField(box, paintInfo, rect);
}

void RenderThemeCocoa::paintTextFieldDecorations(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintTextFieldDecorationsForVectorBasedControls(box, paintInfo, rect))
        return;
#endif

    RenderTheme::paintTextFieldDecorations(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustTextAreaStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustTextAreaStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustTextAreaStyle(style, element);
}

bool RenderThemeCocoa::paintTextArea(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintTextAreaForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintTextArea(box, paintInfo, rect);
}

void RenderThemeCocoa::paintTextAreaDecorations(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintTextAreaDecorationsForVectorBasedControls(box, paintInfo, rect))
        return;
#endif

    RenderTheme::paintTextAreaDecorations(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustMenuListStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustMenuListStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustMenuListStyle(style, element);
}

bool RenderThemeCocoa::paintMenuList(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintMenuListForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintMenuList(box, paintInfo, rect);
}

void RenderThemeCocoa::paintMenuListDecorations(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintMenuListDecorationsForVectorBasedControls(box, paintInfo, rect))
        return;
#endif

    RenderTheme::paintMenuListDecorations(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustMenuListButtonStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustMenuListButtonStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustMenuListButtonStyle(style, element);
}

void RenderThemeCocoa::paintMenuListButtonDecorations(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintMenuListButtonDecorationsForVectorBasedControls(box, paintInfo, rect))
        return;
#endif

    RenderTheme::paintMenuListButtonDecorations(box, paintInfo, rect);
}

bool RenderThemeCocoa::paintMenuListButton(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintMenuListButtonForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintMenuListButton(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustMeterStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustMeterStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustMeterStyle(style, element);
}

bool RenderThemeCocoa::paintMeter(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintMeterForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintMeter(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustListButtonStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustListButtonStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustListButtonStyle(style, element);
}

bool RenderThemeCocoa::paintListButton(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintListButtonForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintListButton(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustProgressBarStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustProgressBarStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustProgressBarStyle(style, element);
}

bool RenderThemeCocoa::paintProgressBar(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintProgressBarForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintProgressBar(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustSliderTrackStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustSliderTrackStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustSliderTrackStyle(style, element);
}

bool RenderThemeCocoa::paintSliderTrack(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintSliderTrackForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintSliderTrack(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustSliderThumbSize(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustSliderThumbSizeForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustSliderThumbSize(style, element);
}

void RenderThemeCocoa::adjustSliderThumbStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustSliderThumbStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustSliderThumbStyle(style, element);
}

bool RenderThemeCocoa::paintSliderThumb(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintSliderThumbForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintSliderThumb(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustSearchFieldStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustSearchFieldStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustSearchFieldStyle(style, element);
}

bool RenderThemeCocoa::paintSearchField(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintSearchFieldForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintSearchField(box, paintInfo, rect);
}

void RenderThemeCocoa::paintSearchFieldDecorations(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintSearchFieldDecorationsForVectorBasedControls(box, paintInfo, rect))
        return;
#endif

    RenderTheme::paintSearchFieldDecorations(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustSearchFieldCancelButtonStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustSearchFieldCancelButtonStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustSearchFieldCancelButtonStyle(style, element);
}

bool RenderThemeCocoa::paintSearchFieldCancelButton(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintSearchFieldCancelButtonForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintSearchFieldCancelButton(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustSearchFieldDecorationPartStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustSearchFieldDecorationPartStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustSearchFieldDecorationPartStyle(style, element);
}

bool RenderThemeCocoa::paintSearchFieldDecorationPart(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintSearchFieldDecorationPartForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintSearchFieldDecorationPart(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustSearchFieldResultsDecorationPartStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustSearchFieldResultsDecorationPartStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustSearchFieldResultsDecorationPartStyle(style, element);
}

bool RenderThemeCocoa::paintSearchFieldResultsDecorationPart(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintSearchFieldResultsDecorationPartForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintSearchFieldResultsDecorationPart(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustSearchFieldResultsButtonStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustSearchFieldResultsButtonStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustSearchFieldResultsButtonStyle(style, element);
}

bool RenderThemeCocoa::paintSearchFieldResultsButton(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintSearchFieldResultsButtonForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintSearchFieldResultsButton(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustSwitchStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustSwitchStyleForVectorBasedControls(style, element))
        return;
#endif

#if PLATFORM(MAC)
    RenderTheme::adjustSwitchStyle(style, element);
#else
    UNUSED_PARAM(element);

    // FIXME: Deduplicate sizing with the generic code somehow.
    if (style.width().isAuto() || style.height().isAuto()) {
        style.setLogicalWidth(Style::PreferredSize::Fixed { logicalSwitchWidth * style.usedZoom() });
        style.setLogicalHeight(Style::PreferredSize::Fixed { logicalSwitchHeight * style.usedZoom() });
    }

    adjustSwitchStyleDisplay(style);

    if (style.hasAutoOutlineStyle())
        style.setOutlineStyle(BorderStyle::None);
#endif
}

bool RenderThemeCocoa::paintSwitchThumb(const RenderObject& renderer, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if PLATFORM(MAC)
    bool useDefaultImplementation = true;
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (renderer.settings().vectorBasedControlsOnMacEnabled())
        useDefaultImplementation = false;
#endif
    if (useDefaultImplementation)
        return RenderTheme::paintSwitchThumb(renderer, paintInfo, rect);
#endif

    return renderThemePaintSwitchThumb(extractControlStyleStatesForRenderer(renderer), renderer, paintInfo, rect, platformFocusRingColor(renderer.styleColorOptions()));
}

bool RenderThemeCocoa::paintSwitchTrack(const RenderObject& renderer, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if PLATFORM(MAC)
    bool useDefaultImplementation = true;
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (renderer.settings().vectorBasedControlsOnMacEnabled())
        useDefaultImplementation = false;
#endif
    if (useDefaultImplementation)
        return RenderTheme::paintSwitchTrack(renderer, paintInfo, rect);
#endif

    return renderThemePaintSwitchTrack(extractControlStyleStatesForRenderer(renderer), renderer, paintInfo, rect);
}

void RenderThemeCocoa::paintPlatformResizer(const RenderLayerModelObject& renderer, GraphicsContext& context, const LayoutRect& resizerCornerRect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintPlatformResizerForVectorBasedControls(renderer, context, resizerCornerRect))
        return;
#endif
    RenderTheme::paintPlatformResizer(renderer, context, resizerCornerRect);
}

void RenderThemeCocoa::paintPlatformResizerFrame(const RenderLayerModelObject& renderer, GraphicsContext& context, const LayoutRect& resizerCornerRect)
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (paintPlatformResizerFrameForVectorBasedControls(renderer, context, resizerCornerRect))
        return;
#endif
    RenderTheme::paintPlatformResizerFrame(renderer, context, resizerCornerRect);
}

bool RenderThemeCocoa::supportsFocusRing(const RenderObject& renderer, const RenderStyle& style) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)

#if PLATFORM(MAC)
    auto tryFocusRingForVectorBasedControls = renderer.settings().vectorBasedControlsOnMacEnabled();
#else
    auto tryFocusRingForVectorBasedControls = renderer.settings().macStyleControlsOnCatalyst();
#endif
    if (tryFocusRingForVectorBasedControls)
        return supportsFocusRingForVectorBasedControls(renderer, style);

#endif

    return RenderTheme::supportsFocusRing(renderer, style);
}

void RenderThemeCocoa::adjustTextControlInnerContainerStyle(RenderStyle& style, const RenderStyle& shadowHostStyle, const Element* shadowHost) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustTextControlInnerContainerStyleForVectorBasedControls(style, shadowHostStyle, shadowHost))
        return;
#endif

    RenderTheme::adjustTextControlInnerContainerStyle(style, shadowHostStyle, shadowHost);
}

void RenderThemeCocoa::adjustTextControlInnerPlaceholderStyle(RenderStyle& style, const RenderStyle& shadowHostStyle, const Element* shadowHost) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustTextControlInnerPlaceholderStyleForVectorBasedControls(style, shadowHostStyle, shadowHost))
        return;
#endif

    RenderTheme::adjustTextControlInnerPlaceholderStyle(style, shadowHostStyle, shadowHost);
}

void RenderThemeCocoa::adjustTextControlInnerTextStyle(RenderStyle& style, const RenderStyle& shadowHostStyle, const Element* shadowHost) const
{
#if ENABLE(VECTOR_BASED_CONTROLS_ON_MAC)
    if (adjustTextControlInnerTextStyleForVectorBasedControls(style, shadowHostStyle, shadowHost))
        return;
#endif

    RenderTheme::adjustTextControlInnerTextStyle(style, shadowHostStyle, shadowHost);
}

}
