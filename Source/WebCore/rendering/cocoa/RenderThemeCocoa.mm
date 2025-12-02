/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#import "BorderPainter.h"
#import "CSSToLengthConversionData.h"
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
#import "HTMLMediaElement.h"
#import "HTMLMeterElement.h"
#import "HTMLOptionElement.h"
#import "HTMLSelectElement.h"
#import "ImageBuffer.h"
#import "LocalizedDateCache.h"
#import "NodeRenderStyle.h"
#import "Page.h"
#import "RenderBoxInlines.h"
#import "RenderBoxModelObjectInlines.h"
#import "RenderButton.h"
#import "RenderMenulist.h"
#import "RenderMeter.h"
#import "RenderProgress.h"
#import "RenderSlider.h"
#import "RenderText.h"
#import "Settings.h"
#import "StylePrimitiveNumericTypes+Evaluation.h"
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

namespace WebCore {

#if ENABLE(FORM_CONTROL_REFRESH)

static bool formControlRefreshEnabled(const RenderObject& renderer)
{
    return renderer.settings().formControlRefreshEnabled();
}

static bool formControlRefreshEnabled(const Element* element)
{
    if (!element)
        return false;

    return element->document().settings().formControlRefreshEnabled();
}

static Color colorCompositedOverCanvasColor(const Color& color, OptionSet<StyleColorOptions> styleColorOptions)
{
    const auto backingColor = RenderTheme::singleton().systemColor(CSSValueCanvas, styleColorOptions);
    return blendSourceOver(backingColor, color);
}

static Color colorCompositedOverCanvasColor(CSSValueID cssValue, OptionSet<StyleColorOptions> styleColorOptions)
{
    const auto backingColor = RenderTheme::singleton().systemColor(CSSValueCanvas, styleColorOptions);
    const auto foregroundColor = RenderTheme::singleton().systemColor(cssValue, styleColorOptions);
    return blendSourceOver(backingColor, foregroundColor);
}

static void drawFocusRingForPathForVectorBasedControls(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect, Path path)
{
    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    // macOS controls have never honored outline offset.
#if PLATFORM(IOS_FAMILY)
    auto deviceScaleFactor = box.document().deviceScaleFactor();
    auto outlineOffset = floorToDevicePixel(Style::evaluate<float>(box.style().outlineOffset(), Style::ZoomNeeded { }), deviceScaleFactor);

    if (outlineOffset > 0) {
        const auto center = rect.center();
        const auto sizeWithOffset = FloatSize { outlineOffset, outlineOffset } * 2 + rect.size();
        context.translate(center);
        context.scale(sizeWithOffset / rect.size());
        context.translate(-center);
    }
#else
    UNUSED_PARAM(rect);
#endif

    auto focusRingColor = RenderTheme::singleton().focusRingColor(box.styleColorOptions() | StyleColorOptions::UseSystemAppearance);

    // We pass 0.f as the border thickness because the parameter is not used by
    // the context. It will determine an appropriate value for us.
    context.drawFocusRing(path, 0.f, focusRingColor);
}

#if PLATFORM(MAC)

static Color highContrastOutlineColor(OptionSet<StyleColorOptions> styleColorOptions)
{
    auto labelColor = RenderTheme::singleton().systemColor(CSSValueAppleSystemLabel, styleColorOptions);
    return labelColor.colorWithAlphaMultipliedBy(0.65f);
}

static void drawHighContrastOutline(GraphicsContext& context, Path path, OptionSet<StyleColorOptions> styleColorOptions)
{
    static constexpr auto highContrastOutlineThickness = 1.f;

    GraphicsContextStateSaver stateSaver(context);

    const auto outlineColor = highContrastOutlineColor(styleColorOptions);

    context.clipPath(path);
    context.setStrokeThickness(highContrastOutlineThickness * 2);
    context.setStrokeColor(outlineColor);
    context.strokePath(path);
}
#endif

static constexpr auto controlTintLuminanceThreshold = 0.5f;

static Color foregroundColorForBackgroundColor(const Color& backgroundColor)
{
    return backgroundColor.luminance() <= controlTintLuminanceThreshold ? WebCore::Color::white : WebCore::Color::black;
}

static Color colorWithTargetLuminance(Color color, float targetLuminance)
{
    // Adjust the luminance of the color while preserving its hue by converting its
    // type to XYZA so that its Y value, which represents luminance, can be easily changed.

    const auto [x, y, z, alpha] = color.toColorTypeLossy<XYZA<float, WhitePoint::D65>>().resolved();

    targetLuminance = std::clamp(0.f, targetLuminance, 1.f);
    if (y > 0.0f) {
        const auto scale = targetLuminance / y;
        return Color(XYZA<float, WhitePoint::D65> { x * scale, targetLuminance, z * scale, alpha });
    }

    return Color(XYZA<float, WhitePoint::D65> { targetLuminance, targetLuminance, targetLuminance, alpha });
}

#endif

}

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/RenderThemeCocoaAdditionsBefore.mm>
#else

namespace WebCore {

static constexpr auto logicalSwitchWidth = 51.f;
#if PLATFORM(IOS_FAMILY)
static constexpr auto logicalSwitchHeight = 31.f;
#else
static constexpr auto logicalSwitchHeight = 18.f;
#endif

static constexpr FloatSize idealRefreshedSwitchSize = { 64, 28 };
static constexpr auto logicalRefreshedSwitchWidth = logicalSwitchHeight * (idealRefreshedSwitchSize.width() / idealRefreshedSwitchSize.height());

static bool renderThemePaintSwitchThumb(OptionSet<ControlStyle::State>, const RenderElement&, const PaintInfo&, const FloatRect&, const Color&)
{
    return true;
}

static bool renderThemePaintSwitchTrack(OptionSet<ControlStyle::State>, const RenderElement&, const PaintInfo&, const FloatRect&)
{
    return true;
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

Color RenderThemeCocoa::pictureFrameColor(const RenderElement& buttonRenderer)
{
    return systemColor(CSSValueAppleSystemControlBackground, buttonRenderer.styleColorOptions());
}

void RenderThemeCocoa::paintFileUploadIconDecorations(const RenderElement&, const RenderElement& buttonRenderer, const PaintInfo& paintInfo, const FloatRect& rect, Icon* icon, FileUploadDecorations fileUploadDecorations)
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
        auto backgroundImageColor = buttonRenderer.checkedStyle()->visitedDependentColor(CSSPropertyBackgroundColor);
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
    return renderer.protectedPage()->preferredRenderingUpdateInterval();
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
        auto radius = Style::LengthPercentage<CSS::Nonnegative>::Dimension { static_cast<float>(PKApplePayButtonDefaultCornerRadius) };
        style.setBorderRadius({ radius, radius });
    }
}

#endif // ENABLE(APPLE_PAY)

#if ENABLE(VIDEO)

#if HAVE(MATERIAL_HOSTING)

static const String& glassMaterialMediaControlsStyleSheet()
{
    static NeverDestroyed<String> glassMaterialMediaControlsStyleSheet {
        "@supports (-apple-visual-effect: -apple-system-glass-material) {"
        "    .media-controls.mac * {"
        "        --primary-glyph-color: white;"
        "        --secondary-glyph-color: white;"
        "    }"
        "    .media-controls.inline.mac:not(.audio, .narrowviewer) {"
        "        background-color: rgba(0, 0, 0, 0.4);"
        "    }"
        "    .media-controls.inline.mac:not(.audio):is(:empty, .faded) {"
        "        background-color: transparent;"
        "    }"
        "    .media-controls.mac:not(.audio) .background-tint > .blur {"
        "        display: none;"
        "    }"
        "    .media-controls.mac.inline.audio .background-tint > .blur {"
        "        background-color: rgba(0, 0, 0, 0.4);"
        "        -webkit-backdrop-filter: unset;"
        "    }"
        "    .media-controls.mac .background-tint > .tint {"
        "        background-color: unset;"
        "        color-scheme: dark;"
        "        mix-blend-mode: unset;"
        "    }"
        "    .media-controls.mac.inline .background-tint > .tint {"
        "        -apple-visual-effect: -apple-system-glass-material-media-controls-subdued;"
        "    }"
        "    .media-controls.mac.fullscreen .background-tint > .tint {"
        "        -apple-visual-effect: -apple-system-glass-material-media-controls;"
        "    }"
        "}"_s
    };

    return glassMaterialMediaControlsStyleSheet;
}

static const String& macOSInlineMediaControlsStyleSheet()
{
    static NeverDestroyed<String> macOSInlineMediaControlsStyleSheet {
        ".media-controls-container.mac.inline:not(.audio) {"
        "    display: flex;"
        "    flex-direction: column;"
        "    position: relative;"
        "}"
        "/* Volume slider */"
        ".media-controls.mac.inline:not(.audio) .volume-slider-container {"
        "    position: absolute;"
        "    top: var(--inline-controls-inside-margin);"
        "    right: calc(var(--inline-controls-inside-margin) * 1);"
        "    width: 180px;"
        "    height: 46px;"
        "    display: flex;"
        "    align-items: center;"
        "    justify-content: center;"
        "    border-radius: var(--inline-controls-border-radius);"
        "    transform: translateY(calc(var(--inline-controls-inside-margin) + 2));"
        "}"
        ".media-controls.mac.inline:not(.audio) .volume-slider-container > .background-tint {"
        "    top: 0;"
        "    left: var(--inline-controls-inside-margin);"
        "    right: 0;"
        "    bottom: 0;"
        "    width: auto;"
        "}"
        ".media-controls.mac.inline:focus-within:not(.audio) .volume-slider-container,"
        ".media-controls.mac.inline:not(.audio).show-controls .volume-slider-container {"
        "    opacity: 1;"
        "    pointer-events: auto;"
        "    transform: none;"
        "}"
        ".media-controls.mac.inline:not(.audio) .volume-slider-container > .background-tint > div {"
        "    border-radius: var(--inline-controls-border-radius);"
        "    overflow: hidden;"
        "}"
        ".media-controls.mac.inline:not(.audio) .volume-slider-container > .slider {"
        "    width: 135px;"
        "}"
        ".media-controls.mac.inline:not(.audio) .volume-slider-container button[class*=\"mute\"],"
        ".media-controls.mac.inline:not(.audio) .volume-slider-container .button.mute {"
        "    position: relative !important;"
        "    transform: scale(0.8);"
        "    margin-left: 2px"
        "}"
        ":host(video.media-document.audio) .media-controls.mac.inline,"
        ".media-controls.mac.inline:not(.audio) * {"
        "    --inline-controls-bar-height: 46px;"
        "}"
        ".media-controls.mac.inline:not(.audio) .controls-bar.bottom,"
        ".media-controls.mac.inline:not(.audio) .controls-bar.top-left {"
        "    border-radius: var(--inline-controls-border-radius);"
        "}"
        ".media-controls.mac.inline:not(.audio) {"
        "    display: flex;"
        "    flex-direction: column;"
        "    justify-content: flex-end;"
        "    height: 100%;"
        "    position: relative;"
        "}"
        ".media-controls-container.mac.inline:not(.audio) > .media-controls.mac.inline {"
        "        position: relative;"
        "}"
        ".media-controls.mac.inline:not(.audio) .controls-bar.bottom {"
        "    position: relative !important;"
        "    bottom: auto !important;"
        "    left: auto !important;"
        "    right: auto !important;"
        "    top: auto !important;"
        "    display: flex !important;"
        "    align-items: center;"
        "    justify-content: space-between;"
        "    max-width: 750px;"
        "    margin: 0 auto;"
        "    width: 100%;"
        "    box-sizing: border-box;"
        "    padding-inline: var(--inline-controls-inside-margin);"
        "    margin-bottom: var(--inline-controls-inside-margin);"
        "}"
        ".media-controls.mac.inline:not(.audio) > .controls-bar.bottom > .left-cluster,"
        ".media-controls.mac.inline:not(.audio) > .controls-bar.bottom > .time-control,"
        ".media-controls.mac.inline:not(.audio) > .controls-bar.bottom > .right-cluster {"
        "    display: flex;"
        "    align-items: center;"
        "    position: relative;"
        "}"
        ".media-controls.mac.inline:not(.audio) > .controls-bar.bottom > .left-cluster {"
        "    flex: 0 0 auto;"
        "    justify-content: flex-start;"
        "    min-width: fit-content;"
        "}"
        ".media-controls.mac.inline:not(.audio) > .controls-bar.bottom > .time-control {"
        "    flex: 1 1 0;"
        "    justify-content: center;"
        "    min-width: 0;"
        "    max-width: calc(100% - 200px);"
        "    margin-inline: 12px;"
        "}"
        ".media-controls.mac.inline:not(.audio) > .controls-bar.bottom > .right-cluster {"
        "    flex: 0 0 auto;"
        "    justify-content: flex-end;"
        "    min-width: fit-content;"
        "}"
        ".media-controls.mac.inline:not(.audio) .buttons-container.right {"
        "    position: relative !important;"
        "    right: auto !important;"
        "    left: 35px !important;"
        "    top: auto !important;"
        "    bottom: auto !important;"
        "    display: flex !important;"
        "    align-items: center;"
        "    gap: 8px;"
        "}"
        ".media-controls.mac.inline:not(.audio) .controls-bar.bottom .time-control .slider,"
        ".media-controls.mac.inline:not(.audio) .controls-bar.bottom .time-control .scrubber{"
        "  display: flex;"
        "  flex: 1 1 auto;"
        "  max-width: 452px;"
        "  min-width: 0px;"
        "  position: relative;"
        "}"
        ".media-controls.mac.inline:not(.audio) .controls-bar.bottom .background-tint,"
        ".media-controls.mac.inline:not(.audio) .controls-bar.top-left .background-tint {"
        "    border-radius: var(--inline-controls-border-radius);"
        "    overflow: hidden;"
        "}"
        ".media-controls.mac.inline:not(.audio) .slider.default > .appearance {"
        "    top: 50%;"
        "    transform: translateY(-50%);"
        "    height: 7.5px;"
        "}"
        ".media-controls.mac.inline:not(.audio) .buttons-container.right > * {"
        "    display: flex;"
        "    position: relative !important;"
        "    left: -8px !important;"
        "}"
        ".media-controls.mac.inline:not(.audio) .slider.default > .appearance > .fill > .primary {"
        "    left: 0;"
        "    background-color: white;"
        "}"
        ".media-controls.mac.inline:not(.audio) .slider.default.allows-relative-scrubbing > .appearance > .fill > .primary {"
        "    background-color: white;"
        "}"
        ".slider.default > .appearance > .fill > .knob.pill,"
        ".media-controls.mac.inline:not(.audio) .volume-slider-container .slider.default > .appearance > .fill > .knob,"
        ".media-controls.mac.inline:not(.audio) .time-control .slider.default > .appearance > .fill > .knob {"
        "    top: -5px;"
        "    width: 21px;"
        "    height: 17px;"
        "    border-radius: 8.5px;"
        "    box-shadow: 0 2px 4px rgba(0, 0, 0, 0.3);"
        "    margin: -2px;"
        "}"
        ".media-controls.mac.inline.narrowviewer:not(.audio) .controls-bar.bottom {"
        "    padding-inline: 8px;"
        "    max-width: 100%;"
        "}"
        ".media-controls.mac.inline.narrowviewer:not(.audio) .volume-slider-container button[class*=\"mute\"],"
        ".media-controls.mac.inline.narrowviewer:not(.audio) .volume-slider-container .button.mute {"
        "    transform: scale(0.8) scaleX(-1);"
        "}"
        ".media-controls.mac.inline.narrowviewer:not(.audio) .controls-bar.top-left {"
        "    left: var(--inline-controls-inside-margin);"
        "}"_s
    };

    return macOSInlineMediaControlsStyleSheet;
}

static const String& macOSFullscreenMediaControlsStyleSheet()
{
    static NeverDestroyed<String> macOSFullscreenMediaControlsStyleSheet {
        "/* Controls bar */"
        ".media-controls.mac.fullscreen {"
        "    --controls-bar-width: 458px; /* Keep in sync with `minVideoWidth`. */"
        "    width: 100% !important;"
        "    height: 100% !important;"
        "}"
        ".media-controls.mac.fullscreen > .controls-bar {"
        "    left: calc((100% - var(--controls-bar-width)) / 2);"
        "    bottom: 25px;"
        "    width: var(--controls-bar-width);"
        "    height: 100px;"
        "    border-radius: 20px;"
        "}"
        ".media-controls.mac.fullscreen .controls-bar.bottom .background-tint > div {"
        "    border-radius: 20px;"
        "}"
        "/* Volume controls */"
        ".media-controls.mac.fullscreen:not(.uses-ltr-user-interface-layout-direction) .volume.slider {"
        "    transform: scaleX(-1);"
        "}"
        "/* Button containers */"
        ".media-controls.mac.fullscreen .buttons-container {"
        "    height: 44px;"
        "}"
        ".media-controls.mac.fullscreen .buttons-container.left {"
        "    top: 22.4px;"
        "    height: 16px;"
        "    width: 100px !important;"
        "    left: 15.5px"
        "}"
        ".media-controls.mac.fullscreen .buttons-container.center {"
        "    left: 50%;"
        "    top: 9px;"
        "    transform: translateX(-50%);"
        "}"
        "/* Make right container flush to the right */"
        ".media-controls.mac.fullscreen .buttons-container.right {"
        "    right: 11px;"
        "    top: 5px;"
        "}"
        "/* Buttons placement for right container */"
        ".media-controls.mac.fullscreen .buttons-container.right button {"
        "    top: 18px;"
        "}"
        "/* Scrubber */"
        ".media-controls.mac.fullscreen .time-control {"
        "    position: absolute;"
        "    left: 25px;"
        "    top: 64px;"
        "    height: 16px;"
        "    width: 408px !important;"
        "    display: flex;"
        "    align-items: center;"
        "}"
        "/* Status Label */"
        ".media-controls.mac.fullscreen > .controls-bar .status-label {"
        "    position: absolute;"
        "    left: 0;"
        "    right: 0;"
        "    bottom: 13px;"
        "    font-size: 14px;"
        "    text-align: center;"
        "}"
        ".media-controls.mac.fullscreen .buttons-container.left .slider {"
        "    width: 78px;"
        "}"
        ".media-controls.mac.fullscreen .slider.default > .appearance {"
        "    height: 6px;"
        "}"
        ".media-controls.mac.fullscreen .slider.default > .appearance > .fill > .primary {"
        "    background-color: white;"
        "}"
        ".media-controls.mac.fullscreen .slider.default.allows-relative-scrubbing > .appearance > .fill > .primary {"
        "    background-color: white;"
        "}"
        ".slider.default > .appearance > .fill > .knob.pill,"
        ".media-controls.mac.fullscreen .buttons-container.left .slider.default > .appearance > .fill > .knob,"
        ".media-controls.mac.fullscreen .time-control .slider.default > .appearance > .fill > .knob {"
        "    top: -5px;"
        "    width: 21px;"
        "    height: 17px;"
        "    border-radius: 8.5px;"
        "    box-shadow: 0 2px 4px rgba(0, 0, 0, 0.3);"
        "    margin: -2px;"
        "}"_s
    };
    return macOSFullscreenMediaControlsStyleSheet;
}
#endif // HAVE(MATERIAL_HOSTING)

Vector<String, 2> RenderThemeCocoa::mediaControlsStyleSheets(const HTMLMediaElement& mediaElement)
{
    if (m_mediaControlsStyleSheet.isEmpty())
        m_mediaControlsStyleSheet = StringImpl::createWithoutCopying(ModernMediaControlsUserAgentStyleSheet);

    auto mediaControlsStyleSheets = Vector<String, 2>::from(m_mediaControlsStyleSheet);

#if HAVE(MATERIAL_HOSTING)
    // FIXME (153018199): Glass material styles can be added directly to ModernMediaControlsUserAgentStyleSheet
    // once `@supports (-apple-visual-effect: -apple-system-glass-material)` behaves correctly in WebKitLegacy.
    if (mediaElement.document().settings().hostedBlurMaterialInMediaControlsEnabled())
        mediaControlsStyleSheets.append(glassMaterialMediaControlsStyleSheet());

    if (mediaElement.document().settings().mediaControlsMacInlineSizeSpecsEnabled()) {
        mediaControlsStyleSheets.append(macOSInlineMediaControlsStyleSheet());
        mediaControlsStyleSheets.append(macOSFullscreenMediaControlsStyleSheet());
    }
#else
    UNUSED_PARAM(mediaElement);
#endif

    return mediaControlsStyleSheets;
}

Vector<String, 2> RenderThemeCocoa::mediaControlsScripts()
{
    // FIXME: Localized strings are not worth having a script. We should make it JSON data etc. instead.
    if (m_mediaControlsLocalizedStringsScript.isEmpty()) {
        NSBundle *bundle = [NSBundle bundleForClass:[WebCoreRenderThemeBundle class]];
        m_mediaControlsLocalizedStringsScript = [NSString stringWithContentsOfFile:retainPtr([bundle pathForResource:@"modern-media-controls-localized-strings" ofType:@"js"]).get() encoding:NSUTF8StringEncoding error:nil];
    }

    if (m_mediaControlsScript.isEmpty())
        m_mediaControlsScript = StringImpl::createWithoutCopying(ModernMediaControlsJavaScript);

    return {
        m_mediaControlsLocalizedStringsScript,
        m_mediaControlsScript,
    };
}

RefPtr<FragmentedSharedBuffer> RenderThemeCocoa::mediaControlsImageDataForIconNameAndType(const String& iconName, const String& iconType)
{
    NSString *directory = @"modern-media-controls/images";
    NSBundle *bundle = [NSBundle bundleForClass:[WebCoreRenderThemeBundle class]];
    return SharedBuffer::create([NSData dataWithContentsOfFile:retainPtr([bundle pathForResource:iconName.createNSString().get() ofType:iconType.createNSString().get() inDirectory:directory]).get()]);
}

