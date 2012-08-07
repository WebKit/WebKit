/*
 * Copyright (C) 2006, 2007 Apple Inc.
 * Copyright (C) 2009 Google Inc.
 * Copyright (C) 2009, 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "RenderThemeBlackBerry.h"

#include "CSSValueKeywords.h"
#include "Frame.h"
#include "HTMLMediaElement.h"
#include "HostWindow.h"
#include "MediaControlElements.h"
#include "MediaPlayerPrivateBlackBerry.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderFullScreen.h"
#include "RenderProgress.h"
#include "RenderSlider.h"
#include "RenderView.h"
#include "UserAgentStyleSheets.h"

namespace WebCore {

// Sizes (unit px)
const unsigned smallRadius = 1;
const unsigned largeRadius = 3;
const unsigned lineWidth = 1;
const float marginSize = 4;
const float mediaControlsHeight = 32;
const float mediaSliderThumbWidth = 40;
const float mediaSliderThumbHeight = 13;
const float mediaSliderThumbRadius = 5;
const float sliderThumbWidth = 15;
const float sliderThumbHeight = 25;

// Checkbox check scalers
const float checkboxLeftX = 7 / 40.0;
const float checkboxLeftY = 1 / 2.0;
const float checkboxMiddleX = 19 / 50.0;
const float checkboxMiddleY = 7 / 25.0;
const float checkboxRightX = 33 / 40.0;
const float checkboxRightY = 1 / 5.0;
const float checkboxStrokeThickness = 6.5;

// Radio button scaler
const float radioButtonCheckStateScaler = 7 / 30.0;

// Multipliers
const unsigned paddingDivisor = 5;
const unsigned fullScreenEnlargementFactor = 2;
const float scaleFactorThreshold = 2.0;

// Colors
const RGBA32 caretBottom = 0xff2163bf;
const RGBA32 caretTop = 0xff69a5fa;

const RGBA32 regularBottom = 0xffdcdee4;
const RGBA32 regularTop = 0xfff7f2ee;
const RGBA32 hoverBottom = 0xffb5d3fc;
const RGBA32 hoverTop = 0xffcceaff;
const RGBA32 depressedBottom = 0xff3388ff;
const RGBA32 depressedTop = 0xff66a0f2;
const RGBA32 disabledBottom = 0xffe7e7e7;
const RGBA32 disabledTop = 0xffefefef;

const RGBA32 regularBottomOutline = 0xff6e7073;
const RGBA32 regularTopOutline = 0xffb9b8b8;
const RGBA32 hoverBottomOutline = 0xff2163bf;
const RGBA32 hoverTopOutline = 0xff69befa;
const RGBA32 depressedBottomOutline = 0xff0c3d81;
const RGBA32 depressedTopOutline = 0xff1d4d70;
const RGBA32 disabledOutline = 0xffd5d9de;

const RGBA32 progressRegularBottom = caretTop;
const RGBA32 progressRegularTop = caretBottom;

const RGBA32 rangeSliderRegularBottom = 0xfff6f2ee;
const RGBA32 rangeSliderRegularTop = 0xffdee0e5;
const RGBA32 rangeSliderRollBottom = 0xffc9e8fe;
const RGBA32 rangeSliderRollTop = 0xffb5d3fc;

const RGBA32 rangeSliderRegularBottomOutline = 0xffb9babd;
const RGBA32 rangeSliderRegularTopOutline = 0xffb7b7b7;
const RGBA32 rangeSliderRollBottomOutline = 0xff67abe0;
const RGBA32 rangeSliderRollTopOutline = 0xff69adf9;

const RGBA32 dragRegularLight = 0xfffdfdfd;
const RGBA32 dragRegularDark = 0xffbababa;
const RGBA32 dragRollLight = 0xfff2f2f2;
const RGBA32 dragRollDark = 0xff69a8ff;

const RGBA32 selection = 0xff2b8fff;

const RGBA32 blackPen = Color::black;
const RGBA32 focusRingPen = 0xffa3c8fe;

float RenderThemeBlackBerry::defaultFontSize = 16;

// We aim to match IE here.
// -IE uses a font based on the encoding as the default font for form controls.
// -Gecko uses MS Shell Dlg (actually calls GetStockObject(DEFAULT_GUI_FONT),
// which returns MS Shell Dlg)
// -Safari uses Lucida Grande.
//
// FIXME: The only case where we know we don't match IE is for ANSI encodings.
// IE uses MS Shell Dlg there, which we render incorrectly at certain pixel
// sizes (e.g. 15px). So we just use Arial for now.
const String& RenderThemeBlackBerry::defaultGUIFont()
{
    DEFINE_STATIC_LOCAL(String, fontFace, ("Arial"));
    return fontFace;
}

static PassRefPtr<Gradient> createLinearGradient(RGBA32 top, RGBA32 bottom, const IntPoint& a, const IntPoint& b)
{
    RefPtr<Gradient> gradient = Gradient::create(a, b);
    gradient->addColorStop(0.0, Color(top));
    gradient->addColorStop(1.0, Color(bottom));
    return gradient.release();
}

static Path roundedRectForBorder(RenderObject* object, const IntRect& rect)
{
    RenderStyle* style = object->style();
    LengthSize topLeftRadius = style->borderTopLeftRadius();
    LengthSize topRightRadius = style->borderTopRightRadius();
    LengthSize bottomLeftRadius = style->borderBottomLeftRadius();
    LengthSize bottomRightRadius = style->borderBottomRightRadius();

    Path roundedRect;
    roundedRect.addRoundedRect(rect, IntSize(topLeftRadius.width().value(), topLeftRadius.height().value()),
                                     IntSize(topRightRadius.width().value(), topRightRadius.height().value()),
                                     IntSize(bottomLeftRadius.width().value(), bottomLeftRadius.height().value()),
                                     IntSize(bottomRightRadius.width().value(), bottomRightRadius.height().value()));
    return roundedRect;
}

static RenderSlider* determineRenderSlider(RenderObject* object)
{
    ASSERT(object->isSliderThumb());
    // The RenderSlider is an ancestor of the slider thumb.
    while (object && !object->isSlider())
        object = object->parent();
    return toRenderSlider(object);
}

static float determineFullScreenMultiplier(Element* element)
{
    float fullScreenMultiplier = 1.0;
#if ENABLE(FULLSCREEN_API) && ENABLE(VIDEO)
    if (element && element->document()->webkitIsFullScreen() && element->document()->webkitCurrentFullScreenElement() == toParentMediaElement(element)) {
        if (element->document()->page()->deviceScaleFactor() < scaleFactorThreshold)
            fullScreenMultiplier = fullScreenEnlargementFactor;

        // The way the BlackBerry port implements the FULLSCREEN_API for media elements
        // might result in the controls being oversized, proportionally to the current page
        // scale. That happens because the fullscreen element gets sized to be as big as the
        // viewport size, and the viewport size might get outstretched to fit to the screen dimensions.
        // To fix that, lets strips out the Page scale factor from the media controls multiplier.
        float scaleFactor = element->document()->view()->hostWindow()->platformPageClient()->currentZoomFactor();
        static ViewportArguments defaultViewportArguments;
        float scaleFactorFudge = 1 / element->document()->page()->deviceScaleFactor();
        fullScreenMultiplier /= scaleFactor * scaleFactorFudge;
    }
#endif
    return fullScreenMultiplier;
}

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page* page)
{
    static RenderTheme* theme = RenderThemeBlackBerry::create().leakRef();
    return theme;
}

PassRefPtr<RenderTheme> RenderThemeBlackBerry::create()
{
    return adoptRef(new RenderThemeBlackBerry());
}

RenderThemeBlackBerry::RenderThemeBlackBerry()
{
}

RenderThemeBlackBerry::~RenderThemeBlackBerry()
{
}

String RenderThemeBlackBerry::extraDefaultStyleSheet()
{
    return String(themeBlackBerryUserAgentStyleSheet, sizeof(themeBlackBerryUserAgentStyleSheet));
}

#if ENABLE(VIDEO)
String RenderThemeBlackBerry::extraMediaControlsStyleSheet()
{
    return String(mediaControlsBlackBerryUserAgentStyleSheet, sizeof(mediaControlsBlackBerryUserAgentStyleSheet));
}

String RenderThemeBlackBerry::formatMediaControlsRemainingTime(float, float duration) const
{
    // This is a workaround to make the appearance of media time controller in
    // in-page mode the same as in fullscreen mode.
    return formatMediaControlsTime(duration);
}
#endif

double RenderThemeBlackBerry::caretBlinkInterval() const
{
    return 0; // Turn off caret blinking.
}

void RenderThemeBlackBerry::systemFont(int propId, FontDescription& fontDescription) const
{
    float fontSize = defaultFontSize;

    if (propId == CSSValueWebkitMiniControl || propId ==  CSSValueWebkitSmallControl || propId == CSSValueWebkitControl) {
        // Why 2 points smaller? Because that's what Gecko does. Note that we
        // are assuming a 96dpi screen, which is the default value we use on Windows.
        static const float pointsPerInch = 72.0f;
        static const float pixelsPerInch = 96.0f;
        fontSize -= (2.0f / pointsPerInch) * pixelsPerInch;
    }

    fontDescription.firstFamily().setFamily(defaultGUIFont());
    fontDescription.setSpecifiedSize(fontSize);
    fontDescription.setIsAbsoluteSize(true);
    fontDescription.setGenericFamily(FontDescription::NoFamily);
    fontDescription.setWeight(FontWeightNormal);
    fontDescription.setItalic(false);
}

void RenderThemeBlackBerry::setButtonStyle(RenderStyle* style) const
{
    Length vertPadding(int(style->fontSize() / paddingDivisor), Fixed);
    style->setPaddingTop(vertPadding);
    style->setPaddingBottom(vertPadding);
}

void RenderThemeBlackBerry::adjustButtonStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    setButtonStyle(style);
    style->setCursor(CURSOR_WEBKIT_GRAB);
}

void RenderThemeBlackBerry::adjustTextAreaStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    setButtonStyle(style);
}

bool RenderThemeBlackBerry::paintTextArea(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    return paintTextFieldOrTextAreaOrSearchField(object, info, rect);
}

void RenderThemeBlackBerry::adjustTextFieldStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    setButtonStyle(style);
}

bool RenderThemeBlackBerry::paintTextFieldOrTextAreaOrSearchField(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    ASSERT(info.context);
    GraphicsContext* context = info.context;

    context->save();
    context->setStrokeStyle(SolidStroke);
    context->setStrokeThickness(lineWidth);
    if (!isEnabled(object))
        context->setStrokeColor(disabledOutline, ColorSpaceDeviceRGB);
    else if (isPressed(object))
        info.context->setStrokeGradient(createLinearGradient(depressedTopOutline, depressedBottomOutline, rect.maxXMinYCorner(), rect.maxXMaxYCorner()));
    else if (isHovered(object) || isFocused(object))
        context->setStrokeGradient(createLinearGradient(hoverTopOutline, hoverBottomOutline, rect.maxXMinYCorner(), rect.maxXMaxYCorner()));
    else
        context->setStrokeGradient(createLinearGradient(regularTopOutline, regularBottomOutline, rect.maxXMinYCorner(), rect.maxXMaxYCorner()));

    Path textFieldRoundedRectangle = roundedRectForBorder(object, rect);
    if (object->style()->appearance() == SearchFieldPart) {
        // We force the fill color to White so as to match the background color of the search cancel button graphic.
        context->setFillColor(Color::white, ColorSpaceDeviceRGB);
        context->fillPath(textFieldRoundedRectangle);
        context->strokePath(textFieldRoundedRectangle);
    } else
        context->strokePath(textFieldRoundedRectangle);
    context->restore();
    return false;
}

bool RenderThemeBlackBerry::paintTextField(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    return paintTextFieldOrTextAreaOrSearchField(object, info, rect);
}

void RenderThemeBlackBerry::adjustSearchFieldStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    setButtonStyle(style);
}

void RenderThemeBlackBerry::adjustSearchFieldCancelButtonStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    static const float defaultControlFontPixelSize = 13;
    static const float defaultCancelButtonSize = 9;
    static const float minCancelButtonSize = 5;
    static const float maxCancelButtonSize = 21;

    // Scale the button size based on the font size
    float fontScale = style->fontSize() / defaultControlFontPixelSize;
    int cancelButtonSize = lroundf(std::min(std::max(minCancelButtonSize, defaultCancelButtonSize * fontScale), maxCancelButtonSize));
    Length length(cancelButtonSize, Fixed);
    style->setWidth(length);
    style->setHeight(length);
}

bool RenderThemeBlackBerry::paintSearchField(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    return paintTextFieldOrTextAreaOrSearchField(object, info, rect);
}

bool RenderThemeBlackBerry::paintSearchFieldCancelButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    ASSERT(object->parent());
    if (!object->parent() || !object->parent()->isBox())
        return false;

    RenderBox* parentRenderBox = toRenderBox(object->parent());

    IntRect parentBox = parentRenderBox->absoluteContentBox();
    IntRect bounds = rect;
    // Make sure the scaled button stays square and fits in its parent's box.
    bounds.setHeight(std::min(parentBox.width(), std::min(parentBox.height(), bounds.height())));
    bounds.setWidth(bounds.height());

    // Put the button in the middle vertically, and round up the value.
    // So if it has to be one pixel off-center, it would be one pixel closer
    // to the bottom of the field. This would look better with the text.
    bounds.setY(parentBox.y() + (parentBox.height() - bounds.height() + 1) / 2);

    static Image* cancelImage = Image::loadPlatformResource("searchCancel").leakRef();
    static Image* cancelPressedImage = Image::loadPlatformResource("searchCancelPressed").leakRef();
    paintInfo.context->drawImage(isPressed(object) ? cancelPressedImage : cancelImage, object->style()->colorSpace(), bounds);
    return false;
}

void RenderThemeBlackBerry::adjustMenuListButtonStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    // These seem to be reasonable padding values from observation.
    const int paddingLeft = 8;
    const int paddingRight = 4;

    const int minHeight = style->fontSize() * 2;

    style->resetPadding();
    style->setHeight(Length(Auto));

    style->setPaddingRight(Length(minHeight + paddingRight, Fixed));
    style->setPaddingLeft(Length(paddingLeft, Fixed));
    style->setCursor(CURSOR_WEBKIT_GRAB);
}

void RenderThemeBlackBerry::calculateButtonSize(RenderStyle* style) const
{
    int size = style->fontSize();
    Length length(size, Fixed);
    if (style->appearance() == CheckboxPart || style->appearance() == RadioPart) {
        style->setWidth(length);
        style->setHeight(length);
        return;
    }

    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    if (style->width().isIntrinsicOrAuto())
        style->setWidth(length);

    if (style->height().isAuto())
        style->setHeight(length);
}

bool RenderThemeBlackBerry::paintCheckbox(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    return paintButton(object, info, rect);
}

void RenderThemeBlackBerry::setCheckboxSize(RenderStyle* style) const
{
    calculateButtonSize(style);
}

bool RenderThemeBlackBerry::paintRadio(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    return paintButton(object, info, rect);
}

void RenderThemeBlackBerry::setRadioSize(RenderStyle* style) const
{
    calculateButtonSize(style);
}

// If this function returns false, WebCore assumes the button is fully decorated
bool RenderThemeBlackBerry::paintButton(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    ASSERT(info.context);
    info.context->save();

    info.context->setStrokeStyle(SolidStroke);
    info.context->setStrokeThickness(lineWidth);

    Color check(blackPen);
    if (!isEnabled(object)) {
        info.context->setFillGradient(createLinearGradient(disabledTop, disabledBottom, rect.maxXMinYCorner(), rect.maxXMaxYCorner()));
        info.context->setStrokeColor(disabledOutline, ColorSpaceDeviceRGB);
    } else if (isPressed(object)) {
        info.context->setFillGradient(createLinearGradient(depressedTop, depressedBottom, rect.maxXMinYCorner(), rect.maxXMaxYCorner()));
        info.context->setStrokeGradient(createLinearGradient(depressedTopOutline, depressedBottomOutline, rect.maxXMinYCorner(), rect.maxXMaxYCorner()));
    } else if (isHovered(object)) {
        info.context->setFillGradient(createLinearGradient(hoverTop, hoverBottom, rect.maxXMinYCorner(), rect.maxXMaxYCorner()));
        info.context->setStrokeGradient(createLinearGradient(hoverTopOutline, hoverBottomOutline, rect.maxXMinYCorner(), rect.maxXMaxYCorner()));
    } else {
        info.context->setFillGradient(createLinearGradient(regularTop, regularBottom, rect.maxXMinYCorner(), rect.maxXMaxYCorner()));
        info.context->setStrokeGradient(createLinearGradient(regularTopOutline, regularBottomOutline, rect.maxXMinYCorner(), rect.maxXMaxYCorner()));
    }

    ControlPart part = object->style()->appearance();
    switch (part) {
    case CheckboxPart: {
        FloatSize smallCorner(smallRadius, smallRadius);
        Path path;
        path.addRoundedRect(rect, smallCorner);
        info.context->fillPath(path);
        info.context->strokePath(path);

        if (isChecked(object)) {
            Path checkPath;
            IntRect rect2 = rect;
            rect2.inflate(-1);
            checkPath.moveTo(FloatPoint(rect2.x() + rect2.width() * checkboxLeftX, rect2.y() + rect2.height() * checkboxLeftY));
            checkPath.addLineTo(FloatPoint(rect2.x() + rect2.width() * checkboxMiddleX, rect2.maxY() - rect2.height() * checkboxMiddleY));
            checkPath.addLineTo(FloatPoint(rect2.x() + rect2.width() * checkboxRightX, rect2.y() + rect2.height() * checkboxRightY));
            info.context->setLineCap(RoundCap);
            info.context->setStrokeColor(blackPen, ColorSpaceDeviceRGB);
            info.context->setStrokeThickness(rect2.width() / checkboxStrokeThickness);
            info.context->fillPath(checkPath);
            info.context->strokePath(checkPath);
        }
        break;
    }
    case RadioPart:
        info.context->drawEllipse(rect);
        if (isChecked(object)) {
            IntRect rect2 = rect;
            rect2.inflate(-rect.width() * radioButtonCheckStateScaler);
            info.context->setFillColor(check, ColorSpaceDeviceRGB);
            info.context->setStrokeColor(check, ColorSpaceDeviceRGB);
            info.context->drawEllipse(rect2);
        }
        break;
    case ButtonPart:
    case PushButtonPart: {
        FloatSize largeCorner(largeRadius, largeRadius);
        Path path;
        path.addRoundedRect(rect, largeCorner);
        info.context->fillPath(path);
        info.context->strokePath(path);
        break;
    }
    case SquareButtonPart: {
        Path path;
        path.addRect(rect);
        info.context->fillPath(path);
        info.context->strokePath(path);
        break;
    }
    default:
        info.context->restore();
        return true;
    }

    info.context->restore();
    return false;
}

void RenderThemeBlackBerry::adjustMenuListStyle(StyleResolver* css, RenderStyle* style, Element* element) const
{
    adjustMenuListButtonStyle(css, style, element);
}

void RenderThemeBlackBerry::adjustCheckboxStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    setCheckboxSize(style);
    style->setBoxShadow(nullptr);
    Length margin(marginSize, Fixed);
    style->setMarginBottom(margin);
    style->setMarginRight(margin);
    style->setCursor(CURSOR_WEBKIT_GRAB);
}

void RenderThemeBlackBerry::adjustRadioStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    setRadioSize(style);
    style->setBoxShadow(nullptr);
    Length margin(marginSize, Fixed);
    style->setMarginBottom(margin);
    style->setMarginRight(margin);
    style->setCursor(CURSOR_WEBKIT_GRAB);
}

void RenderThemeBlackBerry::paintMenuListButtonGradientAndArrow(GraphicsContext* context, RenderObject* object, IntRect buttonRect, const Path& clipPath)
{
    ASSERT(context);
    context->save();
    if (!isEnabled(object))
        context->setFillGradient(createLinearGradient(disabledTop, disabledBottom, buttonRect.maxXMinYCorner(), buttonRect.maxXMaxYCorner()));
    else if (isPressed(object))
        context->setFillGradient(createLinearGradient(depressedTop, depressedBottom, buttonRect.maxXMinYCorner(), buttonRect.maxXMaxYCorner()));
    else if (isHovered(object))
        context->setFillGradient(createLinearGradient(hoverTop, hoverBottom, buttonRect.maxXMinYCorner(), buttonRect.maxXMaxYCorner()));
    else
        context->setFillGradient(createLinearGradient(regularTop, regularBottom, buttonRect.maxXMinYCorner(), buttonRect.maxXMaxYCorner()));

    // 1. Paint the background of the button.
    context->clip(clipPath);
    context->drawRect(buttonRect);
    context->restore();

    // 2. Paint the button arrow.
    buttonRect.inflate(-buttonRect.width() / 3);
    buttonRect.move(0, buttonRect.height() * 7 / 20);
    Path path;
    path.moveTo(FloatPoint(buttonRect.x(), buttonRect.y()));
    path.addLineTo(FloatPoint(buttonRect.x() + buttonRect.width(), buttonRect.y()));
    path.addLineTo(FloatPoint(buttonRect.x() + buttonRect.width() / 2.0, buttonRect.y() + buttonRect.height() / 2.0));
    path.closeSubpath();

    context->save();
    context->setStrokeStyle(SolidStroke);
    context->setStrokeThickness(lineWidth);
    context->setStrokeColor(Color::black, ColorSpaceDeviceRGB);
    context->setFillColor(Color::black, ColorSpaceDeviceRGB);
    context->setLineJoin(BevelJoin);
    context->fillPath(path);
    context->restore();
}

static IntRect computeMenuListArrowButtonRect(const IntRect& rect)
{
    // FIXME: The menu list arrow button should have a minimum and maximum width (to ensure usability) or
    // scale with respect to the font size used in the menu list control or some combination of both.
    return IntRect(IntPoint(rect.maxX() - rect.height(), rect.y()), IntSize(rect.height(), rect.height()));
}

static void paintMenuListBackground(GraphicsContext* context, const Path& menuListPath, const Color& backgroundColor)
{
    ASSERT(context);
    context->save();
    context->setFillColor(backgroundColor, ColorSpaceDeviceRGB);
    context->fillPath(menuListPath);
    context->restore();
}

bool RenderThemeBlackBerry::paintMenuList(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    // Note, this method is not called if the menu list explicitly specifies either a border or background color.
    // Instead, RenderThemeBlackBerry::paintMenuListButton is called. Therefore, when this method is called, we don't
    // have to adjust rect with respect to the border dimensions.

    ASSERT(info.context);
    GraphicsContext* context = info.context;

    Path menuListRoundedRectangle = roundedRectForBorder(object, rect);

    // 1. Paint the background of the entire control.
    paintMenuListBackground(context, menuListRoundedRectangle, Color::white);

    // 2. Paint the background of the button and its arrow.
    IntRect arrowButtonRectangle = computeMenuListArrowButtonRect(rect);
    paintMenuListButtonGradientAndArrow(context, object, arrowButtonRectangle, menuListRoundedRectangle);

    // 4. Stroke an outline around the entire control.
    context->save();
    context->setStrokeStyle(SolidStroke);
    context->setStrokeThickness(lineWidth);
    if (!isEnabled(object))
        context->setStrokeColor(disabledOutline, ColorSpaceDeviceRGB);
    else if (isPressed(object))
        context->setStrokeGradient(createLinearGradient(depressedTopOutline, depressedBottomOutline, rect.maxXMinYCorner(), rect.maxXMaxYCorner()));
    else if (isHovered(object))
        context->setStrokeGradient(createLinearGradient(hoverTopOutline, hoverBottomOutline, rect.maxXMinYCorner(), rect.maxXMaxYCorner()));
    else
        context->setStrokeGradient(createLinearGradient(regularTopOutline, regularBottomOutline, rect.maxXMinYCorner(), rect.maxXMaxYCorner()));

    context->strokePath(menuListRoundedRectangle);
    context->restore();
    return false;
}

bool RenderThemeBlackBerry::paintMenuListButton(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    // Note, this method is only called if the menu list explicitly specifies either a border or background color.
    // Otherwise, RenderThemeBlackBerry::paintMenuList is called. We need to fit the arrow button with the border box
    // of the menu-list so as to not occlude the custom border.

    // We compute menuListRoundedRectangle with respect to the dimensions of the entire menu-list control (i.e. rect) and
    // its border radius so that we clip the contour of the arrow button (when we paint it below) to match the contour of
    // the control.
    Path menuListRoundedRectangle = roundedRectForBorder(object, rect);

    // 1. Paint the background of the entire control.
    Color fillColor = object->style()->visitedDependentColor(CSSPropertyBackgroundColor);
    if (!fillColor.isValid())
        fillColor = Color::white;
    paintMenuListBackground(info.context, menuListRoundedRectangle, fillColor);

    // 2. Paint the background of the button and its arrow.
    IntRect bounds = IntRect(rect.x() + object->style()->borderLeftWidth(),
                         rect.y() + object->style()->borderTopWidth(),
                         rect.width() - object->style()->borderLeftWidth() - object->style()->borderRightWidth(),
                         rect.height() - object->style()->borderTopWidth() - object->style()->borderBottomWidth());

    IntRect arrowButtonRectangle = computeMenuListArrowButtonRect(bounds); // Fit the arrow button within the border box of the menu-list.
    paintMenuListButtonGradientAndArrow(info.context, object, arrowButtonRectangle, menuListRoundedRectangle);
    return false;
}

void RenderThemeBlackBerry::adjustSliderThumbSize(RenderStyle* style, Element* element) const
{
    float fullScreenMultiplier = 1;
    ControlPart part = style->appearance();

    if (part == MediaSliderThumbPart || part == MediaVolumeSliderThumbPart) {
        RenderSlider* slider = determineRenderSlider(element->renderer());
        if (slider)
            fullScreenMultiplier = determineFullScreenMultiplier(toElement(slider->node()));
    }

    if (part == MediaVolumeSliderThumbPart || part == SliderThumbHorizontalPart || part == SliderThumbVerticalPart) {
        style->setWidth(Length((part == SliderThumbVerticalPart ? sliderThumbHeight : sliderThumbWidth) * fullScreenMultiplier, Fixed));
        style->setHeight(Length((part == SliderThumbVerticalPart ? sliderThumbWidth : sliderThumbHeight) * fullScreenMultiplier, Fixed));
    } else if (part == MediaSliderThumbPart) {
        style->setWidth(Length(mediaSliderThumbWidth * fullScreenMultiplier, Fixed));
        style->setHeight(Length(mediaSliderThumbHeight * fullScreenMultiplier, Fixed));
    }
}

bool RenderThemeBlackBerry::paintSliderTrack(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    const static int SliderTrackHeight = 5;
    IntRect rect2;
    if (object->style()->appearance() == SliderHorizontalPart) {
        rect2.setHeight(SliderTrackHeight);
        rect2.setWidth(rect.width());
        rect2.setX(rect.x());
        rect2.setY(rect.y() + (rect.height() - SliderTrackHeight) / 2);
    } else {
        rect2.setHeight(rect.height());
        rect2.setWidth(SliderTrackHeight);
        rect2.setX(rect.x() + (rect.width() - SliderTrackHeight) / 2);
        rect2.setY(rect.y());
    }
    return paintSliderTrackRect(object, info, rect2);
}

bool RenderThemeBlackBerry::paintSliderTrackRect(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    return paintSliderTrackRect(object, info, rect, rangeSliderRegularTopOutline, rangeSliderRegularBottomOutline,
                rangeSliderRegularTop, rangeSliderRegularBottom);
}

bool RenderThemeBlackBerry::paintSliderTrackRect(RenderObject* object, const PaintInfo& info, const IntRect& rect,
        RGBA32 strokeColorStart, RGBA32 strokeColorEnd, RGBA32 fillColorStart, RGBA32 fillColorEnd)
{
    FloatSize smallCorner(smallRadius, smallRadius);

    info.context->save();
    info.context->setStrokeStyle(SolidStroke);
    info.context->setStrokeThickness(lineWidth);

    info.context->setStrokeGradient(createLinearGradient(strokeColorStart, strokeColorEnd, rect.maxXMinYCorner(), rect. maxXMaxYCorner()));
    info.context->setFillGradient(createLinearGradient(fillColorStart, fillColorEnd, rect.maxXMinYCorner(), rect.maxXMaxYCorner()));

    Path path;
    path.addRoundedRect(rect, smallCorner);
    info.context->fillPath(path);

    info.context->restore();
    return false;
}

bool RenderThemeBlackBerry::paintSliderThumb(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    FloatSize largeCorner(largeRadius, largeRadius);

    info.context->save();
    info.context->setStrokeStyle(SolidStroke);
    info.context->setStrokeThickness(lineWidth);

    if (isPressed(object) || isHovered(object)) {
        info.context->setStrokeGradient(createLinearGradient(hoverTopOutline, hoverBottomOutline, rect.maxXMinYCorner(), rect. maxXMaxYCorner()));
        info.context->setFillGradient(createLinearGradient(hoverTop, hoverBottom, rect.maxXMinYCorner(), rect.maxXMaxYCorner()));
    } else {
        info.context->setStrokeGradient(createLinearGradient(regularTopOutline, regularBottomOutline, rect.maxXMinYCorner(), rect. maxXMaxYCorner()));
        info.context->setFillGradient(createLinearGradient(regularTop, regularBottom, rect.maxXMinYCorner(), rect.maxXMaxYCorner()));
    }

    Path path;
    path.addRoundedRect(rect, largeCorner);
    info.context->fillPath(path);

    bool isVertical = rect.width() > rect.height();
    IntPoint startPoint(rect.x() + (isVertical ? 5 : 2), rect.y() + (isVertical ? 2 : 5));
    IntPoint endPoint(rect.x() + (isVertical ? 20 : 2), rect.y() + (isVertical ? 2 : 20));
    const int lineOffset = 2;
    const int shadowOffset = 1;

    for (int i = 0; i < 3; i++) {
        if (isVertical) {
            startPoint.setY(startPoint.y() + lineOffset);
            endPoint.setY(endPoint.y() + lineOffset);
        } else {
            startPoint.setX(startPoint.x() + lineOffset);
            endPoint.setX(endPoint.x() + lineOffset);
        }
        if (isPressed(object) || isHovered(object))
            info.context->setStrokeColor(dragRollLight, ColorSpaceDeviceRGB);
        else
            info.context->setStrokeColor(dragRegularLight, ColorSpaceDeviceRGB);
        info.context->drawLine(startPoint, endPoint);

        if (isVertical) {
            startPoint.setY(startPoint.y() + shadowOffset);
            endPoint.setY(endPoint.y() + shadowOffset);
        } else {
            startPoint.setX(startPoint.x() + shadowOffset);
            endPoint.setX(endPoint.x() + shadowOffset);
        }
        if (isPressed(object) || isHovered(object))
            info.context->setStrokeColor(dragRollDark, ColorSpaceDeviceRGB);
        else
            info.context->setStrokeColor(dragRegularDark, ColorSpaceDeviceRGB);
        info.context->drawLine(startPoint, endPoint);
    }
    info.context->restore();
    return false;
}

void RenderThemeBlackBerry::adjustMediaControlStyle(StyleResolver*, RenderStyle* style, Element* element) const
{
    float fullScreenMultiplier = determineFullScreenMultiplier(element);

    // We use multiples of mediaControlsHeight to make all objects scale evenly
    Length controlsHeight(mediaControlsHeight * fullScreenMultiplier, Fixed);
    Length timeWidth(mediaControlsHeight * 3 / 2 * fullScreenMultiplier, Fixed);
    Length volumeHeight(mediaControlsHeight * 4 * fullScreenMultiplier, Fixed);
    Length padding(mediaControlsHeight / 8 * fullScreenMultiplier, Fixed);
    float fontSize = mediaControlsHeight / 2 * fullScreenMultiplier;

    switch (style->appearance()) {
    case MediaPlayButtonPart:
    case MediaEnterFullscreenButtonPart:
    case MediaExitFullscreenButtonPart:
    case MediaMuteButtonPart:
        style->setWidth(controlsHeight);
        style->setHeight(controlsHeight);
        break;
    case MediaCurrentTimePart:
    case MediaTimeRemainingPart:
        style->setWidth(timeWidth);
        style->setHeight(controlsHeight);
        style->setPaddingRight(padding);
        style->setBlendedFontSize(fontSize);
        break;
    case MediaVolumeSliderContainerPart:
        style->setWidth(controlsHeight);
        style->setHeight(volumeHeight);
        style->setBottom(controlsHeight);
        break;
    default:
        break;
    }
}

void RenderThemeBlackBerry::adjustSliderTrackStyle(StyleResolver*, RenderStyle* style, Element* element) const
{
    float fullScreenMultiplier = determineFullScreenMultiplier(element);

    // We use multiples of mediaControlsHeight to make all objects scale evenly
    Length controlsHeight(mediaControlsHeight * fullScreenMultiplier, Fixed);
    Length volumeHeight(mediaControlsHeight * 4 * fullScreenMultiplier, Fixed);
    switch (style->appearance()) {
    case MediaSliderPart:
        style->setHeight(controlsHeight);
        break;
    case MediaVolumeSliderPart:
        style->setWidth(controlsHeight);
        style->setHeight(volumeHeight);
        break;
    case MediaFullScreenVolumeSliderPart:
    default:
        break;
    }
}

static bool paintMediaButton(GraphicsContext* context, const IntRect& rect, Image* image)
{
    context->drawImage(image, ColorSpaceDeviceRGB, rect);
    return false;
}

bool RenderThemeBlackBerry::paintMediaPlayButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    HTMLMediaElement* mediaElement = toParentMediaElement(object);

    if (!mediaElement)
        return false;

    static Image* mediaPlay = Image::loadPlatformResource("play").leakRef();
    static Image* mediaPause = Image::loadPlatformResource("pause").leakRef();

    return paintMediaButton(paintInfo.context, rect, mediaElement->canPlay() ? mediaPlay : mediaPause);
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

bool RenderThemeBlackBerry::paintMediaMuteButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    HTMLMediaElement* mediaElement = toParentMediaElement(object);

    if (!mediaElement)
        return false;

    static Image* mediaMute = Image::loadPlatformResource("speaker").leakRef();
    static Image* mediaUnmute = Image::loadPlatformResource("speaker_mute").leakRef();

    return paintMediaButton(paintInfo.context, rect, mediaElement->muted() || !mediaElement->volume() ? mediaUnmute : mediaMute);
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

bool RenderThemeBlackBerry::paintMediaFullscreenButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    HTMLMediaElement* mediaElement = toParentMediaElement(object);
    if (!mediaElement)
        return false;

    static Image* mediaEnterFullscreen = Image::loadPlatformResource("fullscreen").leakRef();
    static Image* mediaExitFullscreen = Image::loadPlatformResource("exit_fullscreen").leakRef();

    Image* buttonImage = mediaEnterFullscreen;
#if ENABLE(FULLSCREEN_API)
    if (mediaElement->document()->webkitIsFullScreen() && mediaElement->document()->webkitCurrentFullScreenElement() == mediaElement)
        buttonImage = mediaExitFullscreen;
#endif
    return paintMediaButton(paintInfo.context, rect, buttonImage);
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

bool RenderThemeBlackBerry::paintMediaSliderTrack(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    HTMLMediaElement* mediaElement = toParentMediaElement(object);
    if (!mediaElement)
        return false;

    float fullScreenMultiplier = determineFullScreenMultiplier(mediaElement);
    float loaded = 0;
    // FIXME: replace loaded with commented out one when buffer bug is fixed (see comment in
    // MediaPlayerPrivateMMrenderer::percentLoaded).
    // loaded = mediaElement->percentLoaded();
    if (mediaElement->player() && mediaElement->player()->implementation())
        loaded = static_cast<MediaPlayerPrivate *>(mediaElement->player()->implementation())->percentLoaded();
    float position = mediaElement->duration() > 0 ? (mediaElement->currentTime() / mediaElement->duration()) : 0;

    int x = ceil(rect.x() + 2 * fullScreenMultiplier - fullScreenMultiplier / 2);
    int y = ceil(rect.y() + 14 * fullScreenMultiplier + fullScreenMultiplier / 2);
    int w = ceil(rect.width() - 4 * fullScreenMultiplier + fullScreenMultiplier / 2);
    int h = ceil(2 * fullScreenMultiplier);
    IntRect rect2(x, y, w, h);

    int wPlayed = ceil(w * position);
    int wLoaded = ceil((w - mediaSliderThumbWidth * fullScreenMultiplier) * loaded + mediaSliderThumbWidth * fullScreenMultiplier);

    IntRect played(x, y, wPlayed, h);
    IntRect buffered(x, y, wLoaded, h);

    // This is to paint main slider bar.
    bool result = paintSliderTrackRect(object, paintInfo, rect2);

    if (loaded > 0 || position > 0) {
        // This is to paint buffered bar.
        paintSliderTrackRect(object, paintInfo, buffered, Color::darkGray, Color::darkGray, Color::darkGray, Color::darkGray);

        // This is to paint played part of bar (left of slider thumb) using selection color.
        paintSliderTrackRect(object, paintInfo, played, selection, selection, selection, selection);
    }
    return result;

#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

bool RenderThemeBlackBerry::paintMediaSliderThumb(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    RenderSlider* slider = determineRenderSlider(object);
    if (!slider)
        return false;

    float fullScreenMultiplier = determineFullScreenMultiplier(toElement(slider->node()));

    paintInfo.context->save();
    Path mediaThumbRoundedRectangle;
    mediaThumbRoundedRectangle.addRoundedRect(rect, FloatSize(mediaSliderThumbRadius * fullScreenMultiplier, mediaSliderThumbRadius * fullScreenMultiplier));
    paintInfo.context->setStrokeStyle(SolidStroke);
    paintInfo.context->setStrokeThickness(0.5);
    paintInfo.context->setStrokeColor(Color::black, ColorSpaceDeviceRGB);

    if (isPressed(object) || isHovered(object) || slider->inDragMode()) {
        paintInfo.context->setFillGradient(createLinearGradient(selection, Color(selection).dark().rgb(),
                rect.maxXMinYCorner(), rect.maxXMaxYCorner()));
    } else {
        paintInfo.context->setFillGradient(createLinearGradient(Color::white, Color(Color::white).dark().rgb(),
                rect.maxXMinYCorner(), rect.maxXMaxYCorner()));
    }
    paintInfo.context->fillPath(mediaThumbRoundedRectangle);
    paintInfo.context->restore();

    return true;
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

bool RenderThemeBlackBerry::paintMediaVolumeSliderTrack(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    float pad = rect.width() * 0.45;
    float x = rect.x() + pad;
    float y = rect.y() + pad;
    float width = rect.width() * 0.1;
    float height = rect.height() - (2.0 * pad);

    IntRect rect2(x, y, width, height);

    return paintSliderTrackRect(object, paintInfo, rect2);
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

bool RenderThemeBlackBerry::paintMediaVolumeSliderThumb(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    static Image* mediaVolumeThumb = Image::loadPlatformResource("volume_thumb").leakRef();

    return paintMediaButton(paintInfo.context, rect, mediaVolumeThumb);
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

Color RenderThemeBlackBerry::platformFocusRingColor() const
{
    return focusRingPen;
}

#if ENABLE(TOUCH_EVENTS)
Color RenderThemeBlackBerry::platformTapHighlightColor() const
{
    return Color(0, 168, 223, 50);
}
#endif

Color RenderThemeBlackBerry::platformActiveSelectionBackgroundColor() const
{
    return Color(0, 168, 223, 50);
}

double RenderThemeBlackBerry::animationRepeatIntervalForProgressBar(RenderProgress* renderProgress) const
{
    return renderProgress->isDeterminate() ? 0.0 : 0.1;
}

double RenderThemeBlackBerry::animationDurationForProgressBar(RenderProgress* renderProgress) const
{
    return renderProgress->isDeterminate() ? 0.0 : 2.0;
}

bool RenderThemeBlackBerry::paintProgressBar(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    if (!object->isProgress())
        return true;

    RenderProgress* renderProgress = toRenderProgress(object);

    FloatSize smallCorner(smallRadius, smallRadius);

    info.context->save();
    info.context->setStrokeStyle(SolidStroke);
    info.context->setStrokeThickness(lineWidth);

    info.context->setStrokeGradient(createLinearGradient(rangeSliderRegularTopOutline, rangeSliderRegularBottomOutline, rect.maxXMinYCorner(), rect.maxXMaxYCorner()));
    info.context->setFillGradient(createLinearGradient(rangeSliderRegularTop, rangeSliderRegularBottom, rect.maxXMinYCorner(), rect.maxXMaxYCorner()));

    Path path;
    path.addRoundedRect(rect, smallCorner);
    info.context->fillPath(path);

    IntRect rect2 = rect;
    rect2.setX(rect2.x() + 1);
    rect2.setHeight(rect2.height() - 2);
    rect2.setY(rect2.y() + 1);
    info.context->setStrokeStyle(NoStroke);
    info.context->setStrokeThickness(0);
    if (renderProgress->isDeterminate()) {
        rect2.setWidth(rect2.width() * renderProgress->position() - 2);
        info.context->setFillGradient(createLinearGradient(progressRegularTop, progressRegularBottom, rect2.maxXMinYCorner(), rect2.maxXMaxYCorner()));
    } else {
        // Animating
        rect2.setWidth(rect2.width() - 2);
        RefPtr<Gradient> gradient = Gradient::create(rect2.minXMaxYCorner(), rect2.maxXMaxYCorner());
        gradient->addColorStop(0.0, Color(progressRegularBottom));
        gradient->addColorStop(renderProgress->animationProgress(), Color(progressRegularTop));
        gradient->addColorStop(1.0, Color(progressRegularBottom));
        info.context->setFillGradient(gradient);
    }
    Path path2;
    path2.addRoundedRect(rect2, smallCorner);
    info.context->fillPath(path2);

    info.context->restore();
    return false;
}

Color RenderThemeBlackBerry::platformActiveTextSearchHighlightColor() const
{
    return Color(255, 150, 50); // Orange.
}

Color RenderThemeBlackBerry::platformInactiveTextSearchHighlightColor() const
{
    return Color(255, 255, 0); // Yellow.
}

} // namespace WebCore
