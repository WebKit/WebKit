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
#include "RenderThemeWPE.h"

#include "Color.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "NotImplemented.h"
#include "PaintInfo.h"
#include "RenderBox.h"
#include "RenderObject.h"
#include "RenderProgress.h"
#include "RenderStyle.h"
#include "ThemeWPE.h"
#include "UserAgentScripts.h"
#include "UserAgentStyleSheets.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static const int textFieldBorderSize = 1;
static const Color textFieldBorderColor = makeRGB(205, 199, 194);
static const Color textFieldBorderDisabledColor = makeRGB(213, 208, 204);
static const Color textFieldBackgroundColor = makeRGB(255, 255, 255);
static const Color textFieldBackgroundDisabledColor = makeRGB(252, 252, 252);
static const unsigned menuListButtonArrowSize = 16;
static const int menuListButtonFocusOffset = -3;
static const unsigned menuListButtonPadding = 5;
static const int menuListButtonBorderSize = 1; // Keep in sync with buttonBorderSize in ThemeWPE.
static const unsigned progressActivityBlocks = 5;
static const unsigned progressAnimationFrameCount = 75;
static const Seconds progressAnimationFrameRate = 33_ms; // 30fps.
static const unsigned progressBarSize = 6;
static const Color progressBarBorderColor = makeRGB(205, 199, 194);
static const Color progressBarBackgroundColor = makeRGB(225, 222, 219);
static const unsigned sliderTrackSize = 6;
static const int sliderTrackBorderSize = 1;
static const Color sliderTrackBorderColor = makeRGB(205, 199, 194);
static const Color sliderTrackBackgroundColor = makeRGB(225, 222, 219);
static const int sliderTrackFocusOffset = 2;
static const int sliderThumbSize = 20;
static const int sliderThumbBorderSize = 1;
static const Color sliderThumbBorderColor = makeRGB(205, 199, 194);
static const Color sliderThumbBackgroundColor = makeRGB(244, 242, 241);
static const Color sliderThumbBackgroundHoveredColor = makeRGB(248, 248, 247);
static const Color sliderThumbBackgroundDisabledColor = makeRGB(244, 242, 241);

RenderTheme& RenderTheme::singleton()
{
    static NeverDestroyed<RenderThemeWPE> theme;
    return theme;
}

bool RenderThemeWPE::supportsFocusRing(const RenderStyle& style) const
{
    switch (style.appearance()) {
    case PushButtonPart:
    case ButtonPart:
    case TextFieldPart:
    case TextAreaPart:
    case SearchFieldPart:
    case MenulistPart:
    case RadioPart:
    case CheckboxPart:
    case SliderHorizontalPart:
    case SliderVerticalPart:
        return true;
    default:
        break;
    }

    return false;
}

void RenderThemeWPE::updateCachedSystemFontDescription(CSSValueID, FontCascadeDescription&) const
{
    notImplemented();
}

Color RenderThemeWPE::platformFocusRingColor(OptionSet<StyleColor::Options>) const
{
    return ThemeWPE::focusColor();
}

Color RenderThemeWPE::platformActiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const
{
    return makeRGB(52, 132, 228);
}

Color RenderThemeWPE::platformInactiveSelectionBackgroundColor(OptionSet<StyleColor::Options> options) const
{
    return platformActiveSelectionBackgroundColor(options);
}

Color RenderThemeWPE::platformActiveSelectionForegroundColor(OptionSet<StyleColor::Options>) const
{
    return makeRGB(255, 255, 255);
}

Color RenderThemeWPE::platformInactiveSelectionForegroundColor(OptionSet<StyleColor::Options>) const
{
    return makeRGB(252, 252, 252);
}

Color RenderThemeWPE::platformActiveListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options> options) const
{
    return platformActiveSelectionBackgroundColor(options);
}

Color RenderThemeWPE::platformInactiveListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options> options) const
{
    return platformInactiveSelectionBackgroundColor(options);
}

Color RenderThemeWPE::platformActiveListBoxSelectionForegroundColor(OptionSet<StyleColor::Options> options) const
{
    return platformActiveSelectionForegroundColor(options);
}

Color RenderThemeWPE::platformInactiveListBoxSelectionForegroundColor(OptionSet<StyleColor::Options> options) const
{
    return platformInactiveSelectionForegroundColor(options);
}

String RenderThemeWPE::extraDefaultStyleSheet()
{
    return String(themeAdwaitaUserAgentStyleSheet, sizeof(themeAdwaitaUserAgentStyleSheet));
}