String RenderThemeCocoa::mediaControlsBase64StringForIconNameAndType(const String& iconName, const String& iconType)
{
    NSString *directory = @"modern-media-controls/images";
    NSBundle *bundle = [NSBundle bundleForClass:[WebCoreRenderThemeBundle class]];
    return [[NSData dataWithContentsOfFile:retainPtr([bundle pathForResource:iconName.createNSString().get() ofType:iconType.createNSString().get() inDirectory:directory]).get()] base64EncodedStringWithOptions:0];
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

Color RenderThemeCocoa::controlTintColor(const RenderStyle& style, OptionSet<StyleColorOptions> options) const
{
    if (!style.accentColor().isAuto())
        return style.usedAccentColor(options);

#if PLATFORM(MAC)
    auto cssColorValue = CSSValueAppleSystemControlAccent;
#else
    auto cssColorValue = CSSValueAppleSystemBlue;
#endif
    return systemColor(cssColorValue, options | StyleColorOptions::UseSystemAppearance);
}

void RenderThemeCocoa::inflateRectForControlRenderer(const RenderElement& renderer, FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (inflateRectForControlRendererForVectorBasedControls(renderer, rect))
        return;
#endif

    RenderTheme::inflateRectForControlRenderer(renderer, rect);
}

Style::LineWidthBox RenderThemeCocoa::controlBorder(StyleAppearance appearance, const FontCascade& font, const Style::LineWidthBox& zoomedBox, float zoomFactor, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (formControlRefreshEnabled(element))
        return zoomedBox;
#endif

    return RenderTheme::controlBorder(appearance, font, zoomedBox, zoomFactor, element);
}

LayoutRect RenderThemeCocoa::adjustedPaintRect(const RenderBox& box, const LayoutRect& paintRect) const
{
#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(FORM_CONTROL_REFRESH))
    // Workaround for <rdar://problem/6209763>. Force the painting bounds of checkboxes and radio controls to be square.
    if (box.style().usedAppearance() == StyleAppearance::Checkbox || box.style().usedAppearance() == StyleAppearance::Radio) {
        float width = std::min(paintRect.width(), paintRect.height());
        float height = width;
        return enclosingLayoutRect(FloatRect(paintRect.x(), paintRect.y() + (box.height() - height) / 2, width, height)); // Vertically center the checkbox.
    }
#else
    UNUSED_PARAM(box);
#endif
    return paintRect;
}

void RenderThemeCocoa::adjustRepaintRect(const RenderBox& renderer, FloatRect& rect)
{
    auto repaintRect = rect;
    inflateRectForControlRenderer(renderer, repaintRect);
    renderer.flipForWritingMode(repaintRect);
    rect = repaintRect;
}

#if ENABLE(FORM_CONTROL_REFRESH)

bool RenderThemeCocoa::controlSupportsTints(const RenderElement& box) const
{
#if PLATFORM(MAC)
    switch (box.style().usedAppearance()) {
    case StyleAppearance::Button:
        return isSubmitStyleButton(box.element());
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio:
        return isChecked(box) || isIndeterminate(box);
    case StyleAppearance::ListButton:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::SliderHorizontal:
    case StyleAppearance::SliderVertical:
        return true;
    case StyleAppearance::Switch:
        return isChecked(box);
    default:
        break;
    }
#else
    UNUSED_PARAM(box);
#endif

    return false;
}

enum class ControlSize : uint8_t {
    Micro,
    Mini,
    Small,
    Medium,
    Large,
    ExtraLarge
};

struct PathWithSize {
    Path path;
    FloatSize originalSize;
};

static constexpr auto sliderThumbShadowRadius = 2.f;

bool RenderThemeCocoa::inflateRectForControlRendererForVectorBasedControls(const RenderElement& renderer, FloatRect& rect) const
{
    if (!formControlRefreshEnabled(renderer))
        return false;

    auto appearance = renderer.style().usedAppearance();

    switch (appearance) {
    case StyleAppearance::SliderThumbHorizontal:
    case StyleAppearance::SliderThumbVertical: {
        rect.inflate(sliderThumbShadowRadius * renderer.style().usedZoom());
        break;
    }
    default:
        break;
    }

    return true;
}

bool RenderThemeCocoa::canCreateControlPartForRendererForVectorBasedControls(const RenderElement& renderer) const
{
    auto type = renderer.style().usedAppearance();

    if (type == StyleAppearance::SliderThumbHorizontal
        || type == StyleAppearance::SliderThumbVertical
        || type == StyleAppearance::SliderHorizontal
        || type == StyleAppearance::SliderVertical
        || type == StyleAppearance::Meter
        || type == StyleAppearance::Checkbox
        || type == StyleAppearance::Radio
        || type == StyleAppearance::ProgressBar
        || type == StyleAppearance::SwitchThumb
        || type == StyleAppearance::SwitchTrack
        || type == StyleAppearance::SearchFieldCancelButton
        || type == StyleAppearance::SearchFieldResultsButton
        || type == StyleAppearance::SearchFieldResultsDecoration
        || type == StyleAppearance::SearchField
        || type == StyleAppearance::Button
        || type == StyleAppearance::DefaultButton
        || type == StyleAppearance::PushButton
        || type == StyleAppearance::SquareButton
        || type == StyleAppearance::InnerSpinButton
        || type == StyleAppearance::ColorWell
        || type == StyleAppearance::ColorWellSwatch
        || type == StyleAppearance::ColorWellSwatchOverlay
        || type == StyleAppearance::ColorWellSwatchWrapper
        || type == StyleAppearance::TextArea
        || type == StyleAppearance::TextField
        || type == StyleAppearance::ListButton) {
        return !renderer.settings().formControlRefreshEnabled();
    }

#if ENABLE(APPLE_PAY)
    if (type == StyleAppearance::ApplePayButton)
        return true;
#endif

#if ENABLE(SERVICE_CONTROLS)
    if (type == StyleAppearance::ImageControlsButton)
        return true;
#endif

    return false;
}

bool RenderThemeCocoa::canCreateControlPartForBorderOnlyForVectorBasedControls(const RenderElement& renderer) const
{
    auto appearance = renderer.style().usedAppearance();

    if (appearance == StyleAppearance::TextArea
        || appearance == StyleAppearance::TextField) {
        return !renderer.settings().formControlRefreshEnabled();
    }

    return appearance == StyleAppearance::Listbox;
}

bool RenderThemeCocoa::canCreateControlPartForDecorationsForVectorBasedControls(const RenderElement& renderer) const
{
    return renderer.style().usedAppearance() == StyleAppearance::MenulistButton;
}

constexpr auto kDisabledControlAlpha = 0.4;

constexpr auto nativeControlBorderInlineSizeForVectorBasedControls = 1.0f;

constexpr auto checkboxRadioSizeForVectorBasedControls = 16.f;
constexpr auto checkboxRadioBorderWidthForVectorBasedControls = 1.5f;

static bool controlIsFocusedWithOutlineStyleAutoForVectorBasedControls(const RenderElement& renderer)
{
    return RenderTheme::singleton().isFocused(renderer) && renderer.style().outlineStyle() == OutlineStyle::Auto;
}

static constexpr auto checkboxRadioBorderDisabledOpacityForVectorBasedControls = 0.5f;

static Color checkboxRadioIndicatorColorForVectorBasedControls(const Color& tintColor, OptionSet<ControlStyle::State> states, OptionSet<StyleColorOptions> styleColorOptions)
{
    auto indicatorColor = foregroundColorForBackgroundColor(tintColor);
#if PLATFORM(MAC)
    const auto isWindowActive = states.contains(ControlStyle::State::WindowActive);
    indicatorColor = isWindowActive ? indicatorColor : RenderTheme::singleton().systemColor(CSSValueAppleSystemLabel, styleColorOptions);
#else
    UNUSED_PARAM(styleColorOptions);
#endif
    const auto isEnabled = states.contains(ControlStyle::State::Enabled);

    if (!isEnabled)
        indicatorColor = indicatorColor.colorWithAlphaMultipliedBy(checkboxRadioBorderDisabledOpacityForVectorBasedControls);

    return indicatorColor;
}

#if PLATFORM(MAC)

static Color colorWithContrastOverlay(const Color color, OptionSet<StyleColorOptions> styleColorOptions, float opacity)
{
    ASSERT(opacity <= 1.f && opacity >= 0.f);

    const auto usingDarkMode = styleColorOptions.contains(StyleColorOptions::UseDarkAppearance);
    const Color overlayColor = usingDarkMode ? Color::white : Color::black;

    return blendSourceOver(color, overlayColor.colorWithAlphaMultipliedBy(opacity));
}

#endif

static Color platformAdjustedColorForPressedState(const Color color, OptionSet<StyleColorOptions> styleColorOptions)
{
#if PLATFORM(MAC)
    return colorWithContrastOverlay(color, styleColorOptions, 0.2f);
#else
    UNUSED_PARAM(styleColorOptions);
    return color.colorWithAlphaMultipliedBy(0.75f);
#endif
}

static void drawShapeWithBorder(GraphicsContext& context, float deviceScaleFactor, Path path, FloatRect boundingRect, Color backgroundColor, float borderThickness, Color borderColor)
{
    GraphicsContextStateSaver stateSaver { context };

    // Despite the border and background being painted with the same clip, the background
    // may bleed past the border unless we scale it down slightly.

    const auto shrunkRect = shrinkRectByOneDevicePixel(context, LayoutRect(boundingRect), deviceScaleFactor);
    const auto backgroundScale = shrunkRect.size() / boundingRect.size();

    context.clipPath(path);

    context.save();
    context.translate(boundingRect.center());
    context.scale(backgroundScale);
    context.translate(-boundingRect.center());
    context.setFillColor(backgroundColor);
    context.fillPath(path);
    context.restore();

    context.setStrokeColor(borderColor);
    context.setStrokeThickness(borderThickness * 2);
    context.strokePath(path);
}

static RefPtr<Gradient> checkboxRadioBackgroundGradientForVectorBasedControls(const FloatRect& rect, OptionSet<ControlStyle::State> states)
{
    // FIXME: This is just a copy of RenderThemeIOS::checkboxRadioBackgroundGradient(...). Refactor to remove this duplicate code.
    bool isPressed = states.contains(ControlStyle::State::Pressed);
    if (isPressed)
        return nullptr;

    bool isEmpty = !states.containsAny({ ControlStyle::State::Checked, ControlStyle::State::Indeterminate });
    auto gradient = Gradient::create(Gradient::LinearData { rect.minXMinYCorner(), rect.maxXMaxYCorner() }, { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied });
    gradient->addColorStop({ 0.0f, DisplayP3<float> { 0, 0, 0, isEmpty ? 0.05f : 0.125f } });
    gradient->addColorStop({ 1.0f, DisplayP3<float> { 0, 0, 0, 0 } });
    return gradient;
}

constexpr auto checkboxCornerRadiusRatio = 5.5f / checkboxRadioSizeForVectorBasedControls;

static void paintCheckboxRadioInnerShadowForVectorBasedControls(const PaintInfo& paintInfo, const RoundedShape& shape, OptionSet<ControlStyle::State> states, bool isCheckbox)
{
    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver { context };

    const auto paintRect = shape.boundingRect;
    const auto cornerRadius = shape.cornerRadius;

    if (auto gradient = checkboxRadioBackgroundGradientForVectorBasedControls(paintRect, states)) {
        context.setFillGradient(*gradient);
        context.fillPath(shape.path.value_or(Path { }));
    }

    const FloatSize innerShadowOffset { 2, 2 };
    constexpr auto innerShadowBlur = 3.0f;

    bool isEmpty = !states.containsAny({ ControlStyle::State::Checked, ControlStyle::State::Indeterminate });
    auto firstShadowColor = DisplayP3<float> { 0, 0, 0, isEmpty ? 0.05f : 0.1f };
    context.setDropShadow({ innerShadowOffset, innerShadowBlur, firstShadowColor, ShadowRadiusMode::Default });
    context.setFillColor(Color::black);

    Path innerShadowPath;
    auto innerShadowRect = paintRect;
    innerShadowRect.inflate(std::max<float>(innerShadowOffset.width(), innerShadowOffset.height()) + innerShadowBlur);
    innerShadowPath.addRect(innerShadowRect);

    auto innerShadowHoleRect = paintRect;
    // FIXME: This is not from the spec; but without it we get antialiasing fringe from the fill; we need a better solution.
    innerShadowHoleRect.inflate(0.5);

    if (isCheckbox)
        innerShadowPath.addContinuousRoundedRect(innerShadowHoleRect, cornerRadius, cornerRadius);
    else
        innerShadowPath.addEllipseInRect(innerShadowHoleRect);

    context.setFillRule(WindRule::EvenOdd);
    context.fillPath(innerShadowPath);

    constexpr auto secondShadowColor = DisplayP3<float> { 1, 1, 1, 0.5f };
    context.setDropShadow({ FloatSize { 0, 0 }, 1, secondShadowColor, ShadowRadiusMode::Default });

    context.fillPath(innerShadowPath);
}

static Color checkboxRadioBorderColorForVectorBasedControls(OptionSet<ControlStyle::State> states, OptionSet<StyleColorOptions> styleColorOptions)
{
    const auto defaultBorderColor = RenderTheme::singleton().systemColor(CSSValueAppleSystemSecondaryLabel, styleColorOptions);

    if (!states.contains(ControlStyle::State::Enabled))
        return defaultBorderColor.colorWithAlphaMultipliedBy(checkboxRadioBorderDisabledOpacityForVectorBasedControls);

    if (states.contains(ControlStyle::State::Pressed))
        return platformAdjustedColorForPressedState(defaultBorderColor, styleColorOptions);

    return defaultBorderColor;
}

static Color adjustCheckboxRadioBackgroundColorDisabledState(const Color& backgroundColor, OptionSet<ControlStyle::State> states, OptionSet<StyleColorOptions> styleColorOptions)
{
#if PLATFORM(IOS_FAMILY)
    const auto isEmpty = !states.containsAny({ ControlStyle::State::Checked, ControlStyle::State::Indeterminate });
    if (PAL::currentUserInterfaceIdiomIsVision()) {
        auto disabledBackgroundColor = RenderTheme::singleton().systemColor(isEmpty ? CSSValueWebkitControlBackground : CSSValueAppleSystemOpaqueTertiaryFill, styleColorOptions);
        return colorCompositedOverCanvasColor(disabledBackgroundColor, styleColorOptions);
    }
#else
    UNUSED_PARAM(states);
#endif
    auto disabledBackgroundColor = backgroundColor.colorWithAlphaMultipliedBy(checkboxRadioBorderDisabledOpacityForVectorBasedControls);
    return colorCompositedOverCanvasColor(disabledBackgroundColor, styleColorOptions);
}

Color RenderThemeCocoa::checkboxRadioBackgroundColorForVectorBasedControls(const RenderStyle& style, OptionSet<ControlStyle::State> states, OptionSet<StyleColorOptions> styleColorOptions) const
{
    const auto isEmpty = !states.containsAny({ ControlStyle::State::Checked, ControlStyle::State::Indeterminate });

    Color backgroundColor = Color::white;
    const auto tintColor = controlTintColorWithContrast(style, styleColorOptions);

#if PLATFORM(IOS_FAMILY)
    if (PAL::currentUserInterfaceIdiomIsVision()) {
        backgroundColor = isEmpty ? Color(DisplayP3<float> { 0.835, 0.835, 0.835 }) : tintColor;
        return colorCompositedOverCanvasColor(backgroundColor, styleColorOptions);
    }

    backgroundColor = isEmpty ? systemColor(CSSValueWebkitControlBackground, styleColorOptions) : tintColor;
#else
    const auto isWindowActive = states.contains(ControlStyle::State::WindowActive);

    if (isEmpty)
        backgroundColor = systemColor(CSSValueWebkitControlBackground, styleColorOptions);
    else if (!isWindowActive)
        backgroundColor = systemColor(CSSValueAppleSystemTertiaryFill, styleColorOptions);
    else
        backgroundColor = tintColor;
#endif

    return colorCompositedOverCanvasColor(backgroundColor, styleColorOptions);
}

static RoundedShape continuousRoundedShape(const FloatRect& rect, const float cornerRadius, ShouldComputePath computePath)
{
    if (computePath == ShouldComputePath::No)
        return { { }, rect, cornerRadius, CornerType::Continuous };

    Path path;
    path.addContinuousRoundedRect(rect, cornerRadius);
    return { WTFMove(path), rect, cornerRadius, CornerType::Continuous };
}

static RoundedShape roundedShape(const FloatRect& rect, const float cornerRadius, ShouldComputePath computePath)
{
    if (computePath == ShouldComputePath::No)
        return { { }, rect, cornerRadius, CornerType::Noncontinuous };

    Path path;
    path.addRoundedRect(FloatRoundedRect { rect, FloatRoundedRect::Radii { cornerRadius, cornerRadius } });
    return { WTFMove(path), rect, cornerRadius, CornerType::Noncontinuous };
}

static bool nodeIsDateOrTimeRelatedInput(Node* node)
{
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(node)) {
        return input->isDateField()
        || input->isDateTimeLocalField()
        || input->isMonthField()
        || input->isTimeField()
        || input->isWeekField();
    }

    return false;
}

static RoundedShape shapeForButton(const RenderElement& box, const FloatRect& rect, ShouldComputePath computePath = ShouldComputePath::Yes)
{
    CheckedRef style = box.style();
    const auto zoomScale = style->usedZoom();

    auto controlRadius = 0.f;
#if PLATFORM(MAC)
    controlRadius = 6.f * zoomScale;
#else
    static constexpr auto largeButtonHeight = 45;
    static constexpr auto largeButtonRadiusRatio = 0.35 / 2;

    const auto isVertical = !style->writingMode().isHorizontal();
    const auto boxLogicalHeight = isVertical ? rect.width() : rect.height();
    const auto minDimension = std::min(rect.width(), rect.height());

    const auto radiusForLargeButton = minDimension * largeButtonRadiusRatio;
    if (boxLogicalHeight >= largeButtonHeight * zoomScale)
        controlRadius = radiusForLargeButton;
    else {
        controlRadius = minDimension / 2;

        // If trying to make the button pill-shaped would make it a circle
        // or nearly circle, use the non-pill shape instead.
        const auto sizeRatio = rect.width() / rect.height();
        const auto limitingRatio = 1.5f;
        if (limitingRatio > sizeRatio && sizeRatio > 1 / limitingRatio)
            controlRadius = radiusForLargeButton;
    }
#endif

    return continuousRoundedShape(rect, controlRadius, computePath);
}

static RoundedShape shapeForCheckbox(const FloatRect& rect, ShouldComputePath computePath = ShouldComputePath::Yes)
{
    // At paint time, the rect will already be square due to RenderThemeCocoa::adjustedPaintRect.
    // During the computation of interaction regions, we do not force a square rect.
    const auto cornerRadius = checkboxCornerRadiusRatio * std::min(rect.width(), rect.height());
    return continuousRoundedShape(rect, cornerRadius, computePath);
}

static RoundedShape shapeForRadioButton(const FloatRect& rect, ShouldComputePath computePath = ShouldComputePath::Yes)
{
    // At paint time, the rect will already be square due to RenderThemeCocoa::adjustedPaintRect.
    // During the computation of interaction regions, we do not force a square rect.
    const auto cornerRadius = std::min(rect.width(), rect.height()) / 2.f;
    return roundedShape(rect, cornerRadius, computePath);
}

static constexpr auto searchFieldDecorationEmMargin = 0.36f;
static constexpr auto searchFieldDecorationEmSize = 1.f;
#if PLATFORM(MAC)
static constexpr auto searchFieldDecorationWithDropdownEmSizeLTR = 1.5f;
static constexpr auto searchFieldDecorationWithDropdownEmSizeRTL = 1.7f;
#endif

static bool searchFieldCanBeCapsule(const RenderElement& box, const FloatRect& rect, float pixelsPerEm, bool supportsResults)
{
    // Depending on dimensions and styles, it might not be possible to make the control
    // capsule-shaped in a reasonable manner, or it may look especially strange with a
    // capsule shape. We check for these cases below.

    CheckedRef style = box.style();
    const auto isVertical = style->writingMode().isVertical();
    const auto inlineLength = isVertical ? rect.height() : rect.width();
    const auto boxLength = isVertical ? rect.width() : rect.height();
    const auto borderRadius = boxLength / 2.f;

    // Check if capsule shape would be nearing a circle or if the round ends would
    // be facing the wrong direction.
    if (inlineLength < borderRadius * 3.f)
        return false;

#if PLATFORM(MAC)
    const auto isInlineFlipped = style->writingMode().isInlineFlipped();
    const auto sizeWithDropdown = isInlineFlipped ? searchFieldDecorationWithDropdownEmSizeRTL : searchFieldDecorationWithDropdownEmSizeLTR;

    const auto effectiveSearchFieldDecorationEmSize = supportsResults ? sizeWithDropdown : searchFieldDecorationEmSize;
#else
    UNUSED_PARAM(supportsResults);
    const auto effectiveSearchFieldDecorationEmSize = searchFieldDecorationEmSize;
#endif

    static constexpr auto searchFieldInlinePaddingEmSize = 0.55f;

    // Check if capsule shape would result in text being drawn within the starting
    // semicircle of the shape rather than the flat area.
    const auto textGapEmSize = effectiveSearchFieldDecorationEmSize + searchFieldInlinePaddingEmSize + searchFieldDecorationEmMargin;

    return textGapEmSize * pixelsPerEm >= borderRadius;
}

static CSSToLengthConversionData conversionDataForStyle(const RenderStyle& style)
{
    CSSToLengthConversionData conversionData(style, nullptr, nullptr, nullptr);
    if (style.evaluationTimeZoomEnabled())
        return conversionData.copyWithAdjustedZoom(1.0f, CSS::RangeZoomOptions::Unzoomed);
    return conversionData;
}

static RoundedShape shapeForSearchField(const RenderElement& box, const FloatRect& rect, ShouldComputePath computePath = ShouldComputePath::Yes)
{
    CheckedRef style = box.style();
    const auto isVertical = style->writingMode().isVertical();

    CSSToLengthConversionData conversionData(style, nullptr, nullptr, nullptr);

    auto supportsResults = false;
#if PLATFORM(MAC)
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(box.element()))
        supportsResults = input->maxResults() > 0;
#endif

    Ref emSize = CSSPrimitiveValue::create(1, CSSUnitType::CSS_EM);
    const auto pixelsPerEm = emSize->resolveAsLength<float>(conversionData);
    const auto usingCapsuleShape = searchFieldCanBeCapsule(box, rect, pixelsPerEm, supportsResults);

    float rectRadius = 0.f;
    if (usingCapsuleShape)
        rectRadius = (isVertical ? rect.width() : rect.height()) / 2.f;
    else
        rectRadius = std::min(rect.width(), rect.height()) * 0.15f;

    return continuousRoundedShape(rect, rectRadius, computePath);
}

