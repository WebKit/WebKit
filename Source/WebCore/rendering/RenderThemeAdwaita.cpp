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

namespace WebCore {

static const int textFieldBorderSize = 1;
static const Color textFieldBorderColor = makeSimpleColor(205, 199, 194);
static const Color textFieldBorderDisabledColor = makeSimpleColor(213, 208, 204);
static const Color textFieldBackgroundColor = makeSimpleColor(255, 255, 255);
static const Color textFieldBackgroundDisabledColor = makeSimpleColor(252, 252, 252);
static const unsigned menuListButtonArrowSize = 16;
static const int menuListButtonFocusOffset = -3;
static const unsigned menuListButtonPadding = 5;
static const int menuListButtonBorderSize = 1; // Keep in sync with buttonBorderSize in ThemeAdwaita.
static const unsigned progressActivityBlocks = 5;
static const unsigned progressAnimationFrameCount = 75;
static const Seconds progressAnimationFrameRate = 33_ms; // 30fps.
static const unsigned progressBarSize = 6;
static const Color progressBarBorderColor = makeSimpleColor(205, 199, 194);
static const Color progressBarBackgroundColor = makeSimpleColor(225, 222, 219);
static const unsigned sliderTrackSize = 6;
static const int sliderTrackBorderSize = 1;
static const Color sliderTrackBorderColor = makeSimpleColor(205, 199, 194);
static const Color sliderTrackBackgroundColor = makeSimpleColor(225, 222, 219);
static const int sliderTrackFocusOffset = 2;
static const int sliderThumbSize = 20;
static const int sliderThumbBorderSize = 1;
static const Color sliderThumbBorderColor = makeSimpleColor(205, 199, 194);
static const Color sliderThumbBackgroundColor = makeSimpleColor(244, 242, 241);
static const Color sliderThumbBackgroundHoveredColor = makeSimpleColor(248, 248, 247);
static const Color sliderThumbBackgroundDisabledColor = makeSimpleColor(244, 242, 241);
#if ENABLE(VIDEO)
static const Color mediaSliderTrackBackgroundcolor = makeSimpleColor(77, 77, 77);
static const Color mediaSliderTrackBufferedColor = makeSimpleColor(173, 173, 173);
static const Color mediaSliderTrackActiveColor = makeSimpleColor(252, 252, 252);
#endif

#if !PLATFORM(GTK)
RenderTheme& RenderTheme::singleton()
{
    static NeverDestroyed<RenderThemeAdwaita> theme;
    return theme;
}
#endif

bool RenderThemeAdwaita::supportsFocusRing(const RenderStyle& style) const
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

bool RenderThemeAdwaita::shouldHaveCapsLockIndicator(const HTMLInputElement& element) const
{
    return element.isPasswordField();
}

Color RenderThemeAdwaita::platformActiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const
{
    return static_cast<ThemeAdwaita&>(Theme::singleton()).activeSelectionBackgroundColor();
}

Color RenderThemeAdwaita::platformInactiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const
{
    return static_cast<ThemeAdwaita&>(Theme::singleton()).inactiveSelectionBackgroundColor();
}

Color RenderThemeAdwaita::platformActiveSelectionForegroundColor(OptionSet<StyleColor::Options>) const
{
    return static_cast<ThemeAdwaita&>(Theme::singleton()).activeSelectionForegroundColor();
}

Color RenderThemeAdwaita::platformInactiveSelectionForegroundColor(OptionSet<StyleColor::Options>) const
{
    return static_cast<ThemeAdwaita&>(Theme::singleton()).inactiveSelectionForegroundColor();
}

Color RenderThemeAdwaita::platformActiveListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options>) const
{
    return static_cast<ThemeAdwaita&>(Theme::singleton()).activeSelectionBackgroundColor();
}

Color RenderThemeAdwaita::platformInactiveListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options>) const
{
    return static_cast<ThemeAdwaita&>(Theme::singleton()).inactiveSelectionBackgroundColor();
}

Color RenderThemeAdwaita::platformActiveListBoxSelectionForegroundColor(OptionSet<StyleColor::Options>) const
{
    return static_cast<ThemeAdwaita&>(Theme::singleton()).activeSelectionForegroundColor();
}

Color RenderThemeAdwaita::platformInactiveListBoxSelectionForegroundColor(OptionSet<StyleColor::Options>) const
{
    return static_cast<ThemeAdwaita&>(Theme::singleton()).inactiveSelectionForegroundColor();
}