#if ENABLE(VIDEO)
String RenderThemeWPE::mediaControlsStyleSheet()
{
    return String(mediaControlsBaseUserAgentStyleSheet, sizeof(mediaControlsBaseUserAgentStyleSheet));
}

String RenderThemeWPE::mediaControlsScript()
{
    StringBuilder scriptBuilder;
    scriptBuilder.appendCharacters(mediaControlsLocalizedStringsJavaScript, sizeof(mediaControlsLocalizedStringsJavaScript));
    scriptBuilder.appendCharacters(mediaControlsBaseJavaScript, sizeof(mediaControlsBaseJavaScript));
    return scriptBuilder.toString();
}
#endif

bool RenderThemeWPE::paintTextField(const RenderObject& renderObject, const PaintInfo& paintInfo, const FloatRect& rect)
{
    auto& graphicsContext = paintInfo.context();
    GraphicsContextStateSaver stateSaver(graphicsContext);

    int borderSize = textFieldBorderSize;
    if (isEnabled(renderObject) && !isReadOnlyControl(renderObject) && isFocused(renderObject))
        borderSize *= 2;

    FloatRect fieldRect = rect;
    FloatSize corner(5, 5);
    Path path;
    path.addRoundedRect(fieldRect, corner);
    fieldRect.inflate(-borderSize);
    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::EvenOdd);
    if (!isEnabled(renderObject) || isReadOnlyControl(renderObject))
        graphicsContext.setFillColor(textFieldBorderDisabledColor);
    else if (isFocused(renderObject))
        graphicsContext.setFillColor(activeSelectionBackgroundColor({ }));
    else
        graphicsContext.setFillColor(textFieldBorderColor);
    graphicsContext.fillPath(path);
    path.clear();

    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::NonZero);
    if (!isEnabled(renderObject) || isReadOnlyControl(renderObject))
        graphicsContext.setFillColor(textFieldBackgroundDisabledColor);
    else
        graphicsContext.setFillColor(textFieldBackgroundColor);
    graphicsContext.fillPath(path);

    return false;
}

bool RenderThemeWPE::paintTextArea(const RenderObject& renderObject, const PaintInfo& paintInfo, const FloatRect& rect)
{
    return paintTextField(renderObject, paintInfo, rect);
}

bool RenderThemeWPE::paintSearchField(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintTextField(renderObject, paintInfo, rect);
}

LengthBox RenderThemeWPE::popupInternalPaddingBox(const RenderStyle& style) const
{
    if (style.appearance() == NoControlPart)
        return { };

    int leftPadding = menuListButtonPadding + (style.direction() == TextDirection::RTL ? menuListButtonArrowSize : 0);
    int rightPadding = menuListButtonPadding + (style.direction() == TextDirection::LTR ? menuListButtonArrowSize : 0);

    return { menuListButtonPadding, rightPadding, menuListButtonPadding, leftPadding };
}

bool RenderThemeWPE::paintMenuList(const RenderObject& renderObject, const PaintInfo& paintInfo, const FloatRect& rect)
{
    auto& graphicsContext = paintInfo.context();
    GraphicsContextStateSaver stateSaver(graphicsContext);

    ControlStates::States states = 0;
    if (isEnabled(renderObject))
        states |= ControlStates::EnabledState;
    if (isPressed(renderObject))
        states |= ControlStates::PressedState;
    if (isHovered(renderObject))
        states |= ControlStates::HoverState;
    ControlStates controlStates(states);
    Theme::singleton().paint(ButtonPart, controlStates, graphicsContext, rect, 1., nullptr, 1., 1., false, false);

    FloatRect fieldRect = rect;
    fieldRect.inflate(menuListButtonBorderSize);
    if (renderObject.style().direction() == TextDirection::LTR)
        fieldRect.move(fieldRect.width() - (menuListButtonArrowSize + menuListButtonPadding), (fieldRect.height() / 2.) - (menuListButtonArrowSize / 2));
    else
        fieldRect.move(menuListButtonPadding, (fieldRect.height() / 2.) - (menuListButtonArrowSize / 2));
    fieldRect.setWidth(menuListButtonArrowSize);
    fieldRect.setHeight(menuListButtonArrowSize);
    {
        GraphicsContextStateSaver arrowStateSaver(graphicsContext);
        graphicsContext.translate(fieldRect.x(), fieldRect.y());
        ThemeWPE::paintArrow(graphicsContext, ThemeWPE::ArrowDirection::Down);
    }

    if (isFocused(renderObject))
        ThemeWPE::paintFocus(graphicsContext, rect, menuListButtonFocusOffset);

    return false;
}

