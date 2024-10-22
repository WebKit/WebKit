/*
 * Copyright (C) 2014, 2020 Igalia S.L.
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
#include "RenderObject.h"
#include "RenderProgress.h"
#include "RenderStyleSetters.h"
#include "ThemeAdwaita.h"
#include "TimeRanges.h"
#include "UserAgentStyleSheets.h"
#include <wtf/text/Base64.h>

#if PLATFORM(WIN)
#include "WebCoreBundleWin.h"
#include <wtf/FileSystem.h>
#endif

#if ENABLE(MODERN_MEDIA_CONTROLS)
#include "UserAgentScripts.h"
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#include "SystemSettings.h"
#endif

namespace WebCore {
using namespace WebCore::Adwaita;

RenderTheme& RenderTheme::singleton()
{
    static MainThreadNeverDestroyed<RenderThemeAdwaita> theme;
    return theme;
}

RenderThemeAdwaita::~RenderThemeAdwaita() = default;

bool RenderThemeAdwaita::canCreateControlPartForRenderer(const RenderObject& renderer) const
{
    switch (renderer.style().usedAppearance()) {
    case StyleAppearance::Button:
    case StyleAppearance::Checkbox:
#if ENABLE(INPUT_TYPE_COLOR)
    case StyleAppearance::ColorWell:
#endif
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

bool RenderThemeAdwaita::canCreateControlPartForBorderOnly(const RenderObject& renderer) const
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

bool RenderThemeAdwaita::canCreateControlPartForDecorations(const RenderObject& renderer) const
{
    return renderer.style().usedAppearance() == StyleAppearance::MenulistButton;
}

bool RenderThemeAdwaita::supportsFocusRing(const RenderStyle& style) const
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
#if ENABLE(MODERN_MEDIA_CONTROLS)
    return { StringImpl::createWithoutCopying(ModernMediaControlsJavaScript) };
#else
    return { };
#endif
}

String RenderThemeAdwaita::mediaControlsStyleSheet()
{
#if ENABLE(MODERN_MEDIA_CONTROLS)
    if (m_mediaControlsStyleSheet.isEmpty())
        m_mediaControlsStyleSheet = StringImpl::createWithoutCopying(ModernMediaControlsUserAgentStyleSheet);
    return m_mediaControlsStyleSheet;
#else
    return emptyString();
#endif
}

#if ENABLE(MODERN_MEDIA_CONTROLS)

String RenderThemeAdwaita::mediaControlsBase64StringForIconNameAndType(const String& iconName, const String& iconType)
{
#if USE(GLIB)
    auto path = makeString("/org/webkit/media-controls/"_s, iconName, '.', iconType);
    auto data = adoptGRef(g_resources_lookup_data(path.latin1().data(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr));
    if (!data)
        return emptyString();
    return base64EncodeToString({ static_cast<const uint8_t*>(g_bytes_get_data(data.get(), nullptr)), g_bytes_get_size(data.get()) });
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
#endif // ENABLE(MODERN_MEDIA_CONTROLS)
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
#if HAVE(OS_DARK_MODE_SUPPORT)
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

bool RenderThemeAdwaita::isControlStyled(const RenderStyle& style, const RenderStyle& userAgentStyle) const
{
    auto appearance = style.usedAppearance();
    if (appearance == StyleAppearance::TextField || appearance == StyleAppearance::TextArea || appearance == StyleAppearance::SearchField || appearance == StyleAppearance::Listbox)
        return style.border() != userAgentStyle.border();

    return RenderTheme::isControlStyled(style, userAgentStyle);
}

void RenderThemeAdwaita::adjustTextFieldStyle(RenderStyle& style, const Element*) const
{
    if (!style.hasExplicitlySetBorderRadius())
        style.setBorderRadius(IntSize(5, 5));
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
    style.setLineHeight(RenderStyle::initialLineHeight());
}

void RenderThemeAdwaita::adjustMenuListButtonStyle(RenderStyle& style, const Element* element) const
{
    adjustMenuListStyle(style, element);
}

LengthBox RenderThemeAdwaita::popupInternalPaddingBox(const RenderStyle& style) const
{
    if (style.usedAppearance() == StyleAppearance::None)
        return { };

    auto zoomedArrowSize = menuListButtonArrowSize * style.usedZoom();
    int leftPadding = menuListButtonPadding + (style.writingMode().isBidiRTL() ? zoomedArrowSize : 0);
    int rightPadding = menuListButtonPadding + (style.writingMode().isBidiLTR() ? zoomedArrowSize : 0);

    return { menuListButtonPadding, rightPadding, menuListButtonPadding, leftPadding };
}

Seconds RenderThemeAdwaita::animationRepeatIntervalForProgressBar(const RenderProgress& renderer) const
{
    return renderer.page().preferredRenderingUpdateInterval();
}

IntRect RenderThemeAdwaita::progressBarRectForBounds(const RenderProgress&, const IntRect& bounds) const
{
    return { bounds.x(), bounds.y(), bounds.width(), progressBarSize };
}

void RenderThemeAdwaita::adjustSliderThumbSize(RenderStyle& style, const Element*) const
{
    auto appearance = style.usedAppearance();
    if (appearance != StyleAppearance::SliderThumbHorizontal && appearance != StyleAppearance::SliderThumbVertical)
        return;

    style.setWidth(Length(sliderThumbSize, LengthType::Fixed));
    style.setHeight(Length(sliderThumbSize, LengthType::Fixed));
}

#if ENABLE(DATALIST_ELEMENT)
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
    // Add a margin to place the button at end of the input field.
    if (style.isLeftToRightDirection())
        style.setMarginRight(Length(-2, LengthType::Fixed));
    else
        style.setMarginLeft(Length(-2, LengthType::Fixed));
}
#endif // ENABLE(DATALIST_ELEMENT)

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
