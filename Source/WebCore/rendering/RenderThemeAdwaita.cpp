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

#include "Color.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "MediaControlTextTrackContainerElement.h"
#include "PaintInfo.h"
#include "RenderBox.h"
#include "RenderObject.h"
#include "RenderProgress.h"
#include "RenderStyle.h"
#include "ThemeAdwaita.h"
#include "TimeRanges.h"
#include "UserAgentScripts.h"
#include "UserAgentStyleSheets.h"
#include <wtf/text/Base64.h>

#if PLATFORM(GTK)
#include <gtk/gtk.h>
#endif

#if PLATFORM(WIN)
#include "WebCoreBundleWin.h"
#include <wtf/FileSystem.h>
#endif

namespace WebCore {

static const double focusRingOpacity = 0.8; // Keep in sync with focusRingOpacity in ThemeAdwaita.
static const double disabledOpacity = 0.5; // Keep in sync with disabledOpacity in ThemeAdwaita.
static const int textFieldBorderSize = 1;
static constexpr auto textFieldBorderColorLight = SRGBA<uint8_t> { 0, 0, 0, 50 };
static constexpr auto textFieldBackgroundColorLight = Color::white;

static constexpr auto textFieldBorderColorDark = SRGBA<uint8_t> { 255, 255, 255, 50 };
static constexpr auto textFieldBackgroundColorDark = SRGBA<uint8_t> { 45, 45, 45 };

static const unsigned menuListButtonArrowSize = 16;
static const int menuListButtonFocusOffset = -2;
static const unsigned menuListButtonPadding = 5;
static const int menuListButtonBorderSize = 1; // Keep in sync with buttonBorderSize in ThemeAdwaita.
static const unsigned progressActivityBlocks = 5;
static const unsigned progressAnimationFrameCount = 75;
static const Seconds progressAnimationFrameRate = 33_ms; // 30fps.
static const unsigned progressBarSize = 6;
static constexpr auto progressBarBackgroundColorLight = SRGBA<uint8_t> { 0, 0, 0, 40 };
static constexpr auto progressBarBackgroundColorDark = SRGBA<uint8_t> { 255, 255, 255, 30 };
static const unsigned sliderTrackSize = 6;
static constexpr auto sliderTrackBackgroundColorLight = SRGBA<uint8_t> { 0, 0, 0, 40 };
static constexpr auto sliderTrackBackgroundColorDark = SRGBA<uint8_t> { 255, 255, 255, 30 };
static const int sliderTrackFocusOffset = 2;
static const int sliderThumbSize = 20;
static const int sliderThumbBorderSize = 1;
static constexpr auto sliderThumbBorderColorLight = SRGBA<uint8_t>  { 0, 0, 0, 50 };
static constexpr auto sliderThumbBackgroundColorLight = Color::white;
static constexpr auto sliderThumbBackgroundHoveredColorLight = SRGBA<uint8_t> { 244, 244, 244 };
static constexpr auto sliderThumbBackgroundDisabledColorLight = SRGBA<uint8_t> { 244, 244, 244 };

static constexpr auto sliderThumbBorderColorDark = SRGBA<uint8_t>  { 0, 0, 0, 50 };
static constexpr auto sliderThumbBackgroundColorDark = SRGBA<uint8_t> { 210, 210, 210 };
static constexpr auto sliderThumbBackgroundHoveredColorDark = SRGBA<uint8_t> { 230, 230, 230 };
static constexpr auto sliderThumbBackgroundDisabledColorDark = SRGBA<uint8_t> { 150, 150, 150 };

#if ENABLE(VIDEO)
static constexpr auto mediaSliderTrackBackgroundcolor = SRGBA<uint8_t> { 77, 77, 77 };
static constexpr auto mediaSliderTrackBufferedColor = SRGBA<uint8_t> { 173, 173, 173 };
static constexpr auto mediaSliderTrackActiveColor = SRGBA<uint8_t> { 252, 252, 252 };
#endif

static constexpr auto buttonTextColorLight = SRGBA<uint8_t> { 0, 0, 0, 204 };
static constexpr auto buttonTextDisabledColorLight = SRGBA<uint8_t> { 0, 0, 0, 102 };
static constexpr auto buttonTextColorDark = SRGBA<uint8_t> { 255, 255, 255 };
static constexpr auto buttonTextDisabledColorDark = SRGBA<uint8_t> { 255, 255, 255, 127 };

static inline Color getSystemAccentColor()
{
    return static_cast<ThemeAdwaita&>(Theme::singleton()).accentColor();
}

static inline Color getAccentColor(const RenderObject& renderObject)
{
    if (!renderObject.style().hasAutoAccentColor())
        return renderObject.style().effectiveAccentColor();

    return getSystemAccentColor();
}

#if !PLATFORM(GTK)
RenderTheme& RenderTheme::singleton()
{
    static MainThreadNeverDestroyed<RenderThemeAdwaita> theme;
    return theme;
}
#endif

bool RenderThemeAdwaita::supportsFocusRing(const RenderStyle& style) const
{
    switch (style.effectiveAppearance()) {
    case ControlPartType::PushButton:
    case ControlPartType::Button:
    case ControlPartType::TextField:
    case ControlPartType::TextArea:
    case ControlPartType::SearchField:
    case ControlPartType::Menulist:
    case ControlPartType::Radio:
    case ControlPartType::Checkbox:
    case ControlPartType::SliderHorizontal:
    case ControlPartType::SliderVertical:
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
    return getSystemAccentColor().colorWithAlphaMultipliedBy(0.3);
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
    return getSystemAccentColor().colorWithAlphaMultipliedBy(focusRingOpacity);
}

void RenderThemeAdwaita::platformColorsDidChange()
{
    static_cast<ThemeAdwaita&>(Theme::singleton()).platformColorsDidChange();
    RenderTheme::platformColorsDidChange();
}

String RenderThemeAdwaita::extraDefaultStyleSheet()
{
    return StringImpl::createWithoutCopying(themeAdwaitaUserAgentStyleSheet, sizeof(themeAdwaitaUserAgentStyleSheet));
}

#if ENABLE(VIDEO)

Vector<String, 2> RenderThemeAdwaita::mediaControlsScripts()
{
#if ENABLE(MODERN_MEDIA_CONTROLS)
    return { StringImpl::createWithoutCopying(ModernMediaControlsJavaScript, sizeof(ModernMediaControlsJavaScript)) };
#else
    return { };
#endif
}

String RenderThemeAdwaita::mediaControlsStyleSheet()
{
#if ENABLE(MODERN_MEDIA_CONTROLS)
    if (m_mediaControlsStyleSheet.isEmpty())
        m_mediaControlsStyleSheet = StringImpl::createWithoutCopying(ModernMediaControlsUserAgentStyleSheet, sizeof(ModernMediaControlsUserAgentStyleSheet));
    return m_mediaControlsStyleSheet;
#else
    return emptyString();
#endif
}

#if ENABLE(MODERN_MEDIA_CONTROLS)

String RenderThemeAdwaita::mediaControlsBase64StringForIconNameAndType(const String& iconName, const String& iconType)
{
#if USE(GLIB)
    auto path = makeString("/org/webkit/media-controls/", iconName, '.', iconType);
    auto data = adoptGRef(g_resources_lookup_data(path.latin1().data(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr));
    if (!data)
        return emptyString();
    return base64EncodeToString(g_bytes_get_data(data.get(), nullptr), g_bytes_get_size(data.get()));
#elif PLATFORM(WIN)
    auto path = webKitBundlePath(iconName, iconType, "media-controls"_s);
    auto data = FileSystem::readEntireFile(path);
    if (!data)
        return { };
    return base64EncodeToString(data->data(), data->size());
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
    case CSSValueActivecaption:
    case CSSValueActivebuttontext:
    case CSSValueButtontext:
        return useDarkAppearance ? buttonTextColorDark : buttonTextColorLight;

    case CSSValueGraytext:
        return useDarkAppearance ? buttonTextDisabledColorDark : buttonTextDisabledColorLight;

    case CSSValueCanvas:
    case CSSValueField:
    case CSSValueWebkitControlBackground:
        return useDarkAppearance ? textFieldBackgroundColorDark : textFieldBackgroundColorLight;

    case CSSValueWindow:
        return useDarkAppearance ? SRGBA<uint8_t> { 30, 30, 30 } : Color::white;

    case CSSValueCanvastext:
    case CSSValueCaptiontext:
    case CSSValueFieldtext:
    case CSSValueInactivecaptiontext:
    case CSSValueInfotext:
    case CSSValueText:
    case CSSValueWindowtext:
        return useDarkAppearance ? Color::white : Color::black;

    case CSSValueInactiveborder:
    case CSSValueInactivecaption:
        return useDarkAppearance ? Color::black : Color::white;

    case CSSValueWebkitFocusRingColor:
    case CSSValueActiveborder:
        // Hardcoded to avoid exposing a user appearance preference to the web for fingerprinting.
        return SRGBA<uint8_t> { 52, 132, 228, static_cast<unsigned char>(255 * focusRingOpacity) };

    case CSSValueHighlight:
        // Hardcoded to avoid exposing a user appearance preference to the web for fingerprinting.
        return SRGBA<uint8_t> { 52, 132, 228 };

    case CSSValueHighlighttext:
        return Color::white;

    default:
        return RenderTheme::systemColor(cssValueID, options);
    }
}

bool RenderThemeAdwaita::paintTextField(const RenderObject& renderObject, const PaintInfo& paintInfo, const FloatRect& rect)
{
    auto& graphicsContext = paintInfo.context();
    GraphicsContextStateSaver stateSaver(graphicsContext);

    SRGBA<uint8_t> textFieldBackgroundColor;
    SRGBA<uint8_t> textFieldBorderColor;

    if (renderObject.useDarkAppearance()) {
        textFieldBackgroundColor = textFieldBackgroundColorDark;
        textFieldBorderColor= textFieldBorderColorDark;
    } else {
        textFieldBackgroundColor = textFieldBackgroundColorLight;
        textFieldBorderColor = textFieldBorderColorLight;
    }

    bool enabled = isEnabled(renderObject) && !isReadOnlyControl(renderObject);
    int borderSize = textFieldBorderSize;
    if (enabled && isFocused(renderObject))
        borderSize *= 2;

    if (!enabled)
        graphicsContext.beginTransparencyLayer(disabledOpacity);

    FloatRect fieldRect = rect;
    FloatSize corner(5, 5);
    Path path;
    path.addRoundedRect(fieldRect, corner);
    fieldRect.inflate(-borderSize);
    corner.expand(-borderSize, -borderSize);
    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::EvenOdd);
    if (enabled && isFocused(renderObject))
        graphicsContext.setFillColor(platformFocusRingColor({ }));
    else
        graphicsContext.setFillColor(textFieldBorderColor);
    graphicsContext.fillPath(path);
    path.clear();

    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::NonZero);
    graphicsContext.setFillColor(textFieldBackgroundColor);
    graphicsContext.fillPath(path);

#if ENABLE(DATALIST_ELEMENT)
    if (is<HTMLInputElement>(renderObject.generatingNode()) && downcast<HTMLInputElement>(*(renderObject.generatingNode())).list()) {
        auto zoomedArrowSize = menuListButtonArrowSize * renderObject.style().effectiveZoom();
        FloatRect arrowRect = rect;
        if (renderObject.style().direction() == TextDirection::LTR)
            arrowRect.move(arrowRect.width() - (zoomedArrowSize + textFieldBorderSize * 2), 0);
        else
            fieldRect.move(textFieldBorderSize * 2, 0);
        arrowRect.setWidth(zoomedArrowSize);
        ThemeAdwaita::paintArrow(graphicsContext, arrowRect, ThemeAdwaita::ArrowDirection::Down, renderObject.useDarkAppearance());
    }
#endif

