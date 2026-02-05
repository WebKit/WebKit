/*
 * Copyright (C) 2014, 2020 Igalia S.L.
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

#include "config.h"
#include "RenderThemeAdwaita.h"

#if USE(THEME_ADWAITA)

#include "Adwaita.h"
#include "Color.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "MediaControlTextTrackContainerElement.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderBox.h"
#include "RenderObjectInlines.h"
#include "RenderProgress.h"
#include "RenderStyle+GettersInlines.h"
#include "RenderStyle+SettersInlines.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePadding.h"
#include "ThemeAdwaita.h"
#include "TimeRanges.h"
#include "UserAgentScripts.h"
#include "UserAgentStyleSheets.h"
#include <wtf/text/Base64.h>

#if PLATFORM(WIN)
#include "WebCoreBundleWin.h"
#include <wtf/FileSystem.h>
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#include "SystemSettings.h"
#endif

#if USE(GLIB)
#include <wtf/glib/GSpanExtras.h>
#endif

namespace WebCore {
using namespace CSS::Literals;
using namespace WebCore::Adwaita;

RenderThemeAdwaita& RenderTheme::singleton()
{
    static MainThreadNeverDestroyed<RenderThemeAdwaita> theme;
    return theme;
}

RenderThemeAdwaita::~RenderThemeAdwaita() = default;

bool RenderThemeAdwaita::canCreateControlPartForRenderer(const RenderElement& renderer) const
{
    switch (renderer.style().usedAppearance()) {
    case StyleAppearance::Button:
    case StyleAppearance::Checkbox:
    case StyleAppearance::ColorWell:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::InnerSpinButton:
    case StyleAppearance::Menulist:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::PushButton:
    case StyleAppearance::Radio:
    case StyleAppearance::SearchField:
    case StyleAppearance::SliderThumbHorizontal:
    case StyleAppearance::SliderThumbVertical:
    case StyleAppearance::SliderHorizontal:
    case StyleAppearance::SliderVertical:
    case StyleAppearance::SquareButton:
        return true;
    default:
        break;
    }
    return false;
}

bool RenderThemeAdwaita::canCreateControlPartForBorderOnly(const RenderElement& renderer) const
{
    switch (renderer.style().usedAppearance()) {
    case StyleAppearance::Listbox:
    case StyleAppearance::TextArea:
    case StyleAppearance::TextField:
        return true;
    default:
        break;
    }
    return false;
}

bool RenderThemeAdwaita::canCreateControlPartForDecorations(const RenderElement& renderer) const
{
    return renderer.style().usedAppearance() == StyleAppearance::MenulistButton;
}

bool RenderThemeAdwaita::supportsFocusRing(const RenderElement&, const RenderStyle& style) const
{
    switch (style.usedAppearance()) {
    case StyleAppearance::PushButton:
    case StyleAppearance::Button:
    case StyleAppearance::TextField:
    case StyleAppearance::TextArea:
    case StyleAppearance::SearchField:
    case StyleAppearance::Menulist:
    case StyleAppearance::Radio:
    case StyleAppearance::Checkbox:
    case StyleAppearance::SliderHorizontal:
    case StyleAppearance::SliderVertical:
        return true;
    default:
        break;
    }

    return false;
}

bool RenderThemeAdwaita::shouldHaveCapsLockIndicator(const HTMLInputElement& element) const
{
    return element.isPasswordField();
}

Color RenderThemeAdwaita::platformActiveSelectionBackgroundColor(OptionSet<StyleColorOptions>) const
{
    return systemAccentColor().colorWithAlphaMultipliedBy(0.3);
}

Color RenderThemeAdwaita::platformInactiveSelectionBackgroundColor(OptionSet<StyleColorOptions> options) const
{
    if (options.contains(StyleColorOptions::UseDarkAppearance))
        return SRGBA<uint8_t> { 255, 255, 255, 25 };

    return SRGBA<uint8_t> { 0, 0, 0, 25 };
}

Color RenderThemeAdwaita::platformActiveSelectionForegroundColor(OptionSet<StyleColorOptions>) const
{
    return { };
}

Color RenderThemeAdwaita::platformInactiveSelectionForegroundColor(OptionSet<StyleColorOptions>) const
{
    return { };
}

Color RenderThemeAdwaita::platformActiveListBoxSelectionBackgroundColor(OptionSet<StyleColorOptions> options) const
{
    return platformActiveListBoxSelectionForegroundColor(options).colorWithAlpha(0.15);
}

Color RenderThemeAdwaita::platformInactiveListBoxSelectionBackgroundColor(OptionSet<StyleColorOptions> options) const
{
    return platformInactiveListBoxSelectionForegroundColor(options).colorWithAlpha(0.15);
}

Color RenderThemeAdwaita::platformActiveListBoxSelectionForegroundColor(OptionSet<StyleColorOptions> options) const
{
    return options.contains(StyleColorOptions::UseDarkAppearance) ? Color::white : Color::black;
}

Color RenderThemeAdwaita::platformInactiveListBoxSelectionForegroundColor(OptionSet<StyleColorOptions> options) const
{
    return platformActiveListBoxSelectionForegroundColor(options);
}

Color RenderThemeAdwaita::platformFocusRingColor(OptionSet<StyleColorOptions>) const
{
    return systemFocusRingColor();
}

void RenderThemeAdwaita::platformColorsDidChange()
{
    static_cast<ThemeAdwaita&>(Theme::singleton()).platformColorsDidChange();
    RenderTheme::platformColorsDidChange();
}

String RenderThemeAdwaita::extraDefaultStyleSheet()
{
    return StringImpl::createWithoutCopying(themeAdwaitaUserAgentStyleSheet);
}

#if ENABLE(VIDEO)

Vector<String, 2> RenderThemeAdwaita::mediaControlsScripts()
{
    return { StringImpl::createWithoutCopying(ModernMediaControlsJavaScript) };
}

Vector<String, 2> RenderThemeAdwaita::mediaControlsStyleSheets(const HTMLMediaElement&)
{
    if (m_mediaControlsStyleSheet.isEmpty())
        m_mediaControlsStyleSheet = StringImpl::createWithoutCopying(ModernMediaControlsUserAgentStyleSheet);
    return { m_mediaControlsStyleSheet };
}

RefPtr<FragmentedSharedBuffer> RenderThemeAdwaita::mediaControlsImageDataForIconNameAndType(const String& iconName, const String& iconType)
{
#if USE(GLIB)
    auto path = makeString("/org/webkit/media-controls/"_s, iconName, '.', iconType);
    auto data = adoptGRef(g_resources_lookup_data(path.latin1().data(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr));
    if (!data)
        return nullptr;
    return SharedBuffer::create(span(data));
#elif PLATFORM(WIN)
    auto path = webKitBundlePath(iconName, iconType, "media-controls"_s);
    auto data = FileSystem::readEntireFile(path);
    if (!data)
        return nullptr;
    return SharedBuffer::create(WTF::move(*data));
#else
    return nullptr;
#endif
}

String RenderThemeAdwaita::mediaControlsBase64StringForIconNameAndType(const String& iconName, const String& iconType)
{
#if USE(GLIB)
    auto path = makeString("/org/webkit/media-controls/"_s, iconName, '.', iconType);
    auto data = adoptGRef(g_resources_lookup_data(path.latin1().data(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr));
    if (!data)
        return emptyString();
    return base64EncodeToString(span(data));
#elif PLATFORM(WIN)
    auto path = webKitBundlePath(iconName, iconType, "media-controls"_s);
    auto data = FileSystem::readEntireFile(path);
    if (!data)
        return { };
    return base64EncodeToString(data->span());
#else
    return { };
#endif
}

String RenderThemeAdwaita::mediaControlsFormattedStringForDuration(double durationInSeconds)
{
    // FIXME: Format this somehow, maybe through GDateTime?
    return makeString(durationInSeconds);
}
#endif // ENABLE(VIDEO)

Color RenderThemeAdwaita::systemColor(CSSValueID cssValueID, OptionSet<StyleColorOptions> options) const
{
    const bool useDarkAppearance = options.contains(StyleColorOptions::UseDarkAppearance);

    switch (cssValueID) {
    case CSSValueActivebuttontext:
    case CSSValueButtontext:
        if (useDarkAppearance)
            return { buttonTextColorDark, Color::Flags::Semantic };
        return { buttonTextColorLight, Color::Flags::Semantic };

    case CSSValueGraytext:
        if (useDarkAppearance)
            return { buttonTextDisabledColorDark, Color::Flags::Semantic };
        return { buttonTextDisabledColorLight, Color::Flags::Semantic };

    case CSSValueCanvas:
        if (useDarkAppearance)
            return { SRGBA<uint8_t> { 30, 30, 30 }, Color::Flags::Semantic };
        return { Color::white, Color::Flags::Semantic };

    case CSSValueField:
#if PLATFORM(COCOA)
    case CSSValueWebkitControlBackground:
#endif
        if (useDarkAppearance)
            return { textFieldBackgroundColorDark, Color::Flags::Semantic };
        return { textFieldBackgroundColorLight, Color::Flags::Semantic };

    case CSSValueCanvastext:
    case CSSValueFieldtext:
    case CSSValueText:
        if (useDarkAppearance)
            return { Color::white, Color::Flags::Semantic };
        return { Color::black, Color::Flags::Semantic };

    case CSSValueHighlight:
        // Hardcoded to avoid exposing a user appearance preference to the web for fingerprinting.
        return { SRGBA<uint8_t> { 52, 132, 228 }, Color::Flags::Semantic };

    case CSSValueHighlighttext:
        return { Color::white, Color::Flags::Semantic };

    default:
        return RenderTheme::systemColor(cssValueID, options);
    }
}

bool RenderThemeAdwaita::isControlStyled(const RenderStyle& style) const
{
    auto appearance = style.usedAppearance();
    if (appearance == StyleAppearance::TextField || appearance == StyleAppearance::TextArea || appearance == StyleAppearance::SearchField || appearance == StyleAppearance::Listbox)
        return style.nativeAppearanceDisabled();

    return RenderTheme::isControlStyled(style);
}

void RenderThemeAdwaita::adjustTextFieldStyle(RenderStyle& style, const Element*) const
{
    if (!style.hasExplicitlySetBorderRadius())
        style.setBorderRadius({ 5_css_px, 5_css_px });
}

void RenderThemeAdwaita::adjustTextAreaStyle(RenderStyle& style, const Element* element) const
{
    adjustTextFieldStyle(style, element);
}

void RenderThemeAdwaita::adjustSearchFieldStyle(RenderStyle& style, const Element* element) const
{
    adjustTextFieldStyle(style, element);
}

void RenderThemeAdwaita::adjustMenuListStyle(RenderStyle& style, const Element* element) const
{
    RenderTheme::adjustMenuListStyle(style, element);
    style.setLineHeight(Style::ComputedStyle::initialLineHeight());
}

void RenderThemeAdwaita::adjustMenuListButtonStyle(RenderStyle& style, const Element* element) const
{
    adjustMenuListStyle(style, element);
}

Style::PaddingBox RenderThemeAdwaita::popupInternalPaddingBox(const RenderStyle& style) const
{
    if (style.usedAppearance() == StyleAppearance::None)
        return { 0_css_px };

    auto zoomedArrowSize = menuListButtonArrowSize * style.usedZoom();
    int leftPadding = menuListButtonPadding + (style.writingMode().isBidiRTL() ? zoomedArrowSize : 0);
    int rightPadding = menuListButtonPadding + (style.writingMode().isBidiLTR() ? zoomedArrowSize : 0);

    return {
        Style::PaddingEdge::Fixed { static_cast<float>(menuListButtonPadding) },
        Style::PaddingEdge::Fixed { static_cast<float>(rightPadding) },
        Style::PaddingEdge::Fixed { static_cast<float>(menuListButtonPadding) },
        Style::PaddingEdge::Fixed { static_cast<float>(leftPadding) },
    };
}

Seconds RenderThemeAdwaita::animationRepeatIntervalForProgressBar(const RenderProgress& renderer) const
{
    return renderer.page().preferredRenderingUpdateInterval();
}

IntRect RenderThemeAdwaita::progressBarRectForBounds(const RenderProgress& renderer, const IntRect& bounds) const
{
    bool isHorizontal = renderer.isHorizontalWritingMode();
    return { bounds.x(), bounds.y(), isHorizontal ? bounds.width() : progressBarSize, isHorizontal ? progressBarSize : bounds.height() };
}

void RenderThemeAdwaita::adjustSliderThumbSize(RenderStyle& style, const Element*) const
{
    auto appearance = style.usedAppearance();
    if (appearance != StyleAppearance::SliderThumbHorizontal && appearance != StyleAppearance::SliderThumbVertical)
        return;

    style.setWidth(Style::PreferredSize::Fixed { static_cast<float>(sliderThumbSize) });
    style.setHeight(Style::PreferredSize::Fixed { static_cast<float>(sliderThumbSize) });
}

IntSize RenderThemeAdwaita::sliderTickSize() const
{
    return { 1, 7 };
}

int RenderThemeAdwaita::sliderTickOffsetFromTrackCenter() const
{
    return -16;
}

void RenderThemeAdwaita::adjustListButtonStyle(RenderStyle& style, const Element*) const
{
    style.setLogicalWidth(16_css_px);
    // Add a margin to place the button at end of the input field.
    if (style.isLeftToRightDirection())
        style.setMarginRight(-2_css_px);
    else
        style.setMarginLeft(-2_css_px);
}

Style::PreferredSizePair RenderThemeAdwaita::controlSize(StyleAppearance appearance, const FontCascade& fontCascade, const Style::PreferredSizePair& zoomedSize, float zoomFactor) const
{
    if (!zoomedSize.width().isIntrinsicOrLegacyIntrinsicOrAuto() && !zoomedSize.height().isIntrinsicOrLegacyIntrinsicOrAuto())
        return RenderTheme::controlSize(appearance, fontCascade, zoomedSize, zoomFactor);

    switch (appearance) {
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio: {
        auto buttonSizeWidth = zoomedSize.width();
        auto buttonSizeHeight = zoomedSize.height();
        if (buttonSizeWidth.isIntrinsicOrLegacyIntrinsicOrAuto())
            buttonSizeWidth = 12_css_px * zoomFactor;
        if (buttonSizeHeight.isIntrinsicOrLegacyIntrinsicOrAuto())
            buttonSizeHeight = 12_css_px * zoomFactor;
        return { WTF::move(buttonSizeWidth), WTF::move(buttonSizeHeight) };
    }
    case StyleAppearance::InnerSpinButton: {
        auto spinButtonSizeWidth = zoomedSize.width();
        auto spinButtonSizeHeight = zoomedSize.height();
        if (spinButtonSizeWidth.isIntrinsicOrLegacyIntrinsicOrAuto())
            spinButtonSizeWidth = Style::PreferredSize::Fixed { static_cast<float>(static_cast<int>(arrowSize * zoomFactor)) };
        if (spinButtonSizeHeight.isIntrinsicOrLegacyIntrinsicOrAuto() || fontCascade.size() > arrowSize)
            spinButtonSizeHeight = Style::PreferredSize::Fixed { fontCascade.size() };
        return { WTF::move(spinButtonSizeWidth), WTF::move(spinButtonSizeHeight) };
    }
    default:
        break;
    }

    return RenderTheme::controlSize(appearance, fontCascade, zoomedSize, zoomFactor);
}

Style::MinimumSizePair RenderThemeAdwaita::minimumControlSize(StyleAppearance, const FontCascade&, const Style::MinimumSizePair& zoomedSize, float) const
{
    if (!zoomedSize.width().isIntrinsicOrLegacyIntrinsicOrAuto() && !zoomedSize.height().isIntrinsicOrLegacyIntrinsicOrAuto())
        return zoomedSize;

    auto resultWidth = zoomedSize.width();
    auto resultHeight = zoomedSize.height();

    if (resultWidth.isIntrinsicOrLegacyIntrinsicOrAuto())
        resultWidth = 0_css_px;
    if (resultHeight.isIntrinsicOrLegacyIntrinsicOrAuto())
        resultHeight = 0_css_px;

    return { WTF::move(resultWidth), WTF::move(resultHeight) };
}

Style::LineWidthBox RenderThemeAdwaita::controlBorder(StyleAppearance appearance, const FontCascade& font, const Style::LineWidthBox& zoomedBox, float zoomFactor, const Element* element) const
{
    switch (appearance) {
    case StyleAppearance::PushButton:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button:
    case StyleAppearance::SquareButton:
        return zoomedBox;
    default:
        break;
    }

    return RenderTheme::controlBorder(appearance, font, zoomedBox, zoomFactor, element);
}

#if PLATFORM(GTK) || PLATFORM(WPE)
std::optional<Seconds> RenderThemeAdwaita::caretBlinkInterval() const
{
    auto shouldBlink = SystemSettings::singleton().cursorBlink();
    auto blinkTime = SystemSettings::singleton().cursorBlinkTime();
    if (shouldBlink.value_or(true))
        return { 500_us * blinkTime.value_or(1200) };
    return { };
}
#endif // PLATFORM(GTK) || PLATFORM(WPE)

void RenderThemeAdwaita::setAccentColor(const Color& color)
{
    static_cast<ThemeAdwaita&>(Theme::singleton()).setAccentColor(color);
    platformColorsDidChange();
}

} // namespace WebCore

#endif // USE(THEME_ADWAITA)