static RoundedShape shapeForSwitchTrack(const RenderElement& box, const FloatRect& rect, ShouldComputePath computePath = ShouldComputePath::Yes)
{
    const auto states = RenderTheme::singleton().extractControlStyleStatesForRenderer(box);
    const auto isVertical = states.contains(ControlStyle::State::VerticalWritingMode);
    const auto logicalHeight = isVertical ? rect.width() : rect.height();
    const auto rectRadius = logicalHeight / 2.f;

#if PLATFORM(VISION)
    return roundedShape(rect, rectRadius, computePath);
#else
    return continuousRoundedShape(rect, rectRadius, computePath);
#endif
}

static constexpr auto defaultCornerRadiusForTextBasedControls = 5.f;

static RoundedShape shapeForSliderThumb(const RenderElement& box, const FloatRect& rect, ShouldComputePath computePath = ShouldComputePath::Yes)
{
    const auto deviceScaleFactor = box.document().deviceScaleFactor();

    const auto snappedRect = snapRectToDevicePixels(LayoutRect(rect), deviceScaleFactor);
    const auto cornerRadius = std::min(snappedRect.width(), snappedRect.height()) / 2.f;

    return continuousRoundedShape(snappedRect, cornerRadius, computePath);
}

static RoundedShape shapeForTextAreaOrTextField(const RenderElement& box, const FloatRect& rect, ShouldComputePath computePath = ShouldComputePath::Yes)
{
    const auto cornerRadius = defaultCornerRadiusForTextBasedControls * box.style().usedZoom();
    return continuousRoundedShape(rect, cornerRadius, computePath);
}

bool RenderThemeCocoa::paintCheckboxForVectorBasedControls(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    // FIXME: A pre-existing flicker issue caused by a delay between the pressed state ending
    // and the new checked state updating is now more apparent due to color changes in the
    // pressed state.

    if (!formControlRefreshEnabled(box))
        return false;

    const auto checkboxShape = shapeForCheckbox(rect);
    if (!checkboxShape.path)
        return false;

    const auto path = checkboxShape.path.value();
    const auto paintRect = checkboxShape.boundingRect;

#if PLATFORM(IOS_FAMILY)
    bool isVision = PAL::currentUserInterfaceIdiomIsVision();
#else
    bool isVision = false;
#endif

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver { context };

    auto controlStates = RenderTheme::singleton().extractControlStyleStatesForRenderer(box);
    auto deviceScaleFactor = box.document().deviceScaleFactor();
    auto styleColorOptions = box.styleColorOptions();
    auto usedZoom = box.style().usedZoom();

    const auto isEnabled = controlStates.contains(ControlStyle::State::Enabled);
    const auto isPressed = controlStates.contains(ControlStyle::State::Pressed);

    auto backgroundColor = checkboxRadioBackgroundColorForVectorBasedControls(box.style(), controlStates, styleColorOptions);
    const auto indicatorColor = checkboxRadioIndicatorColorForVectorBasedControls(backgroundColor, controlStates, styleColorOptions);

    if (!isEnabled)
        backgroundColor = adjustCheckboxRadioBackgroundColorDisabledState(backgroundColor, controlStates, styleColorOptions);
    else if (isPressed)
        backgroundColor = platformAdjustedColorForPressedState(backgroundColor, styleColorOptions);

    auto checked = controlStates.contains(ControlStyle::State::Checked);
    auto indeterminate = controlStates.contains(ControlStyle::State::Indeterminate);
    auto empty = !checked && !indeterminate;

    if (controlIsFocusedWithOutlineStyleAutoForVectorBasedControls(box))
        drawFocusRingForPathForVectorBasedControls(box, paintInfo, paintRect, path);

    context.clipPath(path);

    if (empty) {
        if (isVision) {
            context.setFillColor(backgroundColor);
            context.fillPath(path);

            paintCheckboxRadioInnerShadowForVectorBasedControls(paintInfo, checkboxShape, controlStates, true);
        } else {
            const auto borderColor = checkboxRadioBorderColorForVectorBasedControls(controlStates, styleColorOptions);
            const auto borderThickness = checkboxRadioBorderWidthForVectorBasedControls * usedZoom;
            context.setStrokeStyle(StrokeStyle::SolidStroke);

            drawShapeWithBorder(context, deviceScaleFactor, path, paintRect, backgroundColor, borderThickness, borderColor);
        }

        return true;
    }

    context.setFillColor(backgroundColor);
    context.fillPath(path);

    if (isVision)
        paintCheckboxRadioInnerShadowForVectorBasedControls(paintInfo, checkboxShape, controlStates, true);

    Path glyphPath;

    if (indeterminate) {
        const FloatSize indeterminateBarSize(0.40625 * paintRect.width(), 0.15625 * paintRect.height());
        const FloatSize indeterminateBarRoundingRadii(indeterminateBarSize.height() / 2.f, indeterminateBarSize.height() / 2.f);
        const FloatPoint indeterminateBarLocation(paintRect.center() - indeterminateBarSize / 2.f);

        FloatRect indeterminateBarRect(indeterminateBarLocation, indeterminateBarSize);
        glyphPath.addRoundedRect(indeterminateBarRect, indeterminateBarRoundingRadii);
    } else {
        glyphPath.moveTo({ 28.174f, 68.652f });
        glyphPath.addBezierCurveTo({ 31.006f, 68.652f }, { 33.154f, 67.578f }, { 34.668f, 65.332f });
        glyphPath.addLineTo({ 70.02f, 11.28f });
        glyphPath.addBezierCurveTo({ 71.094f, 9.62f }, { 71.582f, 8.107f }, { 71.582f, 6.642f });
        glyphPath.addBezierCurveTo({ 71.582f, 2.784f }, { 68.652f, 0.001f }, { 64.697f, 0.001f });
        glyphPath.addBezierCurveTo({ 62.012f, 0.001f }, { 60.352f, 0.978f }, { 58.691f, 3.565f });
        glyphPath.addLineTo({ 28.027f, 52.1f });
        glyphPath.addLineTo({ 12.354f, 32.52f });
        glyphPath.addBezierCurveTo({ 10.84f, 30.664f }, { 9.18f, 29.834f }, { 6.884f, 29.834f });
        glyphPath.addBezierCurveTo({ 2.882f, 29.834f }, { 0.0f, 32.666f }, { 0.0f, 36.572f });
        glyphPath.addBezierCurveTo({ 0.0f, 38.282f }, { 0.537f, 39.795f }, { 2.002f, 41.504f });
        glyphPath.addLineTo({ 21.826f, 65.625f });
        glyphPath.addBezierCurveTo({ 23.536f, 67.675f }, { 25.536f, 68.652f }, { 28.174f, 68.652f });

        const FloatSize checkmarkSize(71.582f, 68.652f);
        const float checkmarkToRectWidthRatio = 0.581875f;
        float topPaddingToHeightRatio = 0.24375f;
        float scale = (checkmarkToRectWidthRatio * paintRect.width()) / checkmarkSize.width();

        float dx = paintRect.center().x() - (checkmarkSize.width() * scale * 0.5f);
        float dy = paintRect.y() + (paintRect.height() * topPaddingToHeightRatio);

        AffineTransform transform;
        transform.translate(dx, dy);
        transform.scale(scale);
        glyphPath.transform(transform);
    }

    context.setFillColor(indicatorColor);
    context.fillPath(glyphPath);

#if PLATFORM(MAC)
    if (Theme::singleton().userPrefersContrast())
        drawHighContrastOutline(context, path, box.styleColorOptions());
#endif

    return true;
}

bool RenderThemeCocoa::paintRadioForVectorBasedControls(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    if (!formControlRefreshEnabled(box))
        return false;

#if PLATFORM(IOS_FAMILY)
    bool isVision = PAL::currentUserInterfaceIdiomIsVision();
#else
    bool isVision = false;
#endif

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    const auto radioShape = shapeForRadioButton(rect);
    if (!radioShape.path)
        return false;

    const auto boundingPath = radioShape.path.value();
    const auto paintRect = radioShape.boundingRect;

    const auto controlStates = RenderTheme::singleton().extractControlStyleStatesForRenderer(box);
    const auto deviceScaleFactor = box.document().deviceScaleFactor();
    const auto styleColorOptions = box.styleColorOptions();
    const auto usedZoom = box.style().usedZoom();

    const auto isEnabled = controlStates.contains(ControlStyle::State::Enabled);
    const auto isPressed = controlStates.contains(ControlStyle::State::Pressed);

    auto backgroundColor = checkboxRadioBackgroundColorForVectorBasedControls(box.style(), controlStates, styleColorOptions);
    const auto indicatorColor = checkboxRadioIndicatorColorForVectorBasedControls(backgroundColor, controlStates, styleColorOptions);

    if (!isEnabled)
        backgroundColor = adjustCheckboxRadioBackgroundColorDisabledState(backgroundColor, controlStates, styleColorOptions);
    else if (isPressed)
        backgroundColor = platformAdjustedColorForPressedState(backgroundColor, styleColorOptions);

    if (isVision) {
        context.save();
        context.clipPath(boundingPath);
        context.setFillColor(backgroundColor);
        context.fillPath(boundingPath);
        paintCheckboxRadioInnerShadowForVectorBasedControls(paintInfo, radioShape, controlStates, false);
        context.restore();
    }

    if (controlStates.contains(ControlStyle::State::Checked)) {
        if (!isVision) {
            context.setFillColor(backgroundColor);
            context.fillPath(boundingPath);
        }

        constexpr float innerInverseRatio = 7 / 20.0f;

        FloatRect innerCircleRect(paintRect);
        innerCircleRect.inflateX(-innerCircleRect.width() * innerInverseRatio);
        innerCircleRect.inflateY(-innerCircleRect.height() * innerInverseRatio);

        context.setFillColor(indicatorColor);
        context.fillEllipse(innerCircleRect);

#if PLATFORM(MAC)
    if (Theme::singleton().userPrefersContrast())
        drawHighContrastOutline(context, boundingPath, box.styleColorOptions());
#endif
    } else if (!isVision) {
        const auto borderColor = checkboxRadioBorderColorForVectorBasedControls(controlStates, styleColorOptions);
        const auto borderThickness = checkboxRadioBorderWidthForVectorBasedControls * usedZoom;

        drawShapeWithBorder(context, deviceScaleFactor, boundingPath, paintRect, backgroundColor, borderThickness, borderColor);
    }

    if (controlIsFocusedWithOutlineStyleAutoForVectorBasedControls(box))
        drawFocusRingForPathForVectorBasedControls(box, paintInfo, paintRect, boundingPath);

    return true;
}

bool RenderThemeCocoa::paintButtonForVectorBasedControls(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    if (!formControlRefreshEnabled(box))
        return false;

    CheckedRef style = box.style();
    const auto deviceScaleFactor = box.document().deviceScaleFactor();
    const auto styleColorOptions = box.styleColorOptions();

    const auto zoomScale = style->usedZoom();
    const auto borderWidth = zoomScale;

    const auto states = RenderTheme::singleton().extractControlStyleStatesForRenderer(box);
    const auto isEnabled = states.contains(ControlStyle::State::Enabled);

    Color backgroundColor;

    if (box.theme().isDefault(box))
        backgroundColor = controlTintColorWithContrast(box.style(), styleColorOptions);
    else {
        auto isWindowActive = true;
#if PLATFORM(MAC)
        isWindowActive = states.contains(ControlStyle::State::WindowActive);
#endif
        if (isSubmitStyleButton(box.element()) && isWindowActive)
            backgroundColor = controlTintColorWithContrast(box.style(), styleColorOptions);
        else
            backgroundColor = colorCompositedOverCanvasColor(CSSValueAppleSystemOpaqueSecondaryFill, styleColorOptions);
    }

    if (!isEnabled)
        backgroundColor = backgroundColor.colorWithAlphaMultipliedBy(kDisabledControlAlpha);
    else if (states.contains(ControlStyle::State::Pressed))
        backgroundColor = platformAdjustedColorForPressedState(backgroundColor, styleColorOptions);

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    const auto buttonShape = shapeForButton(box, rect);
    if (!buttonShape.path)
        return false;

    const auto path = buttonShape.path.value();
    const auto boundingRect = buttonShape.boundingRect;

#if PLATFORM(MAC)
    const auto userPrefersContrast = Theme::singleton().userPrefersContrast();
    const auto borderColor = userPrefersContrast ? highContrastOutlineColor(styleColorOptions) : systemColor(CSSValueWebkitControlBackground, styleColorOptions);
#else
    const auto borderColor = systemColor(CSSValueWebkitControlBackground, styleColorOptions);
#endif

    drawShapeWithBorder(context, deviceScaleFactor, path, boundingRect, backgroundColor, borderWidth, borderColor);

    if (controlIsFocusedWithOutlineStyleAutoForVectorBasedControls(box))
        drawFocusRingForPathForVectorBasedControls(box, paintInfo, boundingRect, path);

    return true;
}

bool RenderThemeCocoa::adjustColorWellStyleForVectorBasedControls(RenderStyle& style, const Element* element) const
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(style);
    UNUSED_PARAM(element);
    return false;
#else
    if (!formControlRefreshEnabled(element))
        return false;

    RenderTheme::adjustColorWellStyle(style, element);

    return true;
#endif
}

bool RenderThemeCocoa::paintColorWellForVectorBasedControls(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(box);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#else
    if (!formControlRefreshEnabled(box))
        return false;

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    const auto states = RenderTheme::singleton().extractControlStyleStatesForRenderer(box);
    const auto isEnabled = states.contains(ControlStyle::State::Enabled);

    const auto radius = std::min(rect.width(), rect.height()) / 2.f;
    const FloatRoundedRect boundingRoundedRect(rect, FloatRoundedRect::Radii(radius));

    auto backgroundColor = systemColor(CSSValueAppleSystemQuinaryLabel, box.styleColorOptions());

    if (!isEnabled)
        backgroundColor = backgroundColor.colorWithAlphaMultipliedBy(0.5f);

    context.fillRoundedRect(boundingRoundedRect, backgroundColor);

#if PLATFORM(MAC)
    if (Theme::singleton().userPrefersContrast()) {
        Path path;
        path.addRoundedRect(boundingRoundedRect);
        drawHighContrastOutline(context, path, box.styleColorOptions());
    }
#endif

    return true;
#endif
}

bool RenderThemeCocoa::paintColorWellSwatchForVectorBasedControls(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(box);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#else
    if (!formControlRefreshEnabled(box))
        return false;

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    auto inputValueIsOpaque = true;
    auto primarySwatchColor = box.style().backgroundColor().resolvedColor();
    auto secondarySwatchColor = primarySwatchColor;

    const auto borderWidth = box.style().usedZoom() * 0.5f;
    Color borderOverlayColor = SRGBA<uint8_t> { 0, 0, 0, 26 }; // alpha 0.1f

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(box.element()->shadowHost())) {
        if (input->alpha() && !input->valueAsColor().isOpaque()) {
            inputValueIsOpaque = false;
            primarySwatchColor = input->valueAsColor();
            secondarySwatchColor = blendSourceOver(Color::black, primarySwatchColor);
            primarySwatchColor = blendSourceOver(Color::white, primarySwatchColor);
        }

        if (input->isDisabledFormControl()) {
            primarySwatchColor = primarySwatchColor.colorWithAlphaMultipliedBy(0.5f);
            secondarySwatchColor = secondarySwatchColor.colorWithAlphaMultipliedBy(0.5f);
            borderOverlayColor = borderOverlayColor.colorWithAlphaMultipliedBy(0.5f);
        }
    }

    const auto radius = std::min(rect.width(), rect.height()) / 2.f;
    const FloatRoundedRect boundingRoundedRect(rect, FloatRoundedRect::Radii(radius));

    Path path;
    path.addRoundedRect(boundingRoundedRect);

    context.clipPath(path);
    context.setFillColor(primarySwatchColor);
    context.fillPath(path);

    if (!inputValueIsOpaque) {
        Path trianglePath;
        trianglePath.moveTo(rect.minXMaxYCorner());
        trianglePath.addLineTo(rect.maxXMinYCorner());
        trianglePath.addLineTo(rect.minXMinYCorner());
        trianglePath.addLineTo(rect.minXMaxYCorner());

        context.setFillColor(secondarySwatchColor);
        context.fillPath(trianglePath);
    }

    context.setStrokeColor(borderOverlayColor);
    context.setStrokeThickness(borderWidth * 2);
    context.strokePath(path);

    return true;
#endif
}

bool RenderThemeCocoa::adjustColorWellSwatchStyleForVectorBasedControls(RenderStyle&, const Element*) const
{
    return false;
}

static void applyPaddingIfNotExplicitlySet(RenderStyle& style, Style::PaddingBox paddingBox)
{
    if (!style.hasExplicitlySetPaddingLeft())
        style.setPaddingLeft(WTFMove(paddingBox.left()));
    if (!style.hasExplicitlySetPaddingTop())
        style.setPaddingTop(WTFMove(paddingBox.top()));
    if (!style.hasExplicitlySetPaddingRight())
        style.setPaddingRight(WTFMove(paddingBox.right()));
    if (!style.hasExplicitlySetPaddingBottom())
        style.setPaddingBottom(WTFMove(paddingBox.bottom()));
}

bool RenderThemeCocoa::adjustColorWellSwatchWrapperStyleForVectorBasedControls(RenderStyle& style, const Element* element) const
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(style);
    UNUSED_PARAM(element);
    return false;
#else
    if (!formControlRefreshEnabled(element))
        return false;

    applyPaddingIfNotExplicitlySet(style, Style::PaddingBox { 0_css_px });

    return true;
#endif
}

bool RenderThemeCocoa::adjustColorWellSwatchOverlayStyleForVectorBasedControls(RenderStyle& style, const Element* element) const
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(style);
    UNUSED_PARAM(element);
    return false;
#else
    if (!formControlRefreshEnabled(element))
        return false;

    style.setDisplay(DisplayType::None);

    return true;
#endif
}

bool RenderThemeCocoa::paintColorWellDecorationsForVectorBasedControls(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if PLATFORM(MAC)
    UNUSED_PARAM(box);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#else

    if (!formControlRefreshEnabled(box))
        return false;

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);
    context.clip(rect);

    const auto zoomFactor = box.style().zoom();
    const auto strokeThickness = 3.f * zoomFactor;

    constexpr std::array colorStops {
        DisplayP3<float> { 1.00, 0.00, 0.00, 1 },
        DisplayP3<float> { 0.98, 0.00, 1.00, 1 },
        DisplayP3<float> { 0.00, 0.64, 1.00, 1 },
        DisplayP3<float> { 0.26, 1.00, 0.00, 1 },
        DisplayP3<float> { 1.00, 0.96, 0.00, 1 },
        DisplayP3<float> { 1.00, 0.00, 0.00, 1 },
    };
    constexpr int numColorStops = std::size(colorStops);

    Ref gradient = Gradient::create(Gradient::ConicData { rect.center(), 0 }, { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied });
    for (int i = 0; i < numColorStops; ++i)
        gradient->addColorStop({ i * 1.0f / (numColorStops - 1), colorStops[i] });

    FloatRect strokeRect = rect;
    strokeRect.inflate(-strokeThickness / 2.0f);

    context.setStrokeThickness(strokeThickness);
    context.setStrokeStyle(StrokeStyle::SolidStroke);
    context.setStrokeGradient(WTFMove(gradient));

    context.translate(rect.center());
    context.rotate(piOverTwoFloat);
    context.translate(-rect.center());
    context.strokeEllipse(strokeRect);
    return true;
#endif
}

bool RenderThemeCocoa::adjustInnerSpinButtonStyleForVectorBasedControls(RenderStyle& style, const Element* element) const
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(style);
    UNUSED_PARAM(element);
    return false;
#else
    if (!formControlRefreshEnabled(element))
        return false;

    // FIXME: rdar://150914436 The width of the button should dynamically
    // change according to the height of the inner container.

    const auto logicalWidthEm = style.writingMode().isVertical() ? 1.5f : 1.f;
    Ref emSize = CSSPrimitiveValue::create(logicalWidthEm, CSSUnitType::CSS_EM);
    const auto pixelsPerEm = emSize->resolveAsLength<float>(conversionDataForStyle(style));

    style.setLogicalWidth(Style::PreferredSize::Fixed { pixelsPerEm });
    style.setLogicalHeight(CSS::Keyword::Auto { });
    style.setAlignSelf(CSS::Keyword::Stretch { });

    return true;
#endif
}

#if PLATFORM(MAC)

static PathWithSize chevronDownPath()
{
    Path path;
    path.moveTo({ 35.48, 38.029 });
    path.addBezierCurveTo({ 36.904, 38.029 }, { 38.125, 37.5 }, { 39.223, 36.361 });
    path.addLineTo({ 63.352, 11.987 });
    path.addBezierCurveTo({ 64.206, 11.092 }, { 64.695, 9.993 }, { 64.695, 8.691 });
    path.addBezierCurveTo({ 64.695, 6.046 }, { 62.579, 3.971 }, { 59.975, 3.971 });
    path.addBezierCurveTo({ 58.714, 3.971 }, { 57.493, 4.5 }, { 56.557, 5.436 });
    path.addLineTo({ 35.52, 26.839 });
    path.addLineTo({ 14.443, 5.436 });
    path.addBezierCurveTo({ 13.507, 4.5 }, { 12.327, 3.971 }, { 10.984, 3.971 });
    path.addBezierCurveTo({ 8.38, 3.971 }, { 6.305, 6.046 }, { 6.305, 8.691 });
    path.addBezierCurveTo({ 6.305, 9.993 }, { 6.753, 11.092 }, { 7.648, 11.987 });
    path.addLineTo({ 31.777, 36.36 });
    path.addBezierCurveTo({ 32.916, 37.499 }, { 34.096, 38.028 }, { 35.48, 38.028 });
    return { path, { 71.0f, 42.0f } };
}