    if (!enabled)
        graphicsContext.endTransparencyLayer();

    return false;
}

void RenderThemeAdwaita::adjustTextFieldStyle(RenderStyle& style, const Element*) const
{
    if (!style.hasExplicitlySetBorderRadius())
        style.setBorderRadius(IntSize(5, 5));
}

bool RenderThemeAdwaita::paintTextArea(const RenderObject& renderObject, const PaintInfo& paintInfo, const FloatRect& rect)
{
    return paintTextField(renderObject, paintInfo, rect);
}

void RenderThemeAdwaita::adjustTextAreaStyle(RenderStyle& style, const Element* element) const
{
    adjustTextFieldStyle(style, element);
}

bool RenderThemeAdwaita::paintSearchField(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintTextField(renderObject, paintInfo, rect);
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

LengthBox RenderThemeAdwaita::popupInternalPaddingBox(const RenderStyle& style, const Settings&) const
{
    if (style.effectiveAppearance() == ControlPartType::NoControl)
        return { };

    auto zoomedArrowSize = menuListButtonArrowSize * style.effectiveZoom();
    int leftPadding = menuListButtonPadding + (style.direction() == TextDirection::RTL ? zoomedArrowSize : 0);
    int rightPadding = menuListButtonPadding + (style.direction() == TextDirection::LTR ? zoomedArrowSize : 0);

    return { menuListButtonPadding, rightPadding, menuListButtonPadding, leftPadding };
}

bool RenderThemeAdwaita::paintMenuList(const RenderObject& renderObject, const PaintInfo& paintInfo, const FloatRect& rect)
{
    auto& graphicsContext = paintInfo.context();
    GraphicsContextStateSaver stateSaver(graphicsContext);

    OptionSet<ControlStates::States> states;
    if (isEnabled(renderObject))
        states.add(ControlStates::States::Enabled);
    if (isPressed(renderObject))
        states.add(ControlStates::States::Pressed);
    if (isHovered(renderObject))
        states.add(ControlStates::States::Hovered);
    ControlStates controlStates(states);
    Theme::singleton().paint(ControlPartType::Button, controlStates, graphicsContext, rect, 1., nullptr, 1., 1., false, renderObject.useDarkAppearance(), renderObject.style().effectiveAccentColor());

    auto zoomedArrowSize = menuListButtonArrowSize * renderObject.style().effectiveZoom();
    FloatRect fieldRect = rect;
    fieldRect.inflate(menuListButtonBorderSize);
    if (renderObject.style().direction() == TextDirection::LTR)
        fieldRect.move(fieldRect.width() - (zoomedArrowSize + menuListButtonPadding), 0);
    else
        fieldRect.move(menuListButtonPadding, 0);
    fieldRect.setWidth(zoomedArrowSize);
    ThemeAdwaita::paintArrow(graphicsContext, fieldRect, ThemeAdwaita::ArrowDirection::Down, renderObject.useDarkAppearance());

    if (isFocused(renderObject))
        ThemeAdwaita::paintFocus(graphicsContext, rect, menuListButtonFocusOffset, platformFocusRingColor({ }));

    return false;
}

void RenderThemeAdwaita::paintMenuListButtonDecorations(const RenderBox& renderObject, const PaintInfo& paintInfo, const FloatRect& rect)
{
    paintMenuList(renderObject, paintInfo, rect);
}

Seconds RenderThemeAdwaita::animationRepeatIntervalForProgressBar(const RenderProgress&) const
{
    return progressAnimationFrameRate;
}

Seconds RenderThemeAdwaita::animationDurationForProgressBar(const RenderProgress&) const
{
    return progressAnimationFrameRate * progressAnimationFrameCount;
}

IntRect RenderThemeAdwaita::progressBarRectForBounds(const RenderObject&, const IntRect& bounds) const
{
    return { bounds.x(), bounds.y(), bounds.width(), progressBarSize };
}

bool RenderThemeAdwaita::paintProgressBar(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!renderObject.isProgress())
        return true;

    auto& graphicsContext = paintInfo.context();
    GraphicsContextStateSaver stateSaver(graphicsContext);

    SRGBA<uint8_t> progressBarBackgroundColor;

    if (renderObject.useDarkAppearance())
        progressBarBackgroundColor = progressBarBackgroundColorDark;
    else
        progressBarBackgroundColor = progressBarBackgroundColorLight;

    FloatRect fieldRect = rect;
    FloatSize corner(3, 3);
    Path path;

    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::NonZero);
    graphicsContext.setFillColor(progressBarBackgroundColor);
    graphicsContext.fillPath(path);
    path.clear();

    fieldRect = rect;
    const auto& renderProgress = downcast<RenderProgress>(renderObject);
    if (renderProgress.isDeterminate()) {
        auto progressWidth = fieldRect.width() * renderProgress.position();
        if (renderObject.style().direction() == TextDirection::RTL)
            fieldRect.move(fieldRect.width() - progressWidth, 0);
        fieldRect.setWidth(progressWidth);
    } else {
        double animationProgress = renderProgress.animationProgress();

        // Never let the progress rect shrink smaller than 2 pixels.
        fieldRect.setWidth(std::max<float>(2, fieldRect.width() / progressActivityBlocks));
        auto movableWidth = rect.width() - fieldRect.width();

        // We want the first 0.5 units of the animation progress to represent the
        // forward motion and the second 0.5 units to represent the backward motion,
        // thus we multiply by two here to get the full sweep of the progress bar with
        // each direction.
        if (animationProgress < 0.5)
            fieldRect.move(animationProgress * 2 * movableWidth, 0);
        else
            fieldRect.move((1.0 - animationProgress) * 2 * movableWidth, 0);
    }

    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::NonZero);

    graphicsContext.setFillColor(getAccentColor(renderObject));
    graphicsContext.fillPath(path);

    return false;
}

