/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "NotImplemented.h"
#include "PaintInfo.h"
#include "RenderBox.h"
#include "RenderObject.h"
#include "RenderStyle.h"
#include "ThemeWPE.h"
#include "UserAgentScripts.h"
#include "UserAgentStyleSheets.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static const int textFieldBorderSize = 1;
static const Color textFieldBorderColor = makeRGB(205, 199, 194);
static const Color textFieldBorderActiveColor = makeRGB(52, 132, 228);
static const Color textFieldBorderDisabledColor = makeRGB(213, 208, 204);
static const Color textFieldBackgroundColor = makeRGB(255, 255, 255);
static const Color textFieldBackgroundDisabledColor = makeRGB(252, 252, 252);
static const unsigned menuListButtonArrowSize = 16;
static const int menuListButtonFocusOffset = -3;
static const unsigned menuListButtonPadding = 5;
static const int menuListButtonBorderSize = 1; // Keep in sync with buttonBorderSize in ThemeWPE.

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
        return true;
    case SliderHorizontalPart:
    case SliderVerticalPart:
        return false;
    default:
        break;
    }

    return false;
}

void RenderThemeWPE::updateCachedSystemFontDescription(CSSValueID, FontCascadeDescription&) const
{
    notImplemented();
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
        graphicsContext.setFillColor(textFieldBorderActiveColor);
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

} // namespace WebCore