Color RenderThemeAdwaita::platformFocusRingColor(OptionSet<StyleColor::Options>) const
{
    return ThemeAdwaita::focusColor();
}

void RenderThemeAdwaita::platformColorsDidChange()
{
    static_cast<ThemeAdwaita&>(Theme::singleton()).platformColorsDidChange();
    RenderTheme::platformColorsDidChange();
}

String RenderThemeAdwaita::extraDefaultStyleSheet()
{
    return String(themeAdwaitaUserAgentStyleSheet, sizeof(themeAdwaitaUserAgentStyleSheet));
}

#if ENABLE(VIDEO)
String RenderThemeAdwaita::extraMediaControlsStyleSheet()
{
    return String(mediaControlsAdwaitaUserAgentStyleSheet, sizeof(mediaControlsAdwaitaUserAgentStyleSheet));
}

String RenderThemeAdwaita::mediaControlsScript()
{
    return String(mediaControlsAdwaitaJavaScript, sizeof(mediaControlsAdwaitaJavaScript));
}
#endif

bool RenderThemeAdwaita::paintTextField(const RenderObject& renderObject, const PaintInfo& paintInfo, const FloatRect& rect)
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

#if ENABLE(DATALIST_ELEMENT)
    if (is<HTMLInputElement>(renderObject.generatingNode()) && downcast<HTMLInputElement>(*(renderObject.generatingNode())).list()) {
        FloatRect arrowRect = rect;
        if (renderObject.style().direction() == TextDirection::LTR)
            arrowRect.move(arrowRect.width() - (menuListButtonArrowSize + textFieldBorderSize * 2), (arrowRect.height() / 2.) - (menuListButtonArrowSize / 2.));
        else
            fieldRect.move(textFieldBorderSize * 2, (arrowRect.height() / 2.) - (menuListButtonArrowSize / 2.));
        arrowRect.setWidth(menuListButtonArrowSize);
        arrowRect.setHeight(menuListButtonArrowSize);
        {
            GraphicsContextStateSaver arrowStateSaver(graphicsContext);
            graphicsContext.translate(arrowRect.x(), arrowRect.y());
            ThemeAdwaita::paintArrow(graphicsContext, ThemeAdwaita::ArrowDirection::Down);
        }
    }
#endif

    return false;
}

bool RenderThemeAdwaita::paintTextArea(const RenderObject& renderObject, const PaintInfo& paintInfo, const FloatRect& rect)
{
    return paintTextField(renderObject, paintInfo, rect);
}

bool RenderThemeAdwaita::paintSearchField(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintTextField(renderObject, paintInfo, rect);
}

void RenderThemeAdwaita::adjustMenuListStyle(RenderStyle& style, const Element*) const
{
    style.setLineHeight(RenderStyle::initialLineHeight());
}

void RenderThemeAdwaita::adjustMenuListButtonStyle(RenderStyle& style, const Element* element) const
{
    adjustMenuListStyle(style, element);
}

LengthBox RenderThemeAdwaita::popupInternalPaddingBox(const RenderStyle& style) const
{
    if (style.appearance() == NoControlPart)
        return { };

    int leftPadding = menuListButtonPadding + (style.direction() == TextDirection::RTL ? menuListButtonArrowSize : 0);
    int rightPadding = menuListButtonPadding + (style.direction() == TextDirection::LTR ? menuListButtonArrowSize : 0);

    return { menuListButtonPadding, rightPadding, menuListButtonPadding, leftPadding };
}

bool RenderThemeAdwaita::paintMenuList(const RenderObject& renderObject, const PaintInfo& paintInfo, const FloatRect& rect)
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
        ThemeAdwaita::paintArrow(graphicsContext, ThemeAdwaita::ArrowDirection::Down);
    }

    if (isFocused(renderObject))
        ThemeAdwaita::paintFocus(graphicsContext, rect, menuListButtonFocusOffset);

    return false;
}

bool RenderThemeAdwaita::paintMenuListButtonDecorations(const RenderBox& renderObject, const PaintInfo& paintInfo, const FloatRect& rect)
{
    return paintMenuList(renderObject, paintInfo, rect);
}

Seconds RenderThemeAdwaita::animationRepeatIntervalForProgressBar(RenderProgress&) const
{
    return progressAnimationFrameRate;
}