bool RenderThemeAdwaita::paintSliderTrack(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    auto& graphicsContext = paintInfo.context();
    GraphicsContextStateSaver stateSaver(graphicsContext);

    auto type = renderObject.style().effectiveAppearance();
    ASSERT(type == ControlPartType::SliderHorizontal || type == ControlPartType::SliderVertical);

    FloatRect fieldRect = rect;
    if (type == ControlPartType::SliderHorizontal) {
        fieldRect.move(0, rect.height() / 2 - (sliderTrackSize / 2));
        fieldRect.setHeight(6);
    } else {
        fieldRect.move(rect.width() / 2 - (sliderTrackSize / 2), 0);
        fieldRect.setWidth(6);
    }

    SRGBA<uint8_t> sliderTrackBackgroundColor;

    if (renderObject.useDarkAppearance())
        sliderTrackBackgroundColor = sliderTrackBackgroundColorDark;
    else
        sliderTrackBackgroundColor = sliderTrackBackgroundColorLight;

    if (!isEnabled(renderObject))
        graphicsContext.beginTransparencyLayer(disabledOpacity);

    FloatSize corner(3, 3);
    Path path;

    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::NonZero);
    graphicsContext.setFillColor(sliderTrackBackgroundColor);
    graphicsContext.fillPath(path);
    path.clear();

    LayoutPoint thumbLocation;
    if (is<HTMLInputElement>(renderObject.node())) {
        auto& input = downcast<HTMLInputElement>(*renderObject.node());
        if (auto* element = input.sliderThumbElement())
            thumbLocation = element->renderBox()->location() + LayoutPoint(sliderThumbSize / 2, 0);
    }
    FloatRect rangeRect = fieldRect;
    FloatRoundedRect::Radii corners;
    if (type == ControlPartType::SliderHorizontal) {
        if (renderObject.style().direction() == TextDirection::RTL) {
            rangeRect.move(thumbLocation.x(), 0);
            rangeRect.setWidth(rangeRect.width() - thumbLocation.x());
            corners.setTopRight(corner);
            corners.setBottomRight(corner);
        } else {
            rangeRect.setWidth(thumbLocation.x());
            corners.setTopLeft(corner);
            corners.setBottomLeft(corner);
        }
    } else {
        rangeRect.setHeight(thumbLocation.y());
        corners.setTopLeft(corner);
        corners.setTopRight(corner);
    }

    path.addRoundedRect(FloatRoundedRect(rangeRect, corners));
    graphicsContext.setFillRule(WindRule::NonZero);
    graphicsContext.setFillColor(getAccentColor(renderObject));
    graphicsContext.fillPath(path);