static PathWithSize chevronDownSemiboldPath()
{
    Path path;
    path.moveTo({ 22.1604, 27.3589 });
    path.addBezierCurveTo({ 23.0746, 27.3527 }, { 23.8441, 26.9932 }, { 24.5391, 26.2716 });
    path.addLineTo({ 43.4553, 6.70364 });
    path.addBezierCurveTo({ 44.0252, 6.14001 }, { 44.3145, 5.45388 }, { 44.3145, 4.63994 });
    path.addBezierCurveTo({ 44.3145, 2.98722 }, { 43.0027, 1.67536 }, { 41.3615, 1.67536 });
    path.addBezierCurveTo({ 40.568, 1.67536 }, { 39.8055, 1.98867 }, { 39.2108, 2.58246 });
    path.addLineTo({ 21.0808, 21.413 });
    path.addLineTo({ 23.2781, 21.413 });
    path.addLineTo({ 5.0966, 2.58246 });
    path.addBezierCurveTo({ 4.51524, 2.00109 }, { 3.76611, 1.67536 }, { 2.95306, 1.67536 });
    path.addBezierCurveTo({ 1.30565, 1.67536 }, { 0, 2.98722 }, { 0, 4.63994 });
    path.addBezierCurveTo({ 0, 5.44767 }, { 0.308905, 6.1338 }, { 0.853002, 6.71696 });
    path.addLineTo({ 19.7816, 26.2849 });
    path.addBezierCurveTo({ 20.5094, 26.9994 }, { 21.2586, 27.3589 }, { 22.1604, 27.3589 });
    return { path, { 44.6856f, 27.3589f } };
}

#endif

static PathWithSize chevronDownBoldPath()
{
    Path path;
    path.moveTo({ 22.6367, 28.1836 });
    path.addBezierCurveTo({ 23.75, 28.1738 }, { 24.6582, 27.7539 }, { 25.5273, 26.8652 });
    path.addLineTo({ 44.248, 7.57812 });
    path.addBezierCurveTo({ 44.9316, 6.9043 }, { 45.2637, 6.09375 }, { 45.2637, 5.12695 });
    path.addBezierCurveTo({ 45.2637, 3.1543 }, { 43.6816, 1.57227 }, { 41.7383, 1.57227 });
    path.addBezierCurveTo({ 40.7812, 1.57227 }, { 39.873, 1.95312 }, { 39.1504, 2.68555 });
    path.addLineTo({ 21.6211, 20.8691 });
    path.addLineTo({ 23.7012, 20.8691 });
    path.addLineTo({ 6.11328, 2.68555 });
    path.addBezierCurveTo({ 5.40039, 1.97266 }, { 4.50195, 1.57227 }, { 3.52539, 1.57227 });
    path.addBezierCurveTo({ 1.57227, 1.57227 }, { 0, 3.1543 }, { 0, 5.12695 });
    path.addBezierCurveTo({ 0, 6.08398 }, { 0.351562, 6.89453 }, { 1.00586, 7.58789 });
    path.addLineTo({ 19.7461, 26.875 });
    path.addBezierCurveTo({ 20.6445, 27.7637 }, { 21.543, 28.1836 }, { 22.6367, 28.1836 });
    return { path, { 45.6348f, 28.1836 } };
}

static PathWithSize chevronDownHeavyPath()
{
    Path path;
    path.moveTo({ 23.3276, 29.3797 });
    path.addBezierCurveTo({ 24.7296, 29.3648 }, { 25.8389, 28.8572 }, { 26.9607, 27.7263 });
    path.addLineTo({ 45.3978, 8.84645 });
    path.addBezierCurveTo({ 46.2464, 8.01279 }, { 46.6403, 7.02179 }, { 46.6403, 5.8333 });
    path.addBezierCurveTo({ 46.6403, 3.39662 }, { 44.6664, 1.42275 }, { 42.2848, 1.42275 });
    path.addBezierCurveTo({ 41.0906, 1.42275 }, { 39.971, 1.90157 }, { 39.0627, 2.83507 });
    path.addLineTo({ 22.4048, 20.0803 });
    path.addLineTo({ 24.3147, 20.0803 });
    path.addLineTo({ 7.58784, 2.83507 });
    path.addBezierCurveTo({ 6.68419, 1.93141 }, { 5.5692, 1.42275 }, { 4.35547, 1.42275 });
    path.addBezierCurveTo({ 1.95895, 1.42275 }, { 0, 3.39662 }, { 0, 5.8333 });
    path.addBezierCurveTo({ 0, 7.00687 }, { 0.413432, 7.99787 }, { 1.22756, 8.85106 });
    path.addLineTo({ 19.6945, 27.7309 });
    path.addBezierCurveTo({ 20.8405, 28.8722 }, { 21.9554, 29.3797 }, { 23.3276, 29.3797 });
    return { path, { 47.0114f, 29.3797f } };
}

#if PLATFORM(MAC)

static PathWithSize spinButtonIndicatorPath(ControlSize controlSize)
{
    switch (controlSize) {
    case ControlSize::Micro:
    case ControlSize::Mini:
    case ControlSize::Small:
        return chevronDownBoldPath();
    case ControlSize::Medium:
    case ControlSize::Large:
    case ControlSize::ExtraLarge:
        return chevronDownSemiboldPath();
    }
}

static float spinButtonDividerWidthRatioForControlSize(ControlSize controlSize)
{
    switch (controlSize) {
    case ControlSize::Micro:
    case ControlSize::Mini:
        return 9.f / 13.f;
    case ControlSize::Small:
        return 11.f / 17.f;
    case ControlSize::Medium:
        return 14.f / 20.f;
    case ControlSize::Large:
        return 15.f / 23.f;
    case ControlSize::ExtraLarge:
        return 20.f / 30.f;
    }
}

static ControlSize spinButtonControlSizeForHeight(float size)
{
    if (size < 16)
        return ControlSize::Micro;
    if (size < 20)
        return ControlSize::Mini;
    if (size < 24)
        return ControlSize::Small;
    if (size < 28)
        return ControlSize::Medium;
    if (size < 36)
        return ControlSize::Large;

    return ControlSize::ExtraLarge;
}

static float spinButtonIndicatorWidthRatio(ControlSize controlSize)
{
    switch (controlSize) {
    case ControlSize::Micro:
    case ControlSize::Mini:
        return 30.f / 52.f;
    case ControlSize::Small:
        return 36.f / 68.f;
    case ControlSize::Medium:
        return 46.f / 80.f;
    case ControlSize::Large:
        return 36.f / 73.f;
    case ControlSize::ExtraLarge:
        return 27.f / 72.f;
    }
}

static FloatRect spinButtonRectForContentRect(const RenderElement& box, const FloatRect& contentRect)
{
    const auto isHorizontal = !box.writingMode().isVertical();
    const auto isInlineFlipped = box.writingMode().isInlineFlipped();

    // FIXME: rdar://150914436 The width to height ratio of the button needs to
    // dynamically change according to the inner container height. This ratio
    // is correct for mini spin buttons, but the ratio should change slightly
    // for other control sizes.
    static constexpr FloatSize spinButtonSize = { 13.f, 16.f };

    // Find the size of the largest spin button we can fit inside the content rect
    // while preserving the aspect ratio.
    auto paintRect = contentRect;
    const auto boundingRatio = std::min(contentRect.width() / spinButtonSize.width(), contentRect.height() / spinButtonSize.height());
    paintRect.setSize(spinButtonSize * boundingRatio);

    // Push the rect towards the inline-end and center it along the block axis.
    const auto logicalRect = isHorizontal ? contentRect : contentRect.transposedRect();
    const float rectInlineStart = logicalRect.x();
    const float rectInlineEnd = rectInlineStart + logicalRect.width();
    const float rectBlockStart = logicalRect.y();
    const float rectBlockSize = logicalRect.height();

    const auto logicalPaintRect = isHorizontal ? paintRect : paintRect.transposedRect();
    const float paintRectInlineSize = logicalPaintRect.width();
    const float paintRectBlockSize = logicalPaintRect.height();

    const auto inlinePosition = isInlineFlipped ? rectInlineStart : rectInlineEnd - paintRectInlineSize;
    const auto blockPosition = rectBlockStart + rectBlockSize / 2.f - paintRectBlockSize / 2.f;

    auto location = FloatPoint { inlinePosition, blockPosition };
    if (!isHorizontal)
        location = location.transposedPoint();

    paintRect.setLocation(location);

    return paintRect;
}

#endif

bool RenderThemeCocoa::paintInnerSpinButtonForVectorBasedControls(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(box);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#else
    if (!formControlRefreshEnabled(box))
        return false;

    CheckedPtr renderBox = dynamicDowncast<RenderBox>(box);
    if (!renderBox)
        return true;

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    CheckedRef style = box.style();
    const auto usedZoom = style->usedZoom();
    const auto styleColorOptions = box.styleColorOptions();

    const auto controlStates = extractControlStyleStatesForRenderer(box);
    const auto isEnabled = controlStates.contains(ControlStyle::State::Enabled);
    const auto isPressed = controlStates.contains(ControlStyle::State::Pressed);
    const auto isSpinningUp = controlStates.contains(ControlStyle::State::SpinUp);
    const auto isWindowActive = controlStates.contains(ControlStyle::State::WindowActive);

    const auto cssValueForIndicatorColor = isWindowActive ? CSSValueAppleSystemLabel : CSSValueAppleSystemSecondaryLabel;

    auto backgroundColor = systemColor(CSSValueAppleSystemQuinaryLabel, styleColorOptions);
    auto dividerColor = systemColor(CSSValueAppleSystemQuaternaryLabel, styleColorOptions);
    auto indicatorColor = systemColor(cssValueForIndicatorColor, styleColorOptions);

    if (!isEnabled) {
        backgroundColor = backgroundColor.colorWithAlphaMultipliedBy(kDisabledControlAlpha);
        dividerColor = dividerColor.colorWithAlphaMultipliedBy(kDisabledControlAlpha);
        indicatorColor = indicatorColor.colorWithAlphaMultipliedBy(kDisabledControlAlpha);
    }

    // Our paint rect includes the content, padding, and border. Find
    // the rect for just the content so that we don't paint into the
    // padding area. We can't use RenderBox::absoluteContentBoxRect()
    // here because it is inaccurate when transforms are applied.
    auto contentRect = rect;
    const auto contentBox = renderBox->contentBoxRect();
    contentRect.moveBy(contentBox.location());
    contentRect.setSize(contentBox.size());

    // Now that we have our content rect, get the rect for the largest
    // spin button we can fit in it.
    auto paintRect = spinButtonRectForContentRect(box, contentRect);
    auto cornerRadius = 0.25f * paintRect.height();

    Path path;
    path.addContinuousRoundedRect(paintRect, cornerRadius, cornerRadius);
    context.setFillColor(backgroundColor);
    context.clipPath(path);
    context.fillPath(path);

#if PLATFORM(MAC)
    const auto userPrefersContrast = Theme::singleton().userPrefersContrast();

    if (userPrefersContrast)
        drawHighContrastOutline(context, path, styleColorOptions);
#endif

    const auto controlSize = spinButtonControlSizeForHeight(paintRect.height() / usedZoom);
    const auto centerDividerWidth = spinButtonDividerWidthRatioForControlSize(controlSize) * paintRect.width();
    const auto centerDividerHeight = usedZoom;

    const auto centerDividerRect = FloatRect { paintRect.center().x() - centerDividerWidth / 2.f, paintRect.center().y() - centerDividerHeight / 2.f, centerDividerWidth, centerDividerHeight };

    FloatRoundedRect roundedDividerRect(centerDividerRect, FloatRoundedRect::Radii(centerDividerRect.height() / 2.f));

#if PLATFORM(MAC)
    if (userPrefersContrast)
        dividerColor = highContrastOutlineColor(styleColorOptions);
#endif
    context.setFillColor(dividerColor);
    context.fillRoundedRect(roundedDividerRect, dividerColor);

    auto spinUpColor = indicatorColor;
    auto spinDownColor = indicatorColor;

    auto singleButtonRect = FloatRect { paintRect.x(), paintRect.center().y(), paintRect.width(), paintRect.height() / 2.f };

    if (isPressed && isEnabled) {
        if (isSpinningUp) {
            singleButtonRect.setY(paintRect.y());
            spinUpColor = platformAdjustedColorForPressedState(indicatorColor, styleColorOptions);
        } else
            spinDownColor = platformAdjustedColorForPressedState(indicatorColor, styleColorOptions);

        context.save();
        context.clip(singleButtonRect);
        context.fillPath(path);
        context.restore();
    }

    auto chevronPath = spinButtonIndicatorPath(controlSize);
    const auto scale = spinButtonIndicatorWidthRatio(controlSize) * paintRect.width() / chevronPath.originalSize.width();

    singleButtonRect.setY(paintRect.center().y());

    context.save();
    context.translate(singleButtonRect.center() - (chevronPath.originalSize * scale * 0.5f));
    context.scale(scale);
    context.setFillColor(spinDownColor);
    context.fillPath(chevronPath.path);
    context.restore();

    AffineTransform transform;
    transform.translate(0, chevronPath.originalSize.height());
    transform.scale(1, -1);
    chevronPath.path.transform(transform);

    singleButtonRect.setY(paintRect.y());

    context.translate(singleButtonRect.center() - (chevronPath.originalSize * scale * 0.5f));
    context.scale(scale);
    context.setFillColor(spinUpColor);
    context.fillPath(chevronPath.path);

    return true;
#endif
}

static void applyEmPadding(RenderStyle& style, const Element* element, float paddingInlineEm, float paddingBlockEm)
{
    if (!element)
        return;

    Ref paddingInline = CSSPrimitiveValue::create(paddingInlineEm, CSSUnitType::CSS_EM);
    Ref paddingBlock = CSSPrimitiveValue::create(paddingBlockEm, CSSUnitType::CSS_EM);

    Ref document = element->document();

    auto usedZoom = style.usedZoomForLength().value;
    const auto paddingInlinePixels = Style::PaddingEdge::Fixed { static_cast<float>(paddingInline->resolveAsLength<int>({ style, document->renderStyle(), nullptr, document->renderView() })) / usedZoom };
    const auto paddingBlockPixels = Style::PaddingEdge::Fixed { static_cast<float>(paddingBlock->resolveAsLength<int>({ style, document->renderStyle(), nullptr, document->renderView() }))  / usedZoom };

    const auto isVertical = !style.writingMode().isHorizontal();
    const auto horizontalPadding = isVertical ? paddingBlockPixels : paddingInlinePixels;
    const auto verticalPadding = isVertical ? paddingInlinePixels : paddingBlockPixels;

    Style::PaddingBox paddingBox { verticalPadding, horizontalPadding, verticalPadding, horizontalPadding };
    applyPaddingIfNotExplicitlySet(style, paddingBox);
}

static constexpr auto standardTextControlInlinePaddingEm = 0.5f;
static constexpr auto standardTextControlBlockPaddingEm = 0.25f;

#if PLATFORM(MAC)
static Style::PaddingBox paddingBoxForNumberField(const RenderStyle& style, const Element* element)
{
    Ref paddingInlineStart = CSSPrimitiveValue::create(standardTextControlInlinePaddingEm, CSSUnitType::CSS_EM);
    Ref paddingInlineEndAndBlock = CSSPrimitiveValue::create(standardTextControlBlockPaddingEm, CSSUnitType::CSS_EM);
    Ref document = element->document();

    auto usedZoom = style.usedZoomForLength().value;
    const auto paddingInlineStartPixels = Style::PaddingEdge::Fixed { static_cast<float>(paddingInlineStart->resolveAsLength<int>({ style, document->renderStyle(), nullptr, document->renderView() })) / usedZoom };
    const auto paddingInlineEndAndBlockPixels = Style::PaddingEdge::Fixed { static_cast<float>(paddingInlineEndAndBlock->resolveAsLength<int>({ style, document->renderStyle(), nullptr, document->renderView() })) / usedZoom };

    Style::PaddingBox paddingBox { paddingInlineEndAndBlockPixels };
    paddingBox.setStart(paddingInlineStartPixels, style.writingMode());

    return paddingBox;
}

static void applyEmPaddingForNumberField(RenderStyle& style, const Element* element)
{
    if (!element)
        return;

    applyPaddingIfNotExplicitlySet(style, paddingBoxForNumberField(style, element));
}
#endif

bool RenderThemeCocoa::adjustTextFieldStyleForVectorBasedControls(RenderStyle& style, const Element* element) const
{
    // FIXME: In vertical writing mode, the text should be inset more from the block-start.

    if (!formControlRefreshEnabled(element))
        return false;

#if PLATFORM(IOS_FAMILY)
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(*element); input && input->hasDataList())
        applyPaddingIfNotExplicitlySet(style, { 1_css_px / style.usedZoomForLength().value });
    else
        applyEmPadding(style, element, standardTextControlInlinePaddingEm, standardTextControlBlockPaddingEm);
#else
    // If the input has a datalist, we'll apply padding directly to the inner container and
    // inner text elements so that the list button is not subjected to increased padding.
    // This is done to ensure a correct default height and to ensure concentricity by default.
    // Number inputs with datalists will have their list button at the inline-end, which means
    // concentricity will be determined by the list button rather than the spin button.
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(*element); input && !input->hasDataList()) {
        if (input->isNumberField())
            applyEmPaddingForNumberField(style, element);
        else
            applyEmPadding(style, element, standardTextControlInlinePaddingEm, standardTextControlBlockPaddingEm);
    }
#endif

    return true;
}

static ControlSize listButtonControlSizeForBlockSize(float size)
{
    if (size < 9)
        return ControlSize::Micro;
    if (size < 13)
        return ControlSize::Mini;
    if (size < 17)
        return ControlSize::Small;
    if (size < 19)
        return ControlSize::Medium;
    if (size < 25)
        return ControlSize::Large;

    return ControlSize::ExtraLarge;
}

static FloatSize listButtonIndicatorSize(ControlSize controlSize)
{
    switch (controlSize) {
    case ControlSize::Micro:
    case ControlSize::Mini:
        return { 8.5f, 5.f };
    case ControlSize::Small:
    case ControlSize::Medium:
        return { 9.5f, 5.5f };
    case ControlSize::Large:
    case ControlSize::ExtraLarge:
        return { 11.f, 7.f };
    }
}

#if PLATFORM(MAC)
static float listButtonCornerRadius(ControlSize controlSize)
{
    switch (controlSize) {
    case ControlSize::Micro:
        return 0.f;
    case ControlSize::Mini:
        return 2.5f;
    case ControlSize::Small:
        return 3.5f;
    case ControlSize::Medium:
        return 4.5f;
    case ControlSize::Large:
        return 5.5f;
    case ControlSize::ExtraLarge:
        return 6.5f;
    }
}
#endif

static constexpr auto borderThicknessForTextBasedControl = 0.5f;

#if PLATFORM(MAC)
static float cornerRadiusForConcentricTextBasedControl(const RenderElement& box, const FloatRect& rect)
{
    CheckedPtr renderBox = dynamicDowncast<RenderBox>(box);
    if (!renderBox)
        return defaultCornerRadiusForTextBasedControls;

    CheckedRef style = box.style();
    const auto usedZoom = style->usedZoom();

    auto hasSpinButton = false;
    auto hasListButton = false;
    FloatRect innerButtonContentRect;

    // We don't have the paint rect for our inner button, so we cannot determine
    // concentricity by comparing paint rects. Instead, we'll compare the absolute
    // content box rect for the inner button with our own absolute padding box.
    //
    // The input's border widths will have no impact on whether the control can be
    // concentric or not because the UA style sets it to an equal width on all sides.
    // The UA border width is guaranteed at this point; if an author set a different
    // value, the control would have lost native appearance.

    if (CheckedPtr grandchild = renderBox->lastChildBox(); grandchild && (grandchild = grandchild->lastChildBox())) {
        StyleAppearance innerButtonAppearance = grandchild->style().usedAppearance();
        hasSpinButton = innerButtonAppearance == StyleAppearance::InnerSpinButton;
        hasListButton = innerButtonAppearance == StyleAppearance::ListButton;

        if (hasSpinButton || hasListButton) {
            innerButtonContentRect = grandchild->contentBoxRect();
            // During painting, list buttons use all of their content box, but spin buttons
            // maintain a specific aspect-ratio and thus may leave some of the box unused. If
            // the inner button is a spin button, find the rect for the painted spin button.
            if (hasSpinButton)
                innerButtonContentRect = spinButtonRectForContentRect(box, innerButtonContentRect);
            const auto absPos = grandchild->localToAbsolute();
            innerButtonContentRect.move(absPos.x(), absPos.y());
        } else
            return defaultCornerRadiusForTextBasedControls * usedZoom;
    } else
        return defaultCornerRadiusForTextBasedControls * usedZoom;

    const auto isInlineFlipped = style->writingMode().isInlineFlipped();
    const auto isVertical = style->writingMode().isVertical();

    const auto leftPadding = renderBox->paddingLeft().toFloat();
    const auto rightPadding = renderBox->paddingRight().toFloat();
    const auto topPadding = renderBox->paddingTop().toFloat();
    const auto bottomPadding = renderBox->paddingBottom().toFloat();

    auto controlPaddingBox = renderBox->contentBoxRect();
    const auto absPos = renderBox->localToAbsolute();
    controlPaddingBox.move(absPos.x() - leftPadding, absPos.y() - topPadding);
    controlPaddingBox.setSize({ controlPaddingBox.width() + leftPadding + rightPadding, controlPaddingBox.height() + topPadding + bottomPadding });

    const auto topDistance = innerButtonContentRect.y() - controlPaddingBox.y();
    const auto bottomDistance = controlPaddingBox.maxXMaxYCorner().y() - innerButtonContentRect.maxXMaxYCorner().y();
    const auto leftDistance = innerButtonContentRect.x() - controlPaddingBox.x();
    const auto rightDistance = controlPaddingBox.maxXMaxYCorner().x() - innerButtonContentRect.maxXMaxYCorner().x();

    auto inlineDistance = 0.f;
    auto canBeConcentric = false;

    if (isVertical) {
        inlineDistance = isInlineFlipped ? topDistance : bottomDistance;
        canBeConcentric = WTF::areEssentiallyEqual(inlineDistance, leftDistance) && WTF::areEssentiallyEqual(inlineDistance, rightDistance);
    } else {
        inlineDistance = isInlineFlipped ? leftDistance : rightDistance;
        canBeConcentric = WTF::areEssentiallyEqual(inlineDistance, topDistance) && WTF::areEssentiallyEqual(inlineDistance, topDistance);
    }

    if (canBeConcentric) {
        // So far, we have the distance from the outer edge of the painted inner
        // button to the inner edge of the input's CSS border. Use this to determine
        // the distance between the button's outer edge to the inner edge of the
        // *painted* border.
        const auto cssBorderThickness = renderBox->borderTop().toFloat();
        inlineDistance += cssBorderThickness - borderThicknessForTextBasedControl * usedZoom;

        ControlSize innerButtonControlSize = ControlSize::Micro;
        float innerButtonCornerRadius = 0.f;

        if (hasListButton) {
            const auto listButtonBlockLength = isVertical ? innerButtonContentRect.width() : innerButtonContentRect.height();

            innerButtonControlSize = listButtonControlSizeForBlockSize(listButtonBlockLength / usedZoom);
            innerButtonCornerRadius = listButtonCornerRadius(innerButtonControlSize) * usedZoom;
        } else
            innerButtonCornerRadius = innerButtonContentRect.height() * 0.25f;

        const auto minRectLength = std::min(rect.width(), rect.height());
        auto candidateRadius = std::max(innerButtonCornerRadius + inlineDistance, 0.f);

        if (candidateRadius <= 0.4f * minRectLength)
            return candidateRadius;

        // The calculated radius would result in a very round control. Allow the
        // radius only if it wouldn't cause the control shape to near a circle.
        const auto inlineLength = isVertical ? rect.height() : rect.width();
        if (inlineLength >= candidateRadius * 3.f)
            return std::min(candidateRadius, minRectLength * 0.5f);
    }

    return defaultCornerRadiusForTextBasedControls * usedZoom;
}
#endif

