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
#include "UserAgentScripts.h"
#include "UserAgentStyleSheets.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static const int focusOffset = 3;
static const unsigned focusLineWidth = 1;
static const Color focusColor = makeRGBA(46, 52, 54, 150);
static const unsigned arrowSize = 16;
static const Color arrowColor = makeRGB(46, 52, 54);
static const unsigned menuListButtonPadding = 5;
static const int menuListButtonBorderSize = 1;
static const Color menuListButtonBackgroundBorderColor = makeRGB(205, 199, 194);
static const Color menuListButtonBackgroundColor = makeRGB(244, 242, 241);
static const Color menuListButtonBackgroundPressedColor = makeRGB(214, 209, 205);
static const Color menuListButtonBackgroundHoveredColor = makeRGB(248, 248, 247);

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
        return false;
    case MenulistPart:
        return true;
    case RadioPart:
    case CheckboxPart:
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
    return String();
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

static void paintFocus(GraphicsContext& graphicsContext, const FloatRect& rect)
{
    FloatRect focusRect = rect;
    focusRect.inflate(-focusOffset);
    graphicsContext.setStrokeThickness(focusLineWidth);
    graphicsContext.setLineDash({ focusLineWidth, 2 * focusLineWidth }, 0);
    graphicsContext.setLineCap(SquareCap);
    graphicsContext.setLineJoin(MiterJoin);

    Path path;
    path.addRoundedRect(focusRect, { 5, 5 });
    graphicsContext.setStrokeColor(focusColor);
    graphicsContext.strokePath(path);
}

static void paintArrow(GraphicsContext& graphicsContext, const FloatRect& rect)
{
    Path path;
    path.moveTo({ rect.x() + 3, rect.y() + 6 });
    path.addLineTo({ rect.x() + 13, rect.y() + 6 });
    path.addLineTo({ rect.x() + 8, rect.y() + 11 });
    path.closeSubpath();

    graphicsContext.setFillColor(arrowColor);
    graphicsContext.fillPath(path);
}

LengthBox RenderThemeWPE::popupInternalPaddingBox(const RenderStyle& style) const
{
    if (style.appearance() == NoControlPart)
        return { };

    int leftPadding = menuListButtonPadding + (style.direction() == TextDirection::RTL ? arrowSize : 0);
    int rightPadding = menuListButtonPadding + (style.direction() == TextDirection::LTR ? arrowSize : 0);

    return { menuListButtonPadding, rightPadding, menuListButtonPadding, leftPadding };
}

bool RenderThemeWPE::paintMenuList(const RenderObject& renderObject, const PaintInfo& paintInfo, const FloatRect& rect)
{
    auto& graphicsContext = paintInfo.context();
    GraphicsContextStateSaver stateSaver(graphicsContext);

    FloatRect fieldRect = rect;
    FloatSize corner(5, 5);
    Path path;
    path.addRoundedRect(fieldRect, corner);
    fieldRect.inflate(-menuListButtonBorderSize);
    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::EvenOdd);
    graphicsContext.setFillColor(menuListButtonBackgroundBorderColor);
    graphicsContext.fillPath(path);
    path.clear();

    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::NonZero);
    if (isPressed(renderObject))
        graphicsContext.setFillColor(menuListButtonBackgroundPressedColor);
    else if (isHovered(renderObject))
        graphicsContext.setFillColor(menuListButtonBackgroundHoveredColor);
    else
        graphicsContext.setFillColor(menuListButtonBackgroundColor);
    graphicsContext.fillPath(path);
    path.clear();

    if (renderObject.style().direction() == TextDirection::LTR)
        fieldRect.move(fieldRect.width() - (arrowSize + menuListButtonPadding), (fieldRect.height() / 2.) - (arrowSize / 2));
    else
        fieldRect.move(menuListButtonPadding, (fieldRect.height() / 2.) - (arrowSize / 2));
    fieldRect.setWidth(arrowSize);
    fieldRect.setHeight(arrowSize);
    paintArrow(graphicsContext, fieldRect);

    if (isFocused(renderObject))
        paintFocus(graphicsContext, rect);

    return false;
}

bool RenderThemeWPE::paintMenuListButtonDecorations(const RenderBox& renderObject, const PaintInfo& paintInfo, const FloatRect& rect)
{
    return paintMenuList(renderObject, paintInfo, rect);
}

} // namespace WebCore