bool RenderThemeWPE::paintMenuListButtonDecorations(const RenderBox& renderObject, const PaintInfo& paintInfo, const FloatRect& rect)
{
    return paintMenuList(renderObject, paintInfo, rect);
}

Seconds RenderThemeWPE::animationRepeatIntervalForProgressBar(RenderProgress&) const
{
    return progressAnimationFrameRate;
}

Seconds RenderThemeWPE::animationDurationForProgressBar(RenderProgress&) const
{
    return progressAnimationFrameRate * progressAnimationFrameCount;
}

IntRect RenderThemeWPE::progressBarRectForBounds(const RenderObject&, const IntRect& bounds) const
{
    return { bounds.x(), bounds.y(), bounds.width(), progressBarSize };
}

bool RenderThemeWPE::paintProgressBar(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!renderObject.isProgress())
        return true;

    auto& graphicsContext = paintInfo.context();
    GraphicsContextStateSaver stateSaver(graphicsContext);

    FloatRect fieldRect = rect;
    FloatSize corner(3, 3);
    Path path;
    path.addRoundedRect(fieldRect, corner);
    fieldRect.inflate(-1);
    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::EvenOdd);
    graphicsContext.setFillColor(progressBarBorderColor);
    graphicsContext.fillPath(path);
    path.clear();

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
    graphicsContext.setFillColor(activeSelectionBackgroundColor({ }));
    graphicsContext.fillPath(path);

    return false;
}

bool RenderThemeWPE::paintSliderTrack(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    auto& graphicsContext = paintInfo.context();
    GraphicsContextStateSaver stateSaver(graphicsContext);

    ControlPart part = renderObject.style().appearance();
    ASSERT(part == SliderHorizontalPart || part == SliderVerticalPart);

    FloatRect fieldRect = rect;
    if (part == SliderHorizontalPart) {
        fieldRect.move(0, rect.height() / 2 - (sliderTrackSize / 2));
        fieldRect.setHeight(6);
    } else {
        fieldRect.move(rect.width() / 2 - (sliderTrackSize / 2), 0);
        fieldRect.setWidth(6);
    }

    FloatSize corner(3, 3);
    Path path;
    path.addRoundedRect(fieldRect, corner);
    fieldRect.inflate(-sliderTrackBorderSize);
    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::EvenOdd);
    graphicsContext.setFillColor(sliderTrackBorderColor);
    graphicsContext.fillPath(path);
    path.clear();

    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::NonZero);
    graphicsContext.setFillColor(sliderTrackBackgroundColor);
    graphicsContext.fillPath(path);
    path.clear();

    fieldRect.inflate(sliderTrackBorderSize);
    LayoutPoint thumbLocation;
    if (is<HTMLInputElement>(renderObject.node())) {
        auto& input = downcast<HTMLInputElement>(*renderObject.node());
        if (auto* element = input.sliderThumbElement())
            thumbLocation = element->renderBox()->location();
    }
    FloatRect rangeRect = fieldRect;
    FloatRoundedRect::Radii corners;
    if (part == SliderHorizontalPart) {
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
    graphicsContext.setFillColor(activeSelectionBackgroundColor({ }));
    graphicsContext.fillPath(path);

    if (isFocused(renderObject))
        ThemeWPE::paintFocus(graphicsContext, fieldRect, sliderTrackFocusOffset);

    return false;
}

void RenderThemeWPE::adjustSliderThumbSize(RenderStyle& style, const Element*) const
{
    ControlPart part = style.appearance();
    if (part != SliderThumbHorizontalPart && part != SliderThumbVerticalPart)
        return;

    style.setWidth(Length(sliderThumbSize, Fixed));
    style.setHeight(Length(sliderThumbSize, Fixed));
}

bool RenderThemeWPE::paintSliderThumb(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    auto& graphicsContext = paintInfo.context();
    GraphicsContextStateSaver stateSaver(graphicsContext);

    ASSERT(renderObject.style().appearance() == SliderThumbHorizontalPart || renderObject.style().appearance() == SliderThumbVerticalPart);

    FloatRect fieldRect = rect;
    Path path;
    path.addEllipse(fieldRect);
    fieldRect.inflate(-sliderThumbBorderSize);
    path.addEllipse(fieldRect);
    graphicsContext.setFillRule(WindRule::EvenOdd);
    if (isEnabled(renderObject) && isPressed(renderObject))
        graphicsContext.setFillColor(activeSelectionBackgroundColor({ }));
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

} // namespace WebCore