#if PLATFORM(VISION)
static void paintTextAreaOrTextFieldInnerShadow(GraphicsContext& context, const FloatRect& rect, float cornerRadius)
{
    GraphicsContextStateSaver stateSaver(context);

    Path path;
    path.addContinuousRoundedRect(rect, cornerRadius, cornerRadius);

    context.setFillColor(Color::black.colorWithAlphaByte(10));
    context.drawPath(path);
    context.clipPath(path);

    constexpr auto innerShadowOffsetHeight = 5.f;
    constexpr auto innerShadowBlur = 10.0f;
    const auto innerShadowColor = DisplayP3<float> { 0, 0, 0, 0.04f };
    context.setDropShadow({ { 0, innerShadowOffsetHeight }, innerShadowBlur, innerShadowColor, ShadowRadiusMode::Default });
    context.setFillColor(Color::black);

    Path innerShadowPath;
    FloatRect innerShadowRect = rect;
    innerShadowRect.inflate(innerShadowOffsetHeight + innerShadowBlur);
    innerShadowPath.addRect(innerShadowRect);

    FloatRect innerShadowHoleRect = rect;
    // FIXME: This is not from the spec; but without it we get antialiasing fringe from the fill; we need a better solution.
    innerShadowHoleRect.inflate(0.5);
    innerShadowPath.addContinuousRoundedRect(innerShadowHoleRect, cornerRadius, cornerRadius);

    context.setFillRule(WindRule::EvenOdd);
    context.fillPath(innerShadowPath);
}
#endif

static bool paintTextAreaOrTextField(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    if (!formControlRefreshEnabled(box))
        return false;

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    CheckedRef style = box.style();
    const auto usedZoom = style->usedZoom();

#if PLATFORM(MAC)
    RefPtr input = dynamicDowncast<HTMLInputElement>(box.element());
    const auto hasInnerButton = input && (input->hasDataList() || input->isNumberField());
    const auto cornerRadius = hasInnerButton ? cornerRadiusForConcentricTextBasedControl(box, rect) : defaultCornerRadiusForTextBasedControls * usedZoom;
#else
    const auto cornerRadius = defaultCornerRadiusForTextBasedControls * usedZoom;
#endif

#if PLATFORM(VISION)
    if (PAL::currentUserInterfaceIdiomIsVision()) {
        paintTextAreaOrTextFieldInnerShadow(context, rect, cornerRadius);
        return true;
    }
#endif

    const auto styleColorOptions = box.styleColorOptions();
    auto backgroundColor = style->visitedDependentColor(CSSPropertyBackgroundColor);
#if PLATFORM(MAC)
    const auto prefersContrast = Theme::singleton().userPrefersContrast();
    auto borderColor = prefersContrast ? highContrastOutlineColor(styleColorOptions) : RenderTheme::singleton().systemColor(CSSValueAppleSystemContainerBorder, styleColorOptions);
#else
    auto borderColor = RenderTheme::singleton().systemColor(CSSValueAppleSystemContainerBorder, styleColorOptions);
#endif

    const auto states = RenderTheme::singleton().extractControlStyleStatesForRenderer(box);
    if (!states.contains(ControlStyle::State::Enabled)) {
        borderColor = borderColor.colorWithAlphaMultipliedBy(0.5f);
        backgroundColor = backgroundColor.colorWithAlphaMultipliedBy(0.5f);
    }

    Path path;
    path.addContinuousRoundedRect(rect, cornerRadius, cornerRadius);

    const auto deviceScaleFactor = box.document().deviceScaleFactor();
    drawShapeWithBorder(context, deviceScaleFactor, path, rect, backgroundColor, borderThicknessForTextBasedControl * usedZoom, borderColor);

    if (controlIsFocusedWithOutlineStyleAutoForVectorBasedControls(box))
        drawFocusRingForPathForVectorBasedControls(box, paintInfo, rect, path);

    return true;
}

bool RenderThemeCocoa::paintTextFieldForVectorBasedControls(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    return paintTextAreaOrTextField(box, paintInfo, rect);
}

bool RenderThemeCocoa::paintTextFieldDecorationsForVectorBasedControls(const RenderBox&, const PaintInfo&, const FloatRect&)
{
    return false;
}

bool RenderThemeCocoa::adjustTextAreaStyleForVectorBasedControls(RenderStyle& style, const Element* element) const
{
    if (!formControlRefreshEnabled(element))
        return false;

    applyEmPadding(style, element, standardTextControlInlinePaddingEm, standardTextControlBlockPaddingEm);

    return true;
}

bool RenderThemeCocoa::paintTextAreaForVectorBasedControls(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    return paintTextAreaOrTextField(box, paintInfo, rect);
}

bool RenderThemeCocoa::paintTextAreaDecorationsForVectorBasedControls(const RenderBox&, const PaintInfo&, const FloatRect&)
{
    return false;
}

#if !PLATFORM(MAC)

static void applyCommonButtonPaddingToStyleForVectorBasedControls(RenderStyle& style, const Element& element)
{
    Document& document = element.document();
    Ref emSize = CSSPrimitiveValue::create(0.5, CSSUnitType::CSS_EM);
    // We don't need this element's parent style to calculate `em` units, so it's okay to pass nullptr for it here.
    auto pixels = Style::PaddingEdge::Fixed { static_cast<float>(emSize->resolveAsLength<int>({ style, document.renderStyle(), nullptr, document.renderView() })) / style.usedZoomForLength().value };

    auto paddingBox = Style::PaddingBox { 0_css_px, pixels, 0_css_px, pixels };
    if (!style.writingMode().isHorizontal())
        paddingBox = { paddingBox.left(), paddingBox.top(), paddingBox.right(), paddingBox.bottom() };

    applyPaddingIfNotExplicitlySet(style, paddingBox);
}

// FIXME: This is a copy of RenderThemeMeasureTextClient from RenderThemeIOS. Refactor to remove duplicate code.
class RenderThemeMeasureTextClientForVectorBasedControls : public MeasureTextClient {
public:
    RenderThemeMeasureTextClientForVectorBasedControls(const FontCascade& font, const RenderStyle& style)
        : m_font(font)
        , m_style(style)
    {
    }
    float measureText(const String& string) const override
    {
        TextRun run = RenderBlock::constructTextRun(string, m_style);
        return m_font.width(run);
    }
private:
    const FontCascade& m_font;
    const RenderStyle& m_style;
};

static void adjustInputElementButtonStyleForVectorBasedControls(RenderStyle& style, const HTMLInputElement& inputElement)
{
    // FIXME: This is a copy of adjustInputElementButtonStyle(...) from RenderThemeIOS. Refactor to remove duplicate code.

    applyCommonButtonPaddingToStyleForVectorBasedControls(style, inputElement);

    // Don't adjust the style if the width is specified.
    if (auto fixedLogicalWidth = style.logicalWidth().tryFixed(); fixedLogicalWidth && fixedLogicalWidth->isPositive())
        return;

    // Don't adjust for unsupported date input types.
    DateComponentsType dateType = inputElement.dateType();
    if (dateType == DateComponentsType::Invalid)
        return;

#if !ENABLE(INPUT_TYPE_WEEK_PICKER)
    if (dateType == DateComponentsType::Week)
        return;
#endif

    // Enforce the width and set the box-sizing to content-box to not conflict with the padding.
    FontCascade font = style.fontCascade();

    float estimatedMaximumWidth = localizedDateCache().estimatedMaximumWidthForDateType(dateType, font, RenderThemeMeasureTextClientForVectorBasedControls(font, style));

    ASSERT(estimatedMaximumWidth >= 0);

    if (estimatedMaximumWidth > 0) {
        style.setLogicalMinWidth(Style::MinimumSize::Fixed { std::ceil(estimatedMaximumWidth) / style.usedZoomForLength().value });
        style.setBoxSizing(BoxSizing::ContentBox);
    }
}

#endif

static void adjustSelectListButtonStyleForVectorBasedControls(RenderStyle& style, const Element& element)
{
    // FIXME: This is a copy of adjustSelectListButtonStyle(...) from RenderThemeIOS. Refactor to remove duplicate code.
#if PLATFORM(IOS_FAMILY)
    applyCommonButtonPaddingToStyleForVectorBasedControls(style, element);
#else
    UNUSED_PARAM(element);
#endif
    style.setLineHeight(CSS::Keyword::Normal { });
}

bool RenderThemeCocoa::adjustMenuListStyleForVectorBasedControls(RenderStyle& style, const Element* element) const
{
    if (!formControlRefreshEnabled(element))
        return false;

    RenderTheme::adjustMenuListStyle(style, element);

    if (!style.hasExplicitlySetColor()) {
        const auto styleColorOptions = element->document().styleColorOptions(&style);
        style.setColor(buttonTextColor(styleColorOptions, !element->isDisabledFormControl()));
    }

    style.setWhiteSpaceCollapse(WhiteSpaceCollapse::Preserve);
    style.setTextWrapMode(TextWrapMode::NoWrap);
    style.setBoxShadow(CSS::Keyword::None  { });

    // Enforce "line-height: normal" as long as this element isn't a non-select element using `-webkit-appearance: menulist`.
    if (element && is<HTMLSelectElement>(*element))
        style.setLineHeight(CSS::Keyword::Normal { });

    return true;
}

bool RenderThemeCocoa::paintMenuListForVectorBasedControls(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(box);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#else
    if (!formControlRefreshEnabled(box))
        return false;

    paintButtonForVectorBasedControls(box, paintInfo, rect);
    paintMenuListButtonDecorationsForVectorBasedControls(box, paintInfo, rect);
    return true;
#endif
}

bool RenderThemeCocoa::paintMenuListDecorationsForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&)
{
    return false;
}

bool RenderThemeCocoa::paintMenuListButtonForVectorBasedControls(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if PLATFORM(MAC)
    UNUSED_PARAM(box);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#else

    if (!formControlRefreshEnabled(box))
        return false;

    // Unlike most other controls, a select or select-multiple control that has devolved will
    // not have its used appearance set to none, resulting in this paint method still being called.
    // This is done in order to retain the control's decorations. Return false in this scenario to
    // indicate that we need the CSS background and border painted, still.
    if (box.style().nativeAppearanceDisabled())
        return false;

    paintButtonForVectorBasedControls(box, paintInfo, rect);
    paintMenuListButtonDecorationsForVectorBasedControls(box, paintInfo, rect);

    return true;

#endif
}

Color RenderThemeCocoa::buttonTextColor(OptionSet<StyleColorOptions> options, bool enabled) const
{
    const auto cssValue = enabled ? CSSValueAppleSystemLabel : CSSValueAppleSystemTertiaryLabel;
    return systemColor(cssValue, options);
}

bool RenderThemeCocoa::adjustButtonStyleForVectorBasedControls(RenderStyle& style, const Element* element) const
{
    if (!formControlRefreshEnabled(element))
        return false;

#if PLATFORM(MAC)
    RenderTheme::adjustButtonStyle(style, element);
#endif

    auto isEnabled = true;
    if (RefPtr input = dynamicDowncast<HTMLFormControlElement>(element))
        isEnabled = !input->isDisabledFormControl();

    const auto styleColorOptions = element->document().styleColorOptions(&style);

    auto adjustStyleForSubmitButton = [&] {
        style.setInsideSubmitButton(true);
#if PLATFORM(MAC)
        style.setColor(buttonTextColor(styleColorOptions, isEnabled));
#endif
    };

    if (!style.hasExplicitlySetColor()) {
        if (isSubmitStyleButton(element))
            adjustStyleForSubmitButton();
        else
            style.setColor(buttonTextColor(styleColorOptions, isEnabled));
    }

#if PLATFORM(IOS_FAMILY)
    constexpr auto controlBaseHeight = 20.0f;
    constexpr auto controlBaseFontSize = 11.0f;

    if (!style.logicalWidth().isSpecified() || style.logicalHeight().isAuto()) {
        auto minimumHeight = controlBaseHeight / controlBaseFontSize * style.fontDescription().computedSizeForRangeZoomOption(CSS::RangeZoomOptions::Unzoomed);
        if (auto fixedValue = style.logicalMinHeight().tryFixed())
            minimumHeight = std::max(minimumHeight, fixedValue->resolveZoom(Style::ZoomFactor { 1.0f, 1.0f }));
        // FIXME: This may need to be a layout time adjustment to support various
        // values like fit-content etc.
        style.setLogicalMinHeight(Style::MinimumSize::Fixed { minimumHeight });
    }

    if (style.usedAppearance() == StyleAppearance::ColorWell)
        return true;

    Ref emSize = CSSPrimitiveValue::create(1.0, CSSUnitType::CSS_EM);
    auto pixels = Style::PaddingEdge::Fixed { static_cast<float>(emSize->resolveAsLength<int>({ style, nullptr, nullptr, nullptr })) / style.usedZoom() };

    auto paddingBox = Style::PaddingBox { 0_css_px, pixels, 0_css_px, pixels };
#else
    auto usedZoom = style.usedZoomForLength().value;
    auto paddingBox = Style::PaddingBox { 0_css_px, 6_css_px / usedZoom, 1_css_px / usedZoom, 6_css_px / usedZoom };
#endif
    if (!style.writingMode().isHorizontal())
        paddingBox = { paddingBox.left(), paddingBox.top(), paddingBox.right(), paddingBox.bottom() };

    applyPaddingIfNotExplicitlySet(style, paddingBox);

    return true;
}

bool RenderThemeCocoa::adjustMenuListButtonStyleForVectorBasedControls(RenderStyle& style, const Element* element) const
{
    if (!formControlRefreshEnabled(element))
        return false;

    if (!style.hasExplicitlySetColor()) {
        const auto styleColorOptions = element->document().styleColorOptions(&style);
        style.setColor(buttonTextColor(styleColorOptions, !element->isDisabledFormControl()));
    }

    const auto isNonMultipleSelectElement = is<HTMLSelectElement>(*element) && !element->hasAttributeWithoutSynchronization(HTMLNames::multipleAttr);
    if (isNonMultipleSelectElement)
        adjustSelectListButtonStyleForVectorBasedControls(style, *element);

#if PLATFORM(IOS_FAMILY)
    const int menuListMinHeight = 15;
    const float menuListBaseHeight = 20;
    const float menuListBaseFontSize = 11;

    if (style.logicalHeight().isAuto())
        style.setLogicalMinHeight(Style::MinimumSize::Fixed { static_cast<float>(std::max(menuListMinHeight, static_cast<int>(menuListBaseHeight / menuListBaseFontSize * style.fontDescription().computedSizeForRangeZoomOption(CSS::RangeZoomOptions::Unzoomed)))) });
    else
        style.setLogicalMinHeight(Style::MinimumSize::Fixed { static_cast<float>(menuListMinHeight) });

    if (isNonMultipleSelectElement)
        return true;

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(*element))
        adjustInputElementButtonStyleForVectorBasedControls(style, *input);
#endif

    return true;
}

bool RenderThemeCocoa::paintMenuListButtonDecorationsForVectorBasedControls(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    if (!formControlRefreshEnabled(box))
        return false;

    RefPtr element = box.element();

    if (!element || is<HTMLInputElement>(element))
        return true;

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    CheckedRef style = box.style();
    auto styleColorOptions = box.styleColorOptions();

    Path glyphPath;
    FloatSize glyphSize;

    if (box.isRenderMenuList() && downcast<HTMLSelectElement>(element)->multiple()) {
        constexpr int length = 18;
        constexpr int count = 3;
        constexpr int padding = 12;

        FloatRect ellipse(0, 0, length, length);

        for (int i = 0; i < count; ++i) {
            glyphPath.addEllipseInRect(ellipse);
            ellipse.move(length + padding, 0);
        }

        glyphSize = { length * count + padding * (count - 1), length };
    } else {
        // SF symbols 6: chevron.up.chevron.down / semibold / 20pt.
        constexpr float glyphWidth = 14.4618f;
        constexpr float glyphHeight = 20.1723f;
        glyphSize = { glyphWidth, glyphHeight };
        glyphPath.moveTo({ 7.05356f, 0.0f });
        glyphPath.addBezierCurveTo({ 6.6506f, 0.0f }, { 6.31419f, 0.159764f }, { 5.98845f, 0.476187f });
        glyphPath.addLineTo({ 0.449547f, 5.79359f });
        glyphPath.addBezierCurveTo({ 0.188162f, 6.04876f }, { 0.0f, 6.37185f }, { 0.0f, 6.75753f });
        glyphPath.addBezierCurveTo({ 0.0f, 7.5262f }, { 0.619975f, 8.0783f }, { 1.31101f, 8.0783f });
        glyphPath.addBezierCurveTo({ 1.64075f, 8.0783f }, { 1.99003f, 7.97934f }, { 2.27895f, 7.69662f });
        glyphPath.addLineTo({ 7.05356f, 3.02633f });
        glyphPath.addLineTo({ 11.8215f, 7.69662f });
        glyphPath.addBezierCurveTo({ 12.1171f, 7.97268f }, { 12.4597f, 8.0783f }, { 12.7895f, 8.0783f });
        glyphPath.addBezierCurveTo({ 13.4805f, 8.0783f }, { 14.1005f, 7.5262f }, { 14.1005f, 6.75753f });
        glyphPath.addBezierCurveTo({ 14.1005f, 6.37185f }, { 13.9159f, 6.04876f }, { 13.6509f, 5.79359f });
        glyphPath.addLineTo({ 8.11201f, 0.476187f });
        glyphPath.addBezierCurveTo({ 7.78627f, 0.159764f }, { 7.44987f, 0.0f }, { 7.05356f, 0.0f });
        glyphPath.moveTo({ 7.05356f, 20.1625f });
        glyphPath.addBezierCurveTo({ 7.44987f, 20.1625f }, { 7.78627f, 19.9961f }, { 8.11201f, 19.6833f });
        glyphPath.addLineTo({ 13.6509f, 14.3659f });
        glyphPath.addBezierCurveTo({ 13.9159f, 14.1071f }, { 14.1005f, 13.7907f }, { 14.1005f, 13.3984f });
        glyphPath.addBezierCurveTo({ 14.1005f, 12.6297f }, { 13.4805f, 12.0811f }, { 12.7895f, 12.0811f });
        glyphPath.addBezierCurveTo({ 12.4597f, 12.0811f }, { 12.1171f, 12.1899f }, { 11.8215f, 12.4659f });
        glyphPath.addLineTo({ 7.05356f, 17.1362f });
        glyphPath.addLineTo({ 2.27895f, 12.4659f });
        glyphPath.addBezierCurveTo({ 1.99003f, 12.1801f }, { 1.64075f, 12.0811f }, { 1.31101f, 12.0811f });
        glyphPath.addBezierCurveTo({ 0.619975f, 12.0811f }, { 0.0f, 12.6297f }, { 0.0f, 13.3984f });
        glyphPath.addBezierCurveTo({ 0.0f, 13.7907f }, { 0.188162f, 14.1071f }, { 0.449547f, 14.3659f });
        glyphPath.addLineTo({ 5.98845f, 19.6833f });
        glyphPath.addBezierCurveTo({ 6.31419f, 19.9961f }, { 6.6506f, 20.1625f }, { 7.05356f, 20.1625f });
    }

    Ref emSize = CSSPrimitiveValue::create(1.0, CSSUnitType::CSS_EM);
    const auto emPixels = emSize->resolveAsLength<float>({ style, nullptr, nullptr, nullptr });
    const auto glyphScale = 0.55f * emPixels / glyphSize.width();
    glyphSize = glyphScale * glyphSize;

    const bool isHorizontalWritingMode = style->writingMode().isHorizontal();
    const auto logicalRect = isHorizontalWritingMode ? rect : rect.transposedRect();

    FloatPoint glyphOrigin;
    glyphOrigin.setY(logicalRect.center().y() - glyphSize.height() / 2.0f);

    auto glyphPaddingEnd = logicalRect.width();
    auto usedZoom = style->usedZoomForLength();
    if (auto fixedPaddingEnd = style->paddingEnd().tryFixed())
        glyphPaddingEnd = fixedPaddingEnd->resolveZoom(usedZoom);

    // Add RenderMenuList inner start padding for symmetry.
    if (CheckedPtr menulist = dynamicDowncast<RenderMenuList>(box); menulist && menulist->innerRenderer()) {
        if (auto innerPaddingStart = menulist->innerRenderer()->style().paddingStart().tryFixed())
            glyphPaddingEnd += innerPaddingStart->resolveZoom(usedZoom);
    }

    if (!style->writingMode().isInlineFlipped())
        glyphOrigin.setX(logicalRect.maxX() - glyphSize.width() - Style::evaluate<float>(box.style().borderEndWidth(), box.style().usedZoomForLength()) - glyphPaddingEnd);
    else
        glyphOrigin.setX(logicalRect.x() + Style::evaluate<float>(box.style().borderEndWidth(), box.style().usedZoomForLength()) + glyphPaddingEnd);

    if (!isHorizontalWritingMode)
        glyphOrigin = glyphOrigin.transposedPoint();

    AffineTransform transform;
    transform.translate(glyphOrigin);
    transform.scale(glyphScale);
    glyphPath.transform(transform);

    const auto decorationColor = buttonTextColor(styleColorOptions, isEnabled(box));

    context.setFillColor(decorationColor);
    context.fillPath(glyphPath);
    return true;
}