Seconds RenderThemeAdwaita::animationDurationForProgressBar(RenderProgress&) const
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

bool RenderThemeAdwaita::paintSliderTrack(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
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

#if ENABLE(DATALIST_ELEMENT)
    paintSliderTicks(renderObject, paintInfo, rect);
#endif

    if (isFocused(renderObject))
        ThemeAdwaita::paintFocus(graphicsContext, fieldRect, sliderTrackFocusOffset);

    return false;
}

void RenderThemeAdwaita::adjustSliderThumbSize(RenderStyle& style, const Element*) const
{
    ControlPart part = style.appearance();
    if (part != SliderThumbHorizontalPart && part != SliderThumbVerticalPart)
        return;

    style.setWidth(Length(sliderThumbSize, Fixed));
    style.setHeight(Length(sliderThumbSize, Fixed));
}

bool RenderThemeAdwaita::paintSliderThumb(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
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

#if ENABLE(VIDEO)
static RefPtr<HTMLMediaElement> parentMediaElement(const Node* node)
{
    if (!node)
        return nullptr;
    RefPtr<Node> mediaNode = node->shadowHost();
    if (!mediaNode)
        mediaNode = const_cast<Node*>(node);
    if (!is<HTMLMediaElement>(*mediaNode))
        return nullptr;
    return downcast<HTMLMediaElement>(mediaNode.get());
}

bool RenderThemeAdwaita::paintMediaSliderTrack(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    auto mediaElement = parentMediaElement(renderObject.node());
    if (!mediaElement)
        return false;

    auto& graphicsContext = paintInfo.context();
    GraphicsContextStateSaver stateSaver(graphicsContext);

    FloatRect trackRect = rect;
    FloatSize corner(2, 2);
    Path path;
    path.addRoundedRect(trackRect, corner);
    graphicsContext.setFillColor(mediaSliderTrackBackgroundcolor);
    graphicsContext.fillPath(path);
    path.clear();

    graphicsContext.setFillColor(mediaSliderTrackBufferedColor);

    float mediaDuration = mediaElement->duration();
    RefPtr<TimeRanges> timeRanges = mediaElement->buffered();
    for (unsigned index = 0; index < timeRanges->length(); ++index) {
        float start = timeRanges->start(index).releaseReturnValue();
        float end = timeRanges->end(index).releaseReturnValue();
        float startRatio = start / mediaDuration;
        float lengthRatio = (end - start) / mediaDuration;
        if (!lengthRatio)
            continue;

        FloatRect rangeRect = rect;
        rangeRect.setWidth(lengthRatio * rect.width());
        if (index)
            rangeRect.move(startRatio * rect.width(), 0);

        path.addRoundedRect(rangeRect, corner);
        graphicsContext.fillPath(path);
        path.clear();
    }

    FloatRect playedRect = rect;
    playedRect.setWidth((mediaElement->currentTime() / mediaDuration) * rect.width());
    graphicsContext.setFillColor(mediaSliderTrackActiveColor);
    path.addRoundedRect(playedRect, corner);
    graphicsContext.fillPath(path);

    return false;
}

bool RenderThemeAdwaita::paintMediaVolumeSliderTrack(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    auto mediaElement = parentMediaElement(renderObject.node());
    if (!mediaElement)
        return false;

    auto& graphicsContext = paintInfo.context();
    GraphicsContextStateSaver stateSaver(graphicsContext);

    FloatRect trackRect = rect;
    FloatSize corner(2, 2);
    Path path;
    path.addRoundedRect(trackRect, corner);
    graphicsContext.setFillColor(mediaSliderTrackBackgroundcolor);
    graphicsContext.fillPath(path);
    path.clear();

    float volume = mediaElement->muted() ? 0.0f : mediaElement->volume();
    if (volume) {
        FloatRect volumeRect = rect;
        volumeRect.setHeight(volumeRect.height() * volume);
        volumeRect.move(0, rect.height() - volumeRect.height());
        path.addRoundedRect(volumeRect, corner);
        graphicsContext.setFillColor(mediaSliderTrackActiveColor);
        graphicsContext.fillPath(path);
    }

    return false;
}
#endif // ENABLE(VIDEO)

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
        style.setMarginRight(Length(-2, Fixed));
    else
        style.setMarginLeft(Length(-2, Fixed));
}
#endif // ENABLE(DATALIST_ELEMENT)

} // namespace WebCore