#if ENABLE(DATALIST_ELEMENT)
    paintSliderTicks(renderObject, paintInfo, rect);
#endif

    if (isFocused(renderObject)) {
        // Sliders support accent-color, so we want to color their focus rings too
        Color focusRingColor = getAccentColor(renderObject).colorWithAlphaMultipliedBy(focusRingOpacity);
        ThemeAdwaita::paintFocus(graphicsContext, fieldRect, sliderTrackFocusOffset, focusRingColor, ThemeAdwaita::PaintRounded::Yes);
    }

    if (!isEnabled(renderObject))
        graphicsContext.endTransparencyLayer();

    return false;
}

void RenderThemeAdwaita::adjustSliderThumbSize(RenderStyle& style, const Element*) const
{
    auto type = style.effectiveAppearance();
    if (type != ControlPartType::SliderThumbHorizontal && type != ControlPartType::SliderThumbVertical)
        return;

    style.setWidth(Length(sliderThumbSize, LengthType::Fixed));
    style.setHeight(Length(sliderThumbSize, LengthType::Fixed));
}

bool RenderThemeAdwaita::paintSliderThumb(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    auto& graphicsContext = paintInfo.context();
    GraphicsContextStateSaver stateSaver(graphicsContext);

    ASSERT(renderObject.style().effectiveAppearance() == ControlPartType::SliderThumbHorizontal || renderObject.style().effectiveAppearance() == ControlPartType::SliderThumbVertical);

    SRGBA<uint8_t> sliderThumbBackgroundColor;
    SRGBA<uint8_t> sliderThumbBackgroundHoveredColor;
    SRGBA<uint8_t> sliderThumbBackgroundDisabledColor;
    SRGBA<uint8_t> sliderThumbBorderColor;

    if (renderObject.useDarkAppearance()) {
        sliderThumbBackgroundColor = sliderThumbBackgroundColorDark;
        sliderThumbBackgroundHoveredColor = sliderThumbBackgroundHoveredColorDark;
        sliderThumbBackgroundDisabledColor = sliderThumbBackgroundDisabledColorDark;
        sliderThumbBorderColor = sliderThumbBorderColorDark;
    } else {
        sliderThumbBackgroundColor = sliderThumbBackgroundColorLight;
        sliderThumbBackgroundHoveredColor = sliderThumbBackgroundHoveredColorLight;
        sliderThumbBackgroundDisabledColor = sliderThumbBackgroundDisabledColorLight;
        sliderThumbBorderColor = sliderThumbBorderColorLight;
    }

    FloatRect fieldRect = rect;
    Path path;
    path.addEllipse(fieldRect);
    fieldRect.inflate(-sliderThumbBorderSize);
    path.addEllipse(fieldRect);
    graphicsContext.setFillRule(WindRule::EvenOdd);
    if (isEnabled(renderObject) && isPressed(renderObject))
        graphicsContext.setFillColor(getAccentColor(renderObject));
    else
        graphicsContext.setFillColor(sliderThumbBorderColor);
    graphicsContext.fillPath(path);
    path.clear();

    path.addEllipse(fieldRect);
    graphicsContext.setFillRule(WindRule::NonZero);
    if (!isEnabled(renderObject))
        graphicsContext.setFillColor(sliderThumbBackgroundDisabledColor);
    else if (isHovered(renderObject))
        graphicsContext.setFillColor(sliderThumbBackgroundHoveredColor);
    else
        graphicsContext.setFillColor(sliderThumbBackgroundColor);
    graphicsContext.fillPath(path);

    return false;
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

#if PLATFORM(GTK)
Seconds RenderThemeAdwaita::caretBlinkInterval() const
{
    gboolean shouldBlink;
    gint time;
    g_object_get(gtk_settings_get_default(), "gtk-cursor-blink", &shouldBlink, "gtk-cursor-blink-time", &time, nullptr);
    return shouldBlink ? 500_us * time : 0_s;
}
#endif

void RenderThemeAdwaita::setAccentColor(const Color& color)
{
    static_cast<ThemeAdwaita&>(Theme::singleton()).setAccentColor(color);
    platformColorsDidChange();
}

} // namespace WebCore