bool RenderThemeCocoa::adjustMeterStyleForVectorBasedControls(RenderStyle&, const Element*) const
{
    return false;
}

static constexpr auto cssValueForProgressAndMeterTrackColor = CSSValueAppleSystemOpaqueFill;

bool RenderThemeCocoa::paintMeterForVectorBasedControls(const RenderElement& renderer, const PaintInfo& paintInfo, const FloatRect& rect)
{
    if (!formControlRefreshEnabled(renderer))
        return false;

    CheckedPtr renderMeter = dynamicDowncast<RenderMeter>(renderer);
    if (!renderMeter)
        return true;

    RefPtr element = renderMeter->meterElement();

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    float cornerRadius = std::min(rect.width(), rect.height()) / 2.0f;
    FloatRoundedRect roundedFillRect(rect, FloatRoundedRect::Radii(cornerRadius));

    auto styleColorOptions = renderer.styleColorOptions();
    auto isHorizontalWritingMode = renderer.writingMode().isHorizontal();

#if PLATFORM(MAC)
    const auto userPrefersContrast = Theme::singleton().userPrefersContrast();
    FloatRoundedRect boundingRoundedRect = roundedFillRect;
    if (userPrefersContrast)
        context.save();
    else
        context.fillRoundedRect(roundedFillRect, systemColor(CSSValueWebkitControlBackground, styleColorOptions));
#endif

    roundedFillRect.inflateWithRadii(-nativeControlBorderInlineSizeForVectorBasedControls);
    context.fillRoundedRect(roundedFillRect, systemColor(cssValueForProgressAndMeterTrackColor, styleColorOptions));

    context.clipRoundedRect(roundedFillRect);

    FloatRect fillRect(roundedFillRect.rect());

    auto fillRectInlineSize = isHorizontalWritingMode ? fillRect.width() : fillRect.height();
    FloatSize gaugeRegionPosition(fillRectInlineSize * (element->valueRatio() - 1), 0);

    if (!isHorizontalWritingMode)
        gaugeRegionPosition = gaugeRegionPosition.transposedSize();

    if (renderer.writingMode().isInlineFlipped())
        gaugeRegionPosition = -gaugeRegionPosition;

    fillRect.move(gaugeRegionPosition);
    roundedFillRect.setRect(fillRect);

    auto colorCSSValueID = CSSValueInvalid;
    switch (element->gaugeRegion()) {
    case HTMLMeterElement::GaugeRegionOptimum:
        colorCSSValueID = CSSValueAppleSystemGreen;
        break;
    case HTMLMeterElement::GaugeRegionSuboptimal:
        colorCSSValueID = CSSValueAppleSystemYellow;
        break;
    case HTMLMeterElement::GaugeRegionEvenLessGood:
        colorCSSValueID = CSSValueAppleSystemRed;
        break;
    }

    context.fillRoundedRect(roundedFillRect, systemColor(colorCSSValueID, styleColorOptions));

#if PLATFORM(MAC)
    if (userPrefersContrast) {
        Path gaugePath;
        Path boundingPath;
        gaugePath.addRoundedRect(roundedFillRect);
        boundingPath.addRoundedRect(boundingRoundedRect);

        drawHighContrastOutline(context, gaugePath, styleColorOptions);
        context.restore();
        drawHighContrastOutline(context, boundingPath, styleColorOptions);
        return true;
    }
#endif


    return true;
}

bool RenderThemeCocoa::adjustListButtonStyleForVectorBasedControls(RenderStyle& style, const Element* element) const
{
    if (!formControlRefreshEnabled(element))
        return false;

#if PLATFORM(IOS_FAMILY)
    if (style.hasContent() || style.hasUsedContentNone())
        return true;
#endif

    // FIXME: rdar://150914436 The width to height ratio of the button needs to
    // dynamically change according to the overall control size.

    style.setLogicalWidth(15.4_css_percentage);
    style.setLogicalHeight(CSS::Keyword::Auto { });

    return true;
}

static PathWithSize listButtonIndicatorPath(ControlSize controlSize)
{
    switch (controlSize) {
    case ControlSize::Micro:
    case ControlSize::Mini:
    case ControlSize::Small:
    case ControlSize::Medium:
        return chevronDownHeavyPath();
    case ControlSize::Large:
    case ControlSize::ExtraLarge:
        return chevronDownBoldPath();
    }
}

Color RenderThemeCocoa::controlTintColorWithContrast(const RenderStyle& style, const OptionSet<StyleColorOptions> styleColorOptions) const
{
    const auto tintColor = controlTintColor(style, styleColorOptions);
    if (style.accentColor().isAuto())
        return tintColor;

    const auto isDarkMode = styleColorOptions.contains(StyleColorOptions::UseDarkAppearance);
    const auto luminance = tintColor.luminance();

    if ((isDarkMode && luminance <= controlTintLuminanceThreshold) || (!isDarkMode && luminance > controlTintLuminanceThreshold))
        return colorWithTargetLuminance(tintColor, controlTintLuminanceThreshold);

    return tintColor;
}

bool RenderThemeCocoa::paintListButtonForVectorBasedControls(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    if (!formControlRefreshEnabled(box))
        return false;

    CheckedPtr renderBox = dynamicDowncast<RenderBox>(box);
    if (!renderBox)
        return true;

    if (RefPtr shadowHost = box.element()->shadowHost()) {
        CheckedPtr hostStyle = shadowHost->existingComputedStyle();
        if (hostStyle->usedAppearance() == StyleAppearance::None)
            return false;
    }

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver { context };

    // Our paint rect includes the content, padding, and border. Find
    // the rect for just the content so that we don't paint into the
    // padding area. We can't use RenderBox::absoluteContentBoxRect()
    // here because it is inaccurate when transforms are applied.
    auto contentRect = rect;
    const auto contentBox = renderBox->contentBoxRect();
    contentRect.moveBy(contentBox.location());
    contentRect.setSize(contentBox.size());

    CheckedRef style = box.style();

#if PLATFORM(IOS_FAMILY)
    if (style->hasContent() || style->hasUsedContentNone())
        return true;
#endif

    const auto usedZoom = style->usedZoom();
    const auto isVertical = box.writingMode().isVertical();

    const auto blockLength = isVertical ? contentRect.width() : contentRect.height();
    const ControlSize controlSize = listButtonControlSizeForBlockSize(blockLength / usedZoom);

    const auto states = extractControlStyleStatesForRenderer(box);
    const auto isEnabled = states.contains(ControlStyle::State::Enabled);
    const auto isPressed = states.contains(ControlStyle::State::Pressed);

    const auto styleColorOptions = box.styleColorOptions();

#if PLATFORM(MAC)
    const auto effectiveCornerRadius = listButtonCornerRadius(controlSize) * usedZoom;
    auto backgroundColor = systemColor(CSSValueAppleSystemQuaternaryLabel, styleColorOptions);
#endif

    auto indicatorColor = systemColor(CSSValueAppleSystemLabel, styleColorOptions);

    if (!isEnabled) {
        indicatorColor = indicatorColor.colorWithAlphaMultipliedBy(kDisabledControlAlpha);
#if PLATFORM(MAC)
        backgroundColor = backgroundColor.colorWithAlphaMultipliedBy(kDisabledControlAlpha);
#endif
    } else if (isPressed) {
        indicatorColor = platformAdjustedColorForPressedState(indicatorColor, styleColorOptions);
#if PLATFORM(MAC)
        backgroundColor = platformAdjustedColorForPressedState(backgroundColor, styleColorOptions);
#endif
    }

#if PLATFORM(MAC)
    Path backgroundPath;
    backgroundPath.addContinuousRoundedRect(contentRect, effectiveCornerRadius, effectiveCornerRadius);

    context.setFillColor(backgroundColor);
    context.fillPath(backgroundPath);

    if (Theme::singleton().userPrefersContrast())
        drawHighContrastOutline(context, backgroundPath, styleColorOptions);
#endif

    FloatRect chevronRect = contentRect;
    chevronRect.setSize(listButtonIndicatorSize(controlSize) * usedZoom);
    chevronRect.move((contentRect.size() * 0.5f) - (chevronRect.size() * 0.5f));

    const auto indicatorPath = listButtonIndicatorPath(controlSize);
    const auto scale = chevronRect.width() / indicatorPath.originalSize.width();

    context.translate(chevronRect.center() - (indicatorPath.originalSize * scale * 0.5f));
    context.scale(scale);
    context.setFillColor(indicatorColor);
    context.fillPath(indicatorPath.path);

    return true;
}

bool RenderThemeCocoa::adjustProgressBarStyleForVectorBasedControls(RenderStyle&, const Element*) const
{
    return false;
}

static constexpr auto cssValueForInactiveBarFill = CSSValueAppleSystemTertiaryLabel;

bool RenderThemeCocoa::paintProgressBarForVectorBasedControls(const RenderElement& renderer, const PaintInfo& paintInfo, const FloatRect& rect)
{
    if (!formControlRefreshEnabled(renderer))
        return false;

    CheckedPtr renderProgress = dynamicDowncast<RenderProgress>(renderer);
    if (!renderProgress)
        return true;

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    auto styleColorOptions = renderer.styleColorOptions();
    auto isHorizontalWritingMode = renderer.writingMode().isHorizontal();

    constexpr auto barBlockSize = 8.f;

    constexpr auto barCornerRadiusInlineSize = 4.f;
    constexpr auto barCornerRadiusBlockSize = 4.f;

    constexpr auto reducedMotionProgressAnimationMinOpacity = 0.3f;
    constexpr auto reducedMotionProgressAnimationMaxOpacity = 0.6f;

    FloatRoundedRect::Radii barCornerRadii(
        isHorizontalWritingMode ? barCornerRadiusInlineSize : barCornerRadiusBlockSize,
        isHorizontalWritingMode ? barCornerRadiusBlockSize : barCornerRadiusInlineSize
    );

    auto logicalRect = isHorizontalWritingMode ? rect : rect.transposedRect();

    float rectInlineSize = logicalRect.width();
    float rectInlineStart = logicalRect.x();
    float rectBlockSize = logicalRect.height();
    float rectBlockStart = logicalRect.y();

    if (rectBlockSize < barBlockSize) {
        // The rect is smaller than the standard progress bar. We clip to the
        // element's rect to avoid leaking pixels outside the repaint rect.
        context.clip(rect);
    }

    float trackInlineStart = rectInlineStart + nativeControlBorderInlineSizeForVectorBasedControls;
    float trackBlockStart = rectBlockStart + (rectBlockSize - barBlockSize) / 2.0f;
    float trackInlineSize = rectInlineSize - 2 * nativeControlBorderInlineSizeForVectorBasedControls;

    FloatRect trackRect(trackInlineStart, trackBlockStart, trackInlineSize, barBlockSize);
    FloatRoundedRect roundedTrackRect(isHorizontalWritingMode ? trackRect : trackRect.transposedRect(), barCornerRadii);

    FloatRoundedRect roundedTrackBorderRect(roundedTrackRect);
    roundedTrackBorderRect.inflateWithRadii(nativeControlBorderInlineSizeForVectorBasedControls);
    context.fillRoundedRect(roundedTrackBorderRect, systemColor(CSSValueWebkitControlBackground, styleColorOptions));

    context.fillRoundedRect(roundedTrackRect, systemColor(cssValueForProgressAndMeterTrackColor, styleColorOptions));

    float barInlineSize;
    float barInlineStart = trackInlineStart;
    float barBlockStart = trackBlockStart;
    float alpha = 1.0f;

    if (renderProgress->isDeterminate()) {
        barInlineSize = clampTo<float>(renderProgress->position(), 0.0f, 1.0f) * trackInlineSize;

        if (renderProgress->writingMode().isInlineFlipped())
            barInlineStart = trackInlineStart + trackInlineSize - barInlineSize;
    } else {
        Seconds elapsed = MonotonicTime::now() - renderProgress->animationStartTime();
        float progress = fmodf(elapsed.value(), 1.0f);
        bool reverseDirection = static_cast<int>(elapsed.value()) % 2;

        if (Theme::singleton().userPrefersReducedMotion()) {
            barInlineSize = trackInlineSize;

            float difference = progress * (reducedMotionProgressAnimationMaxOpacity - reducedMotionProgressAnimationMinOpacity);
            if (reverseDirection)
                alpha = reducedMotionProgressAnimationMaxOpacity - difference;
            else
                alpha = reducedMotionProgressAnimationMinOpacity + difference;
        } else {
            barInlineSize = 0.25f * trackInlineSize;

            float offset = progress * (trackInlineSize - barInlineSize);
            if (reverseDirection)
                barInlineStart = trackInlineStart + trackInlineSize - offset - barInlineSize;
            else
                barInlineStart += offset;

            context.clipRoundedRect(roundedTrackRect);
        }
    }

    auto isWindowActive = true;
#if PLATFORM(MAC)
    isWindowActive = extractControlStyleStatesForRenderer(renderer).contains(ControlStyle::State::WindowActive);
#endif
    const auto fillColor = isWindowActive ? controlTintColorWithContrast(renderer.style(), styleColorOptions) : systemColor(cssValueForInactiveBarFill, styleColorOptions);

    FloatRect barRect(barInlineStart, barBlockStart, barInlineSize, barBlockSize);
    context.fillRoundedRect(FloatRoundedRect(isHorizontalWritingMode ? barRect : barRect.transposedRect(), barCornerRadii), fillColor.colorWithAlphaMultipliedBy(alpha));

    return true;
}

static bool hasVisibleSliderThumbDescendant(const RenderElement& box)
{
    CheckedPtr renderBox = dynamicDowncast<RenderBox>(box);
    if (!renderBox)
        return false;

    while (CheckedPtr childBox = dynamicDowncast<RenderBox>(renderBox->lastChild()))
        renderBox = childBox;

    CheckedRef style = renderBox->style();
    const auto usedAppearance = style->usedAppearance();
    const auto isSliderThumb = usedAppearance == StyleAppearance::SliderThumbHorizontal || usedAppearance == StyleAppearance::SliderThumbVertical;
    if (isSliderThumb && style->usedVisibility() == Visibility::Visible)
        return true;

    return false;
}

#if PLATFORM(MAC)
constexpr auto trackThicknessForVectorBasedControls = 8.0;
#else
constexpr auto trackThicknessForVectorBasedControls = 4.0;
#endif
constexpr auto trackRadiusForVectorBasedControls = trackThicknessForVectorBasedControls / 2.0;
constexpr auto tickLengthForVectorBasedControls = trackThicknessForVectorBasedControls / 4.0;
constexpr auto defaultSliderTickRadius = trackThicknessForVectorBasedControls / 8.0;
constexpr FloatSize sliderThumbSize = { 24.f, 16.f };

static void paintSliderTicksForVectorBasedControls(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect, bool isThumbVisible, const Color& tickColorOn, const Color& tickColorOff)
{
    // FIXME: RenderTheme{Mac,IOS}::sliderTickSize() and RenderThemeIOS{Mac,IOS}::sliderTickOffsetFromTrackCenter() need to be updated.

    RefPtr input = dynamicDowncast<HTMLInputElement>(box.element());
    if (!input || !input->isRangeControl())
        return;

    RefPtr dataList = input->dataList();
    if (!dataList)
        return;

    double min = input->minimum();
    double max = input->maximum();
    if (min >= max)
        return;

    const auto usedZoom = box.style().usedZoom();
    const auto tickLength = tickLengthForVectorBasedControls * usedZoom;

    FloatRect tickRect(0, 0, tickLength, tickLength);

    bool isHorizontal = box.style().usedAppearance() == StyleAppearance::SliderHorizontal;
    if (isHorizontal)
        tickRect.setY(rect.center().y() - tickRect.height() / 2.0f);
    else
        tickRect.setX(rect.center().x() - tickRect.width() / 2.0f);

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    auto value = input->valueAsNumber();

    bool isInlineFlipped = (!isHorizontal && box.writingMode().isHorizontal()) || box.writingMode().isInlineFlipped();
    FloatRect layoutRectForTicks(rect);

    auto tickMargin = isThumbVisible ? sliderThumbSize.width() / 2.f * usedZoom - tickLength / 2.f : 0.f;

    if (isHorizontal) {
        layoutRectForTicks.setWidth(rect.width() - (tickMargin * 2.f));
        layoutRectForTicks.setHeight(tickLength);
        layoutRectForTicks.setX(rect.x() + tickMargin);
        layoutRectForTicks.setY(rect.center().y() - tickRect.height() / 2.0f);
    } else {
        layoutRectForTicks.setWidth(tickLength);
        layoutRectForTicks.setHeight(rect.height() - (tickMargin * 2.f));
        layoutRectForTicks.setX(rect.center().x() - tickRect.width() / 2.0f);
        layoutRectForTicks.setY(rect.y() + tickMargin);
    }

    float alpha = RenderTheme::singleton().isEnabled(box) ? 1.0f : kDisabledControlAlpha;

    for (auto& optionElement : dataList->suggestions()) {
        if (auto optionValue = input->listOptionValueAsDouble(optionElement)) {
            auto tickFraction = (*optionValue - min) / (max - min);
            auto tickRatio = isInlineFlipped ? 1.0 - tickFraction : tickFraction;

            // Don't draw ticks if they're associated with the max or min value since they would be
            // partially clipped by the rounded border of the track.
            if (!isThumbVisible && (WTF::areEssentiallyEqual(tickRatio, 0.0) || WTF::areEssentiallyEqual(tickRatio, 1.0)))
                continue;

            if (isHorizontal)
                tickRect.setX(layoutRectForTicks.x() + tickRatio * (layoutRectForTicks.width() - tickRect.width()));
            else
                tickRect.setY(layoutRectForTicks.y() + tickRatio * (layoutRectForTicks.height() - tickRect.height()));

            // Snap the tick to device pixels along the sliding axis so that it lines up with the slider thumb,
            // but keep the width and height equal so that it remains a circle.
            const auto deviceScaleFactor = box.document().deviceScaleFactor();
            tickRect = snapRectToDevicePixels(LayoutRect(tickRect), deviceScaleFactor);

            if (isHorizontal)
                tickRect.setHeight(tickRect.width());
            else
                tickRect.setWidth(tickRect.height());

            const auto tickColor = (value >= *optionValue) ? tickColorOn : tickColorOff;
            context.setFillColor(tickColor.colorWithAlphaMultipliedBy(alpha));
            context.fillEllipse(tickRect);
        }
    }
}

bool RenderThemeCocoa::adjustSliderTrackStyleForVectorBasedControls(RenderStyle&, const Element* element) const
{
    if (!formControlRefreshEnabled(element))
        return false;

    return true;
}

bool RenderThemeCocoa::paintSliderTrackForVectorBasedControls(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    if (!formControlRefreshEnabled(box))
        return false;

    CheckedPtr renderSlider = dynamicDowncast<RenderSlider>(box);
    if (!renderSlider)
        return true;

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    const auto states = extractControlStyleStatesForRenderer(box);
    const auto isEnabled = states.contains(ControlStyle::State::Enabled);
#if PLATFORM(MAC)
    const auto isPressed = states.contains(ControlStyle::State::Pressed);
#endif

    bool isHorizontal = true;
    FloatRect trackClip = rect;

    const auto usedZoom = box.style().usedZoom();
    const auto trackThickness = trackThicknessForVectorBasedControls * usedZoom;

    switch (box.style().usedAppearance()) {
    case StyleAppearance::SliderHorizontal:
        // Inset slightly so the thumb covers the edge.
        if (trackClip.width() > 2) {
            trackClip.setWidth(trackClip.width() - 2);
            trackClip.setX(trackClip.x() + 1);
        }
        trackClip.setHeight(trackThickness);
        trackClip.setY(rect.y() + rect.height() / 2 - trackThickness / 2);
        break;
    case StyleAppearance::SliderVertical:
        isHorizontal = false;
        // Inset slightly so the thumb covers the edge.
        if (trackClip.height() > 2) {
            trackClip.setHeight(trackClip.height() - 2);
            trackClip.setY(trackClip.y() + 1);
        }
        trackClip.setWidth(trackThickness);
        trackClip.setX(rect.x() + rect.width() / 2 - trackThickness / 2);
        break;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }

    auto cornerRadius = std::min(trackClip.width(), trackClip.height()) / 2.f;

    FloatRoundedRect::Radii cornerRadii(cornerRadius, cornerRadius);
    FloatRoundedRect innerBorder(trackClip, cornerRadii);
    FloatRoundedRect outerBorder(trackClip, cornerRadii);

    outerBorder.inflateWithRadii(nativeControlBorderInlineSizeForVectorBasedControls * usedZoom);

    auto styleColorOptions = box.styleColorOptions();

    auto borderColor = systemColor(CSSValueWebkitControlBackground, styleColorOptions);
    auto trackColor = systemColor(CSSValueAppleSystemOpaqueFill, styleColorOptions);

#if PLATFORM(MAC)
    const auto isWindowActive = states.contains(ControlStyle::State::WindowActive);
    auto fillColor = isWindowActive ? controlTintColorWithContrast(box.style(), styleColorOptions) : systemColor(cssValueForInactiveBarFill, styleColorOptions);
#else
    auto fillColor = controlTintColorWithContrast(box.style(), styleColorOptions);
#endif
    auto unadjustedFillColor = fillColor;

    if (!isEnabled) {
        trackColor = trackColor.colorWithAlphaMultipliedBy(kDisabledControlAlpha);
        borderColor = borderColor.colorWithAlphaMultipliedBy(kDisabledControlAlpha);
        fillColor = fillColor.colorWithAlphaMultipliedBy(kDisabledControlAlpha);
    }

#if PLATFORM(MAC)
    if (isPressed && isEnabled) {
        trackColor = colorWithContrastOverlay(trackColor, styleColorOptions, 0.05f);
        fillColor = colorWithContrastOverlay(fillColor, styleColorOptions, 0.08f);
    }
#endif

    context.fillRoundedRect(outerBorder, borderColor);
    context.fillRoundedRect(innerBorder, trackColor);
    context.clipRoundedRect(innerBorder);

    const auto isThumbVisible = hasVisibleSliderThumbDescendant(box);
    double valueRatio = renderSlider->valueRatio();

    auto hasTicks = false;
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(box.element()); input
        && input->isRangeControl()
        && input->dataList()
        && input->minimum() < input->maximum())
        hasTicks = true;

    // If the thumb is not visible and ticks are present, we should increase the indicator rect length to
    // fully contain the tick associated with the current value.
    const auto needsAdditionalLength = hasTicks && !isThumbVisible && !WTF::areEssentiallyEqual(valueRatio, 0.0) && !WTF::areEssentiallyEqual(valueRatio, 1.0);
    const auto tickLength = tickLengthForVectorBasedControls * usedZoom;
    const auto additionalLength = tickLength * 1.5f;

    if (isHorizontal) {
        double newWidth = trackClip.width() * valueRatio;
        if (needsAdditionalLength)
            newWidth += additionalLength;

        if (box.writingMode().isInlineFlipped())
            trackClip.move(trackClip.width() - newWidth, 0);

        trackClip.setWidth(newWidth);
    } else {
        float height = trackClip.height();
        float newHeight = height * valueRatio;
        if (needsAdditionalLength)
            newHeight += tickLength * additionalLength;

        if (box.writingMode().isHorizontal() || box.writingMode().isInlineFlipped())
            trackClip.setY(trackClip.y() + height - newHeight);

        trackClip.setHeight(newHeight);
    }

    const auto fillCornerRadius = isThumbVisible ? 0.f : std::min(trackClip.width(), trackClip.height()) / 2.f;
    const auto fillCornerRadii = FloatRoundedRect::Radii { fillCornerRadius, fillCornerRadius };

    FloatRoundedRect fillRect(trackClip, fillCornerRadii);
    context.fillRoundedRect(fillRect, fillColor);
    if (hasTicks) {
        auto tickColorOn = foregroundColorForBackgroundColor(unadjustedFillColor);
        tickColorOn = isEnabled ? tickColorOn : tickColorOn.colorWithAlphaMultipliedBy(kDisabledControlAlpha);

        const auto cssValueForTickOffColor = isEnabled ? CSSValueAppleSystemTertiaryLabel : CSSValueAppleSystemQuaternaryLabel;
        const auto tickColorOff = systemColor(cssValueForTickOffColor, box.styleColorOptions());

        paintSliderTicksForVectorBasedControls(box, paintInfo, rect, isThumbVisible, tickColorOn, tickColorOff);
    }

    return true;
}

bool RenderThemeCocoa::adjustSliderThumbSizeForVectorBasedControls(RenderStyle& style, const Element* element) const
{
    if (!formControlRefreshEnabled(element))
        return false;

    auto appearance = style.usedAppearance();
    auto hasHorizontalThumbAppearance = appearance == StyleAppearance::SliderThumbHorizontal;
    auto hasVerticalThumbAppearance = appearance == StyleAppearance::SliderThumbVertical;

    if (!hasHorizontalThumbAppearance && !hasVerticalThumbAppearance)
        return true;

    bool isVertical = style.writingMode().isVertical() || hasVerticalThumbAppearance;

    static constexpr auto kDefaultSliderThumbWidth = 24;
    static constexpr auto kDefaultSliderThumbHeight = 16;

    const float sliderThumbWidthForLayout = isVertical ? kDefaultSliderThumbHeight : kDefaultSliderThumbWidth;
    const float sliderThumbHeightForLayout = isVertical ? kDefaultSliderThumbWidth : kDefaultSliderThumbHeight;

    const auto usedZoom = usedZoomForComputedStyle(style);

    // Enforce a 24x16 size (16x24 in vertical mode) if no size is provided.
    if (style.width().isIntrinsicOrLegacyIntrinsicOrAuto() || style.height().isAuto()) {
        style.setWidth(Style::PreferredSize::Fixed { sliderThumbWidthForLayout * usedZoom });
        style.setHeight(Style::PreferredSize::Fixed { sliderThumbHeightForLayout * usedZoom });
    }

    return true;
}

bool RenderThemeCocoa::adjustSliderThumbStyleForVectorBasedControls(RenderStyle& style, const Element* element) const
{
    if (!formControlRefreshEnabled(element))
        return false;

    RenderTheme::adjustSliderThumbStyle(style, element);
    style.setBoxShadow(CSS::Keyword::None { });

    return true;
}

bool RenderThemeCocoa::paintSliderThumbForVectorBasedControls(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    if (!formControlRefreshEnabled(box))
        return false;

    CheckedRef style = box.style();

    const auto sliderThumbShape = shapeForSliderThumb(box, rect);
    if (!sliderThumbShape.path)
        return false;

    const auto sliderThumbPath = sliderThumbShape.path.value();

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver { context };

    Color thumbColor = Color::white;

#if PLATFORM(MAC)
    const auto states = extractControlStyleStatesForRenderer(box);
    const auto styleColorOptions = box.styleColorOptions();

    if (states.contains(ControlStyle::State::Pressed))
        thumbColor = colorWithContrastOverlay(thumbColor, styleColorOptions, 0.08f);
#endif

    // This is the shadow used for the native slider thumb on iOS. Here, it is used on macOS
    // as well to improve contrast against light backgrounds.
    static constexpr FloatSize innerShadowOffset { 0, 0 };
    static constexpr auto firstShadowColor = DisplayP3<float> { 0, 0, 0, 0.24f };
    const auto innerShadowBlur = sliderThumbShadowRadius * style->usedZoom();

    context.setDropShadow({ innerShadowOffset, innerShadowBlur, firstShadowColor, ShadowRadiusMode::Default });
    context.setFillColor(thumbColor);
    context.fillPath(sliderThumbPath);

#if PLATFORM(MAC)
    if (Theme::singleton().userPrefersContrast()) {
        drawHighContrastOutline(context, sliderThumbPath, styleColorOptions);
        return true;
    }
#endif

    return true;
}

bool RenderThemeCocoa::adjustSearchFieldStyleForVectorBasedControls(RenderStyle& style, const Element* element) const
{
    if (!formControlRefreshEnabled(element))
        return false;

    applyEmPadding(style, element, standardTextControlInlinePaddingEm, standardTextControlBlockPaddingEm);

    return true;
}

bool RenderThemeCocoa::paintSearchFieldForVectorBasedControls(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    if (!formControlRefreshEnabled(box))
        return false;

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    const auto [optionalPath, boundingRect, cornerRadius, _] = shapeForSearchField(box, rect);
    if (!optionalPath)
        return false;

    const auto path = optionalPath.value();

    const auto styleColorOptions = box.styleColorOptions();

    const auto& style = box.style();
    const auto borderThickness = style.usedZoom();
    auto backgroundColor = style.backgroundColor().resolvedColor();

    const auto states = RenderTheme::singleton().extractControlStyleStatesForRenderer(box);
    const auto isEnabled = states.contains(ControlStyle::State::Enabled);

#if PLATFORM(MAC)
    auto userPrefersContrast = Theme::singleton().userPrefersContrast();
    auto isDarkMode = box.styleColorOptions().contains(StyleColorOptions::UseDarkAppearance);
    Color borderColor;
    if (userPrefersContrast)
        borderColor = highContrastOutlineColor(styleColorOptions);
    else
        borderColor = isDarkMode ? Color::transparentBlack : RenderTheme::singleton().systemColor(CSSValueAppleSystemQuinaryLabel, styleColorOptions);
#else
    auto userPrefersContrast = false;
    auto borderColor = RenderTheme::singleton().systemColor(CSSValueWebkitControlBackground, styleColorOptions);
#endif
    if (!isEnabled) {
        backgroundColor = backgroundColor.colorWithAlphaMultipliedBy(0.5f);
        if (!userPrefersContrast)
            borderColor = borderColor.colorWithAlphaMultipliedBy(0.5f);
    }

    CSSToLengthConversionData conversionData(style, nullptr, nullptr, nullptr);

    if (controlIsFocusedWithOutlineStyleAutoForVectorBasedControls(box))
        drawFocusRingForPathForVectorBasedControls(box, paintInfo, boundingRect, path);

    context.setFillColor(borderColor);
    context.fillPath(path);

    FloatRect innerRect = boundingRect;
    innerRect.inflate(-borderThickness);

    Path innerPath;
    innerPath.addContinuousRoundedRect(innerRect, cornerRadius - borderThickness);

    context.setFillColor(backgroundColor);
    context.fillPath(innerPath);

    return true;
}

bool RenderThemeCocoa::paintSearchFieldDecorationsForVectorBasedControls(const RenderBox&, const PaintInfo&, const FloatRect&)
{
    return false;
}


bool RenderThemeCocoa::adjustSearchFieldCancelButtonStyleForVectorBasedControls(RenderStyle& style, const Element* element) const
{
#if PLATFORM(MAC)
    if (!formControlRefreshEnabled(element))
        return false;

    Ref emSize = CSSPrimitiveValue::create(1, CSSUnitType::CSS_EM);
    auto pixelsPerEm = emSize->resolveAsLength<float>(conversionDataForStyle(style));

    style.setWidth(Style::PreferredSize::Fixed { searchFieldDecorationEmSize * pixelsPerEm });
    style.setHeight(Style::PreferredSize::Fixed { searchFieldDecorationEmSize * pixelsPerEm });
    return true;
#else
    UNUSED_PARAM(style);
    UNUSED_PARAM(element);
    return false;
#endif
}

bool RenderThemeCocoa::paintSearchFieldCancelButtonForVectorBasedControls(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if PLATFORM(MAC)
    if (!formControlRefreshEnabled(box))
        return false;

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    const FloatSize glyphSize(20.2832f, 19.9316f);

    Path glyphPath;
    glyphPath.moveTo({ 19.9219f, 9.96094f });
    glyphPath.addBezierCurveTo({ 19.9219f, 15.4004f }, { 15.4102f, 19.9219f }, { 9.96094f, 19.9219f });
    glyphPath.addBezierCurveTo({ 4.52148f, 19.9219f }, { 0.0f, 15.4004f }, { 0.0f, 9.96094f });
    glyphPath.addBezierCurveTo({ 0.0f, 4.51172f }, { 4.51172f, 0.0f }, { 9.95117f, 0.0f });
    glyphPath.addBezierCurveTo({ 15.4004f, 0.0f }, { 19.9219f, 4.51172f }, { 19.9219f, 9.96094f });
    glyphPath.closeSubpath();
    glyphPath.moveTo({ 12.7051f, 6.11328f });
    glyphPath.addLineTo({ 9.96094f, 8.83789f });
    glyphPath.addLineTo({ 7.23633f, 6.12305f });
    glyphPath.addBezierCurveTo({ 7.08008f, 5.97656f }, { 6.9043f, 5.89844f }, { 6.67969f, 5.89844f });
    glyphPath.addBezierCurveTo({ 6.23047f, 5.89844f }, { 5.87891f, 6.24023f }, { 5.87891f, 6.69922f });
    glyphPath.addBezierCurveTo({ 5.87891f, 6.91406f }, { 5.95703f, 7.10938f }, { 6.11328f, 7.26562f });
    glyphPath.addLineTo({ 8.81836f, 9.9707f });
    glyphPath.addLineTo({ 6.11328f, 12.6855f });
    glyphPath.addBezierCurveTo({ 5.95703f, 12.832f }, { 5.87891f, 13.0371f }, { 5.87891f, 13.252f });
    glyphPath.addBezierCurveTo({ 5.87891f, 13.7012f }, { 6.23047f, 14.0625f }, { 6.67969f, 14.0625f });
    glyphPath.addBezierCurveTo({ 6.9043f, 14.0625f }, { 7.10938f, 13.9844f }, { 7.26562f, 13.8281f });
    glyphPath.addLineTo({ 9.96094f, 11.1133f });
    glyphPath.addLineTo({ 12.666f, 13.8281f });
    glyphPath.addBezierCurveTo({ 12.8125f, 13.9844f }, { 13.0176f, 14.0625f }, { 13.2422f, 14.0625f });
    glyphPath.addBezierCurveTo({ 13.7012f, 14.0625f }, { 14.0625f, 13.7012f }, { 14.0625f, 13.252f });
    glyphPath.addBezierCurveTo({ 14.0625f, 13.0273f }, { 13.9844f, 12.8223f }, { 13.8184f, 12.6758f });
    glyphPath.addLineTo({ 11.1133f, 9.9707f });
    glyphPath.addLineTo({ 13.8281f, 7.25586f });
    glyphPath.addBezierCurveTo({ 14.0039f, 7.08008f }, { 14.0723f, 6.9043f }, { 14.0723f, 6.67969f });
    glyphPath.addBezierCurveTo({ 14.0723f, 6.23047f }, { 13.7109f, 5.87891f }, { 13.2617f, 5.87891f });
    glyphPath.addBezierCurveTo({ 13.0469f, 5.87891f }, { 12.8711f, 5.94727f }, { 12.7051f, 6.11328f });
    glyphPath.closeSubpath();

    FloatRect paintRect(rect);
    float scale = paintRect.width() / glyphSize.width();

    AffineTransform transform;
    transform.translate(paintRect.center() - (glyphSize * scale * 0.5f));
    transform.scale(scale);
    glyphPath.transform(transform);

    auto isEnabled = true;
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(box.protectedNode().get()->shadowHost()))
        isEnabled = !input->isDisabledFormControl();

    const auto styleColorOptions = box.styleColorOptions();
    const auto isDarkMode = styleColorOptions.contains(StyleColorOptions::UseDarkAppearance);

    Color fillColor;
    if (Theme::singleton().userPrefersContrast())
        fillColor = highContrastOutlineColor(styleColorOptions);
    else {
        fillColor = isDarkMode ? SRGBA<uint8_t> { 161, 161, 161 } : SRGBA<uint8_t> { 76, 76, 76, 216 };
        if (!isEnabled)
            fillColor = fillColor.colorWithAlphaMultipliedBy(0.5f);
    }

    context.setFillColor(fillColor);
    context.fillPath(glyphPath);

    return true;
#else
    UNUSED_PARAM(box);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

bool RenderThemeCocoa::adjustSearchFieldDecorationPartStyleForVectorBasedControls(RenderStyle& style, const Element* element) const
{
    if (!formControlRefreshEnabled(element))
        return false;

    Ref emSize = CSSPrimitiveValue::create(1, CSSUnitType::CSS_EM);
    auto pixelsPerEm = emSize->resolveAsLength<float>(conversionDataForStyle(style));

#if PLATFORM(MAC)
    RefPtr input = dynamicDowncast<HTMLInputElement>(element->shadowHost());
    auto hasSearchResults = input && input->maxResults() > 0;

    const auto isInlineFlipped = style.writingMode().isInlineFlipped();
    const auto sizeWithDropdown = isInlineFlipped ? searchFieldDecorationWithDropdownEmSizeRTL : searchFieldDecorationWithDropdownEmSizeLTR;
    const auto searchFieldDecorationWidth = (hasSearchResults ? sizeWithDropdown : searchFieldDecorationEmSize);
#else
    const auto searchFieldDecorationWidth = searchFieldDecorationEmSize;
#endif
    auto searchFieldDecorationHeight = searchFieldDecorationEmSize;

    style.setWidth(Style::PreferredSize::Fixed { static_cast<float>(searchFieldDecorationWidth * pixelsPerEm) });
    style.setHeight(Style::PreferredSize::Fixed { static_cast<float>(searchFieldDecorationHeight * pixelsPerEm) });
    style.setMarginEnd(Style::MarginEdge::Fixed { static_cast<float>(searchFieldDecorationEmMargin * pixelsPerEm) });
    return true;
}

bool RenderThemeCocoa::paintSearchFieldDecorationPartForVectorBasedControls(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    if (!formControlRefreshEnabled(box))
        return false;

    auto styleColorOptions = box.styleColorOptions();
    auto isDarkMode = styleColorOptions.contains(StyleColorOptions::UseDarkAppearance);

    RefPtr input = dynamicDowncast<HTMLInputElement>(box.element()->shadowHost());
    auto isEnabled = input && !input->isDisabledFormControl();

    static constexpr auto colorForDarkMode = Color::white;
    static constexpr auto colorForLightMode = SRGBA<uint8_t> { 76, 76, 76 };

#if PLATFORM(MAC)
    // In dark mode, the high contrast color is darker than white, which is what
    // we use if "Increase contrast" was off. To avoid decreasing contrast in this case,
    // only behave as if "Increase contrast" is enabled when not in dark mode.
    auto userPrefersContrast = Theme::singleton().userPrefersContrast()  && !isDarkMode;
    auto hasSearchResults = input && input->maxResults() > 0;

    auto color = userPrefersContrast ? highContrastOutlineColor(styleColorOptions) : (isDarkMode ? colorForDarkMode : colorForLightMode);
#else
    auto userPrefersContrast = false;
    Color color = isDarkMode ? colorForDarkMode : colorForLightMode;
#endif

    if (!userPrefersContrast && !isEnabled)
        color = color.colorWithAlphaMultipliedBy(0.5f);

    auto& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver(context);

    const FloatSize glyphSize(73.0f, 73.0f);

    Path glyphPath;
    glyphPath.moveTo({ 29.6875f, 59.375f });
    glyphPath.addBezierCurveTo({ 35.9863f, 59.375f }, { 41.7969f, 57.422f }, { 46.6309f, 54.0528f });
    glyphPath.addLineTo({ 63.9649f, 71.3868f });
    glyphPath.addBezierCurveTo({ 64.8926f, 72.3145f }, { 66.1133f, 72.754f }, { 67.3829f, 72.754f });
    glyphPath.addBezierCurveTo({ 70.1172f, 72.754f }, { 72.1191f, 70.6544f }, { 72.1191f, 67.9688f });
    glyphPath.addBezierCurveTo({ 72.1191f, 66.6993f }, { 71.6797f, 65.4786f }, { 70.7519f, 64.5508f });
    glyphPath.addLineTo({ 53.5644f, 47.3145f });
    glyphPath.addBezierCurveTo({ 57.2266f, 42.3829f }, { 59.375f, 36.2793f }, { 59.375f, 29.6875f });
    glyphPath.addBezierCurveTo({ 59.375f, 13.3301f }, { 46.045f, 0.0f }, { 29.6875f, 0.0f });
    glyphPath.addBezierCurveTo({ 13.3301f, 0.0f }, { 0.0f, 13.3301f }, { 0.0f, 29.6875f });
    glyphPath.addBezierCurveTo({ 0.0f, 46.045f }, { 13.33f, 59.375f }, { 29.6875f, 59.375f });
    glyphPath.moveTo({ 29.6875f, 52.0997f });
    glyphPath.addBezierCurveTo({ 17.4316f, 52.0997f }, { 7.2754f, 41.9434f }, { 7.2754f, 29.6875f });
    glyphPath.addBezierCurveTo({ 7.2754f, 17.3829f }, { 17.4316f, 7.2754f }, { 29.6875f, 7.2754f });
    glyphPath.addBezierCurveTo({ 41.9922f, 7.2754f }, { 52.1f, 17.3829f }, { 52.1f, 29.6875f });
    glyphPath.addBezierCurveTo({ 52.1f, 41.9435f }, { 41.9922f, 52.0997f }, { 29.6875f, 52.0997f });

    FloatRect magnifyingGlassRect(rect);
#if PLATFORM(MAC)
    if (hasSearchResults) {
        // We must show a dropdown indicator because the "results" attribute is present on the
        // search input element, and its value is greater than 0. The style adjustment method
        // will have already increased the width of the decoration to make room, so adjust the
        // width of magnifyingGlassRect to keep it a square.
        magnifyingGlassRect.setWidth(magnifyingGlassRect.height());

        constexpr auto searchFieldDropdownToMainGlyphRatio = 0.6f;
        const auto dropdownGlyphWidth = magnifyingGlassRect.width() * searchFieldDropdownToMainGlyphRatio;

        FloatRect dropdownGlyphRect(rect);
        dropdownGlyphRect.setWidth(dropdownGlyphWidth);

        if (box.style().writingMode().isInlineFlipped())
            magnifyingGlassRect.setX(rect.maxX() - magnifyingGlassRect.width());
        else
            dropdownGlyphRect.setX(rect.maxX() - dropdownGlyphWidth);

        const auto dropdownPath = chevronDownPath();
        const auto scale = dropdownGlyphRect.width() / dropdownPath.originalSize.width();

        context.save();
        context.translate(dropdownGlyphRect.center() - (dropdownPath.originalSize * scale * 0.5f));
        context.scale(scale);
        context.setFillColor(color);
        context.fillPath(dropdownPath.path);
        context.restore();
    }
#endif

    float scale = magnifyingGlassRect.width() / glyphSize.width();

    AffineTransform transform;
    transform.translate(magnifyingGlassRect.center() - (glyphSize * scale * 0.5f));
    transform.scale(scale);
    glyphPath.transform(transform);

    context.setFillColor(color);
    context.fillPath(glyphPath);

    return true;
}

bool RenderThemeCocoa::adjustSearchFieldResultsDecorationPartStyleForVectorBasedControls(RenderStyle& style, const Element* element) const
{
    return adjustSearchFieldDecorationPartStyleForVectorBasedControls(style, element);
}

bool RenderThemeCocoa::paintSearchFieldResultsDecorationPartForVectorBasedControls(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    return paintSearchFieldDecorationPartForVectorBasedControls(box, paintInfo, rect);
}

bool RenderThemeCocoa::adjustSearchFieldResultsButtonStyleForVectorBasedControls(RenderStyle& style, const Element* element) const
{
    return adjustSearchFieldDecorationPartStyleForVectorBasedControls(style, element);
}

bool RenderThemeCocoa::paintSearchFieldResultsButtonForVectorBasedControls(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    return paintSearchFieldDecorationPartForVectorBasedControls(box, paintInfo, rect);
}

bool RenderThemeCocoa::adjustSwitchStyleForVectorBasedControls(RenderStyle& style, const Element* element) const
{
    if (!formControlRefreshEnabled(element))
        return false;

    // FIXME: Deduplicate sizing with the generic code somehow.
    if (style.width().isIntrinsicOrLegacyIntrinsicOrAuto() || style.height().isIntrinsicOrLegacyIntrinsicOrAuto()) {
        auto usedZoom = usedZoomForComputedStyle(style);
#if PLATFORM(VISION)
        style.setLogicalWidth(Style::PreferredSize::Fixed { logicalSwitchWidth * usedZoom });
#else
        style.setLogicalWidth(Style::PreferredSize::Fixed { logicalRefreshedSwitchWidth * usedZoom });
#endif
        style.setLogicalHeight(Style::PreferredSize::Fixed { logicalSwitchHeight * usedZoom });
    }

    adjustSwitchStyleDisplay(style);

    return true;
}

bool RenderThemeCocoa::paintPlatformResizerForVectorBasedControls(const RenderLayerModelObject& renderer, GraphicsContext& context, const LayoutRect& resizerCornerRect)
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(renderer);
    UNUSED_PARAM(context);
    UNUSED_PARAM(resizerCornerRect);
    return false;
#else

    if (!formControlRefreshEnabled(renderer))
        return false;

    GraphicsContextStateSaver stateSaver(context);

    FloatRect paintRect(resizerCornerRect);
    float length = std::min(paintRect.width(), paintRect.height());
    paintRect.setSize({ length, length });
    paintRect.setLocation(resizerCornerRect.maxXMaxYCorner() - paintRect.size());

    const auto barThickness = length * 0.075f;
    const auto barRadii = FloatRoundedRect::Radii { barThickness / 2.f, barThickness / 2.f };

    const auto wideBarWidth = length * 0.75f;
    const auto wideBarX = paintRect.x() + (paintRect.width() - wideBarWidth) / 2.f;
    const auto wideBarY = paintRect.y() + paintRect.height() / 2.f - barThickness;
    const FloatRoundedRect wideBarRect({ wideBarX, wideBarY, wideBarWidth, barThickness }, barRadii);

    const auto smallBarWidth = wideBarWidth / 2.f;
    const auto smallBarX = paintRect.x() + (paintRect.width() - smallBarWidth) / 2.f;
    const auto smallBarY = wideBarY + 2.5f * barThickness;
    const FloatRoundedRect smallBarRect({ smallBarX, smallBarY, smallBarWidth, barThickness }, barRadii);

    Path path;
    path.addRoundedRect(wideBarRect);
    path.addRoundedRect(smallBarRect);

    const auto styleColorOptions = renderer.styleColorOptions();

    Color resizerColor;
    if (Theme::singleton().userPrefersContrast())
        resizerColor = highContrastOutlineColor(styleColorOptions);
    else
        resizerColor = systemColor(CSSValueAppleSystemSecondaryLabel, styleColorOptions);

    auto rotation = -piOverFourFloat;
    if (renderer.shouldPlaceVerticalScrollbarOnLeft())
        rotation *= -1;

    context.setFillColor(resizerColor);
    context.translate(paintRect.center());
    context.rotate(rotation);
    context.translate(-paintRect.center());
    context.fillPath(path);

    return true;
#endif
}

bool RenderThemeCocoa::paintPlatformResizerFrameForVectorBasedControls(const RenderLayerModelObject& renderer, GraphicsContext&, const LayoutRect&)
{
    return formControlRefreshEnabled(renderer);
}

bool RenderThemeCocoa::supportsFocusRingForVectorBasedControls(const RenderElement& box, const RenderStyle& style) const
{
    if (!formControlRefreshEnabled(box))
        return RenderTheme::supportsFocusRing(box, style);

    // Menulist buttons that have devolved will still have their paint methods called. This
    // occurs because we do not set their used appearance to `None` in order to keep their
    // decorations. We skip painting the focus ring from the theme in this scenario.
    if (style.usedAppearance() == StyleAppearance::MenulistButton)
        return !style.nativeAppearanceDisabled();

#if PLATFORM(MAC)
    return style.hasUsedAppearance() && style.usedAppearance() != StyleAppearance::Listbox;
#else
    return style.hasUsedAppearance();
#endif
}

static inline bool shouldAdjustTextControlInnerElementStyles(const RenderStyle& shadowHostStyle, const Element* shadowHost)
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(shadowHost);
    return input && input->hasDataList() && !shadowHostStyle.nativeAppearanceDisabled();
}

bool RenderThemeCocoa::adjustTextControlInnerContainerStyleForVectorBasedControls(RenderStyle&, const RenderStyle&, const Element* shadowHost) const
{
    if (!formControlRefreshEnabled(shadowHost))
        return false;

    return true;
}

bool RenderThemeCocoa::adjustTextControlInnerPlaceholderStyleForVectorBasedControls(RenderStyle& style, const RenderStyle& shadowHostStyle, const Element* shadowHost) const
{
    if (!formControlRefreshEnabled(shadowHost))
        return false;

    if (shouldAdjustTextControlInnerElementStyles(shadowHostStyle, shadowHost))
        applyEmPadding(style, shadowHost, 0.4f, 0.f);

    return true;
}

bool RenderThemeCocoa::adjustTextControlInnerTextStyleForVectorBasedControls(RenderStyle& style, const RenderStyle& shadowHostStyle, const Element* shadowHost) const
{
    if (!formControlRefreshEnabled(shadowHost))
        return false;

    if (shouldAdjustTextControlInnerElementStyles(shadowHostStyle, shadowHost))
        applyEmPadding(style, shadowHost, 0.4f, 0.15f);

    return true;
}

Color RenderThemeCocoa::submitButtonTextColor(const RenderText& textRenderer) const
{
    auto isEnabled = true;
    if (CheckedPtr controlRenderer = textRenderer.parent()) {
        if ((controlRenderer = controlRenderer->parent())) {
            const auto states = extractControlStyleStatesForRenderer(*controlRenderer.get());
            isEnabled = states.contains(ControlStyle::State::Enabled);
        }
    }

    const auto tintColor = controlTintColorWithContrast(textRenderer.style(), textRenderer.styleColorOptions());
    auto textColor = foregroundColorForBackgroundColor(tintColor);
    if (!isEnabled)
        textColor = textColor.colorWithAlphaMultipliedBy(textColor != Color::white ? 0.3f : 0.6f);

    return textColor;
}

bool RenderThemeCocoa::mayNeedBleedAvoidance(const RenderStyle& style) const
{
    if (style.nativeAppearanceDisabled())
        return true;

    switch (style.usedAppearance()) {
    case StyleAppearance::BorderlessAttachment:
    case StyleAppearance::Button:
    case StyleAppearance::Checkbox:
#if PLATFORM(MAC)
    case StyleAppearance::ColorWell:
    case StyleAppearance::ColorWellSwatch:
#endif
    case StyleAppearance::DefaultButton:
    case StyleAppearance::InnerSpinButton:
    case StyleAppearance::ListButton:
#if PLATFORM(MAC)
    case StyleAppearance::Menulist:
#else
    case StyleAppearance::MenulistButton:
#endif
    case StyleAppearance::Meter:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::PushButton:
    case StyleAppearance::Radio:
    case StyleAppearance::SearchField:
#if PLATFORM(MAC)
    case StyleAppearance::SearchFieldCancelButton:
#endif
    case StyleAppearance::SearchFieldDecoration:
    case StyleAppearance::SearchFieldResultsButton:
    case StyleAppearance::SearchFieldResultsDecoration:
    case StyleAppearance::SquareButton:
    case StyleAppearance::SliderHorizontal:
    case StyleAppearance::SliderThumbHorizontal:
    case StyleAppearance::SliderThumbVertical:
    case StyleAppearance::SliderVertical:
    case StyleAppearance::TextArea:
    case StyleAppearance::TextField:
    case StyleAppearance::SwitchThumb:
    case StyleAppearance::SwitchTrack:
        return false;
    default:
        return true;
    }
}

std::optional<RoundedShape> RenderThemeCocoa::shapeForInteractionRegion(const RenderBox& box, const FloatRect& rect, ShouldComputePath computePath)
{
    CheckedRef style = box.style();

    // Menulist buttons retain their used appearance even after devolving, so we
    // must do an additional check before returning an adjusted path. On macOS,
    // we don't paint menulist buttons.
    auto tryPathForMenulistButton = [&] -> std::optional<RoundedShape> {
#if !PLATFORM(MAC)
        if (style->nativeAppearanceDisabled())
            return std::nullopt;
#endif
        return std::optional { shapeForButton(box, rect, computePath) };
    }();

    switch (style->usedAppearance()) {
    case StyleAppearance::Button:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::PushButton:
    case StyleAppearance::SquareButton:
        return shapeForButton(box, rect, computePath);
    case StyleAppearance::Checkbox:
        return shapeForCheckbox(rect, computePath);
    case StyleAppearance::MenulistButton:
        return tryPathForMenulistButton;
    case StyleAppearance::Radio:
        return shapeForRadioButton(rect, computePath);
    case StyleAppearance::SearchField:
        return shapeForSearchField(box, rect, computePath);
    case StyleAppearance::SliderThumbHorizontal:
    case StyleAppearance::SliderThumbVertical:
        return shapeForSliderThumb(box, rect, computePath);
    case StyleAppearance::Switch:
    case StyleAppearance::SwitchTrack:
        return shapeForSwitchTrack(box, rect, computePath);
    case StyleAppearance::TextField:
        return shapeForTextAreaOrTextField(box, rect, computePath);
    default:
        return std::nullopt;
    }
}

FloatSize RenderThemeCocoa::inflateRectForInteractionRegion(const RenderElement& box, FloatRect& rect)
{
    if (nodeIsDateOrTimeRelatedInput(box.element())) {
        const auto cssBorderWidth = box.style().usedZoom();
        rect.inflate(cssBorderWidth);
        return { cssBorderWidth, cssBorderWidth };
    }

    return { 0, 0 };
}

float RenderThemeCocoa::adjustedMaximumLogicalWidthForControl(const RenderStyle& style, const Element& element, float maximumLogicalWidth) const
{
#if PLATFORM(MAC)
    if (!formControlRefreshEnabled(&element) || !style.hasUsedAppearance() || style.nativeAppearanceDisabled())
        return maximumLogicalWidth;

    const auto writingMode = style.writingMode();

    const auto inlineEndPaddingExplicitlySet = [&] {
        const auto isInlineFlipped = writingMode.isInlineFlipped();
        if (writingMode.isHorizontal())
            return isInlineFlipped ? style.hasExplicitlySetPaddingLeft() : style.hasExplicitlySetPaddingRight();

        return isInlineFlipped ? style.hasExplicitlySetPaddingTop() : style.hasExplicitlySetPaddingBottom();
    };

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(element); input && input->isNumberField() && !inlineEndPaddingExplicitlySet()) {
        const auto paddingBox = paddingBoxForNumberField(style, &element);
        const auto paddingEdgeInlineStart = paddingBox.start(writingMode);
        const auto paddingEdgeInlineEnd = paddingBox.end(writingMode);

        if (auto paddingEdgeInlineStartFixed = paddingEdgeInlineStart.tryFixed()) {
            if (auto paddingEdgeInlineEndFixed = paddingEdgeInlineEnd.tryFixed()) {
                auto usedZoom = style.usedZoomForLength();
                maximumLogicalWidth += paddingEdgeInlineStartFixed->resolveZoom(usedZoom) - paddingEdgeInlineEndFixed->resolveZoom(usedZoom);
            }
        }
    }
#else
    UNUSED_PARAM(style);
    UNUSED_PARAM(element);
#endif
    return maximumLogicalWidth;
}
#endif

void RenderThemeCocoa::adjustCheckboxStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (formControlRefreshEnabled(element))
        style.resetBorder();
#endif

    RenderTheme::adjustCheckboxStyle(style, element);
}

bool RenderThemeCocoa::paintCheckbox(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintCheckboxForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintCheckbox(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustRadioStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (formControlRefreshEnabled(element))
        style.resetBorder();
#endif

    RenderTheme::adjustRadioStyle(style, element);
}

bool RenderThemeCocoa::paintRadio(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintRadioForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintRadio(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustButtonStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustButtonStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustButtonStyle(style, element);
}

bool RenderThemeCocoa::paintButton(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintButtonForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintButton(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustColorWellStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustColorWellStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustColorWellStyle(style, element);
}

void RenderThemeCocoa::adjustColorWellSwatchStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustColorWellSwatchStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustColorWellSwatchStyle(style, element);
}

void RenderThemeCocoa::adjustColorWellSwatchOverlayStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustColorWellSwatchOverlayStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustColorWellSwatchOverlayStyle(style, element);
}

void RenderThemeCocoa::adjustColorWellSwatchWrapperStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustColorWellSwatchWrapperStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustColorWellSwatchWrapperStyle(style, element);
}

bool RenderThemeCocoa::paintColorWellSwatch(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintColorWellSwatchForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintColorWellSwatch(box, paintInfo, rect);
}

bool RenderThemeCocoa::paintColorWell(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintColorWellForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintColorWell(box, paintInfo, rect);
}

void RenderThemeCocoa::paintColorWellDecorations(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintColorWellDecorationsForVectorBasedControls(box, paintInfo, rect))
        return;
#endif

    RenderTheme::paintColorWellDecorations(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustInnerSpinButtonStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustInnerSpinButtonStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustInnerSpinButtonStyle(style, element);
}

bool RenderThemeCocoa::paintInnerSpinButton(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintInnerSpinButtonForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintInnerSpinButton(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustTextFieldStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustTextFieldStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustTextFieldStyle(style, element);
}

bool RenderThemeCocoa::paintTextField(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintTextFieldForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintTextField(box, paintInfo, rect);
}

void RenderThemeCocoa::paintTextFieldDecorations(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintTextFieldDecorationsForVectorBasedControls(box, paintInfo, rect))
        return;
#endif

    RenderTheme::paintTextFieldDecorations(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustTextAreaStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustTextAreaStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustTextAreaStyle(style, element);
}

bool RenderThemeCocoa::paintTextArea(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintTextAreaForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintTextArea(box, paintInfo, rect);
}

void RenderThemeCocoa::paintTextAreaDecorations(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintTextAreaDecorationsForVectorBasedControls(box, paintInfo, rect))
        return;
#endif

    RenderTheme::paintTextAreaDecorations(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustMenuListStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustMenuListStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustMenuListStyle(style, element);
}

bool RenderThemeCocoa::paintMenuList(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintMenuListForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintMenuList(box, paintInfo, rect);
}

void RenderThemeCocoa::paintMenuListDecorations(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintMenuListDecorationsForVectorBasedControls(box, paintInfo, rect))
        return;
#endif

    RenderTheme::paintMenuListDecorations(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustMenuListButtonStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustMenuListButtonStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustMenuListButtonStyle(style, element);
}

void RenderThemeCocoa::paintMenuListButtonDecorations(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintMenuListButtonDecorationsForVectorBasedControls(box, paintInfo, rect))
        return;
#endif

    RenderTheme::paintMenuListButtonDecorations(box, paintInfo, rect);
}

bool RenderThemeCocoa::paintMenuListButton(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintMenuListButtonForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintMenuListButton(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustMeterStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustMeterStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustMeterStyle(style, element);
}

bool RenderThemeCocoa::paintMeter(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintMeterForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintMeter(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustListButtonStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustListButtonStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustListButtonStyle(style, element);
}

bool RenderThemeCocoa::paintListButton(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintListButtonForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintListButton(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustProgressBarStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustProgressBarStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustProgressBarStyle(style, element);
}

bool RenderThemeCocoa::paintProgressBar(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintProgressBarForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintProgressBar(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustSliderTrackStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustSliderTrackStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustSliderTrackStyle(style, element);
}

bool RenderThemeCocoa::paintSliderTrack(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintSliderTrackForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintSliderTrack(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustSliderThumbSize(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustSliderThumbSizeForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustSliderThumbSize(style, element);
}

void RenderThemeCocoa::adjustSliderThumbStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustSliderThumbStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustSliderThumbStyle(style, element);
}

bool RenderThemeCocoa::paintSliderThumb(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintSliderThumbForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintSliderThumb(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustSearchFieldStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustSearchFieldStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustSearchFieldStyle(style, element);
}

bool RenderThemeCocoa::paintSearchField(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintSearchFieldForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintSearchField(box, paintInfo, rect);
}

void RenderThemeCocoa::paintSearchFieldDecorations(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintSearchFieldDecorationsForVectorBasedControls(box, paintInfo, rect))
        return;
#endif

    RenderTheme::paintSearchFieldDecorations(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustSearchFieldCancelButtonStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustSearchFieldCancelButtonStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustSearchFieldCancelButtonStyle(style, element);
}

bool RenderThemeCocoa::paintSearchFieldCancelButton(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintSearchFieldCancelButtonForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintSearchFieldCancelButton(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustSearchFieldDecorationPartStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustSearchFieldDecorationPartStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustSearchFieldDecorationPartStyle(style, element);
}

bool RenderThemeCocoa::paintSearchFieldDecorationPart(const RenderElement& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintSearchFieldDecorationPartForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintSearchFieldDecorationPart(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustSearchFieldResultsDecorationPartStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustSearchFieldResultsDecorationPartStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustSearchFieldResultsDecorationPartStyle(style, element);
}

bool RenderThemeCocoa::paintSearchFieldResultsDecorationPart(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintSearchFieldResultsDecorationPartForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintSearchFieldResultsDecorationPart(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustSearchFieldResultsButtonStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustSearchFieldResultsButtonStyleForVectorBasedControls(style, element))
        return;
#endif

    RenderTheme::adjustSearchFieldResultsButtonStyle(style, element);
}

bool RenderThemeCocoa::paintSearchFieldResultsButton(const RenderBox& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintSearchFieldResultsButtonForVectorBasedControls(box, paintInfo, rect))
        return false;
#endif

    return RenderTheme::paintSearchFieldResultsButton(box, paintInfo, rect);
}

void RenderThemeCocoa::adjustSwitchStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
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

    if (style.outlineStyle() == OutlineStyle::Auto)
        style.setOutlineStyle(OutlineStyle::None);
#endif
}

bool RenderThemeCocoa::paintSwitchThumb(const RenderElement& renderer, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if PLATFORM(MAC)
    bool useDefaultImplementation = true;
#if ENABLE(FORM_CONTROL_REFRESH)
    if (renderer.settings().formControlRefreshEnabled())
        useDefaultImplementation = false;
#endif
    if (useDefaultImplementation)
        return RenderTheme::paintSwitchThumb(renderer, paintInfo, rect);
#endif

    return renderThemePaintSwitchThumb(extractControlStyleStatesForRenderer(renderer), renderer, paintInfo, rect, platformFocusRingColor(renderer.styleColorOptions()));
}

bool RenderThemeCocoa::paintSwitchTrack(const RenderElement& renderer, const PaintInfo& paintInfo, const FloatRect& rect)
{
#if PLATFORM(MAC)
    bool useDefaultImplementation = true;
#if ENABLE(FORM_CONTROL_REFRESH)
    if (renderer.settings().formControlRefreshEnabled())
        useDefaultImplementation = false;
#endif
    if (useDefaultImplementation)
        return RenderTheme::paintSwitchTrack(renderer, paintInfo, rect);
#endif

    return renderThemePaintSwitchTrack(extractControlStyleStatesForRenderer(renderer), renderer, paintInfo, rect);
}

void RenderThemeCocoa::paintPlatformResizer(const RenderLayerModelObject& renderer, GraphicsContext& context, const LayoutRect& resizerCornerRect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintPlatformResizerForVectorBasedControls(renderer, context, resizerCornerRect))
        return;
#endif
    RenderTheme::paintPlatformResizer(renderer, context, resizerCornerRect);
}

void RenderThemeCocoa::paintPlatformResizerFrame(const RenderLayerModelObject& renderer, GraphicsContext& context, const LayoutRect& resizerCornerRect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (paintPlatformResizerFrameForVectorBasedControls(renderer, context, resizerCornerRect))
        return;
#endif
    RenderTheme::paintPlatformResizerFrame(renderer, context, resizerCornerRect);
}

bool RenderThemeCocoa::supportsFocusRing(const RenderElement& renderer, const RenderStyle& style) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    auto tryFocusRingForVectorBasedControls = renderer.settings().formControlRefreshEnabled();
    if (tryFocusRingForVectorBasedControls)
        return supportsFocusRingForVectorBasedControls(renderer, style);
#endif

    return RenderTheme::supportsFocusRing(renderer, style);
}

void RenderThemeCocoa::adjustTextControlInnerContainerStyle(RenderStyle& style, const RenderStyle& shadowHostStyle, const Element* shadowHost) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustTextControlInnerContainerStyleForVectorBasedControls(style, shadowHostStyle, shadowHost))
        return;
#endif

    RenderTheme::adjustTextControlInnerContainerStyle(style, shadowHostStyle, shadowHost);
}

void RenderThemeCocoa::adjustTextControlInnerPlaceholderStyle(RenderStyle& style, const RenderStyle& shadowHostStyle, const Element* shadowHost) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustTextControlInnerPlaceholderStyleForVectorBasedControls(style, shadowHostStyle, shadowHost))
        return;
#endif

    RenderTheme::adjustTextControlInnerPlaceholderStyle(style, shadowHostStyle, shadowHost);
}

void RenderThemeCocoa::adjustTextControlInnerTextStyle(RenderStyle& style, const RenderStyle& shadowHostStyle, const Element* shadowHost) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (adjustTextControlInnerTextStyleForVectorBasedControls(style, shadowHostStyle, shadowHost))
        return;
#endif

    RenderTheme::adjustTextControlInnerTextStyle(style, shadowHostStyle, shadowHost);
}

bool RenderThemeCocoa::isSubmitStyleButton(const Node* node) const
{
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(node))
        return input->isSubmitButton();

    if (RefPtr button = dynamicDowncast<HTMLButtonElement>(node))
        return button->isExplicitlySetSubmitButton();

    return false;
}

}
