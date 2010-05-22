/*
 * Copyright (C) 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2008, 2009 Google Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderThemeChromiumSkia.h"

#include "ChromiumBridge.h"
#include "CSSValueKeywords.h"
#include "GraphicsContext.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "Image.h"
#include "MediaControlElements.h"
#include "PlatformContextSkia.h"
#include "RenderBox.h"
#include "RenderMediaControlsChromium.h"
#include "RenderObject.h"
#include "RenderSlider.h"
#include "ScrollbarTheme.h"
#include "TimeRanges.h"
#include "TransformationMatrix.h"
#include "UserAgentStyleSheets.h"

#include "SkShader.h"
#include "SkGradientShader.h"

namespace WebCore {

enum PaddingType {
    TopPadding,
    RightPadding,
    BottomPadding,
    LeftPadding
};

static const int styledMenuListInternalPadding[4] = { 1, 4, 1, 4 };

// These values all match Safari/Win.
static const float defaultControlFontPixelSize = 13;
static const float defaultCancelButtonSize = 9;
static const float minCancelButtonSize = 5;
static const float maxCancelButtonSize = 21;
static const float defaultSearchFieldResultsDecorationSize = 13;
static const float minSearchFieldResultsDecorationSize = 9;
static const float maxSearchFieldResultsDecorationSize = 30;
static const float defaultSearchFieldResultsButtonWidth = 18;

static void setSizeIfAuto(RenderStyle* style, const IntSize& size)
{
    if (style->width().isIntrinsicOrAuto())
        style->setWidth(Length(size.width(), Fixed));
    if (style->height().isAuto())
        style->setHeight(Length(size.height(), Fixed));
}

static void drawVertLine(SkCanvas* canvas, int x, int y1, int y2, const SkPaint& paint)
{
    SkIRect skrect;
    skrect.set(x, y1, x + 1, y2 + 1);
    canvas->drawIRect(skrect, paint);
}

static void drawHorizLine(SkCanvas* canvas, int x1, int x2, int y, const SkPaint& paint)
{
    SkIRect skrect;
    skrect.set(x1, y, x2 + 1, y + 1);
    canvas->drawIRect(skrect, paint);
}

static void drawBox(SkCanvas* canvas, const IntRect& rect, const SkPaint& paint)
{
    const int right = rect.x() + rect.width() - 1;
    const int bottom = rect.y() + rect.height() - 1;
    drawHorizLine(canvas, rect.x(), right, rect.y(), paint);
    drawVertLine(canvas, right, rect.y(), bottom, paint);
    drawHorizLine(canvas, rect.x(), right, bottom, paint);
    drawVertLine(canvas, rect.x(), rect.y(), bottom, paint);
}

// We aim to match IE here.
// -IE uses a font based on the encoding as the default font for form controls.
// -Gecko uses MS Shell Dlg (actually calls GetStockObject(DEFAULT_GUI_FONT),
// which returns MS Shell Dlg)
// -Safari uses Lucida Grande.
//
// FIXME: The only case where we know we don't match IE is for ANSI encodings.
// IE uses MS Shell Dlg there, which we render incorrectly at certain pixel
// sizes (e.g. 15px). So, for now we just use Arial.
const String& RenderThemeChromiumSkia::defaultGUIFont()
{
    DEFINE_STATIC_LOCAL(String, fontFace, ("Arial"));
    return fontFace;
}

float RenderThemeChromiumSkia::defaultFontSize = 16.0;

RenderThemeChromiumSkia::RenderThemeChromiumSkia()
{
}

RenderThemeChromiumSkia::~RenderThemeChromiumSkia()
{
}

// Use the Windows style sheets to match their metrics.
String RenderThemeChromiumSkia::extraDefaultStyleSheet()
{
    return String(themeWinUserAgentStyleSheet, sizeof(themeWinUserAgentStyleSheet));
}

String RenderThemeChromiumSkia::extraQuirksStyleSheet()
{
    return String(themeWinQuirksUserAgentStyleSheet, sizeof(themeWinQuirksUserAgentStyleSheet));
}

#if ENABLE(VIDEO)
String RenderThemeChromiumSkia::extraMediaControlsStyleSheet()
{
    return String(mediaControlsChromiumUserAgentStyleSheet, sizeof(mediaControlsChromiumUserAgentStyleSheet));
}
#endif

bool RenderThemeChromiumSkia::supportsHover(const RenderStyle* style) const
{
    return true;
}

bool RenderThemeChromiumSkia::supportsFocusRing(const RenderStyle* style) const
{
    // This causes WebKit to draw the focus rings for us.
    return false;
}

Color RenderThemeChromiumSkia::platformActiveSelectionBackgroundColor() const
{
    return Color(0x1e, 0x90, 0xff);
}

Color RenderThemeChromiumSkia::platformInactiveSelectionBackgroundColor() const
{
    return Color(0xc8, 0xc8, 0xc8);
}

Color RenderThemeChromiumSkia::platformActiveSelectionForegroundColor() const
{
    return Color::black;
}

Color RenderThemeChromiumSkia::platformInactiveSelectionForegroundColor() const
{
    return Color(0x32, 0x32, 0x32);
}

Color RenderThemeChromiumSkia::platformFocusRingColor() const
{
    static Color focusRingColor(229, 151, 0, 255);
    return focusRingColor;
}

double RenderThemeChromiumSkia::caretBlinkInterval() const
{
    // Disable the blinking caret in layout test mode, as it introduces
    // a race condition for the pixel tests. http://b/1198440
    if (ChromiumBridge::layoutTestMode())
        return 0;

    return caretBlinkIntervalInternal();
}

void RenderThemeChromiumSkia::systemFont(int propId, FontDescription& fontDescription) const
{
    float fontSize = defaultFontSize;

    switch (propId) {
    case CSSValueWebkitMiniControl:
    case CSSValueWebkitSmallControl:
    case CSSValueWebkitControl:
        // Why 2 points smaller? Because that's what Gecko does. Note that we
        // are assuming a 96dpi screen, which is the default that we use on
        // Windows.
        static const float pointsPerInch = 72.0f;
        static const float pixelsPerInch = 96.0f;
        fontSize -= (2.0f / pointsPerInch) * pixelsPerInch;
        break;
    }

    fontDescription.firstFamily().setFamily(defaultGUIFont());
    fontDescription.setSpecifiedSize(fontSize);
    fontDescription.setIsAbsoluteSize(true);
    fontDescription.setGenericFamily(FontDescription::NoFamily);
    fontDescription.setWeight(FontWeightNormal);
    fontDescription.setItalic(false);
}

int RenderThemeChromiumSkia::minimumMenuListSize(RenderStyle* style) const
{
    return 0;
}

// These are the default dimensions of radio buttons and checkboxes.
static const int widgetStandardWidth = 13;
static const int widgetStandardHeight = 13;

// Return a rectangle that has the same center point as |original|, but with a
// size capped at |width| by |height|.
IntRect center(const IntRect& original, int width, int height)
{
    width = std::min(original.width(), width);
    height = std::min(original.height(), height);
    int x = original.x() + (original.width() - width) / 2;
    int y = original.y() + (original.height() - height) / 2;

    return IntRect(x, y, width, height);
}

bool RenderThemeChromiumSkia::paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    static Image* const checkedImage = Image::loadPlatformResource("linuxCheckboxOn").releaseRef();
    static Image* const uncheckedImage = Image::loadPlatformResource("linuxCheckboxOff").releaseRef();
    static Image* const disabledCheckedImage = Image::loadPlatformResource("linuxCheckboxDisabledOn").releaseRef();
    static Image* const disabledUncheckedImage = Image::loadPlatformResource("linuxCheckboxDisabledOff").releaseRef();

    Image* image;

    if (this->isEnabled(o))
        image = this->isChecked(o) ? checkedImage : uncheckedImage;
    else
        image = this->isChecked(o) ? disabledCheckedImage : disabledUncheckedImage;

    i.context->drawImage(image, o->style()->colorSpace(), center(rect, widgetStandardHeight, widgetStandardWidth));
    return false;
}

void RenderThemeChromiumSkia::setCheckboxSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    // FIXME:  A hard-coded size of 13 is used.  This is wrong but necessary
    // for now.  It matches Firefox.  At different DPI settings on Windows,
    // querying the theme gives you a larger size that accounts for the higher
    // DPI.  Until our entire engine honors a DPI setting other than 96, we
    // can't rely on the theme's metrics.
    const IntSize size(widgetStandardHeight, widgetStandardWidth);
    setSizeIfAuto(style, size);
}

bool RenderThemeChromiumSkia::paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    static Image* const checkedImage = Image::loadPlatformResource("linuxRadioOn").releaseRef();
    static Image* const uncheckedImage = Image::loadPlatformResource("linuxRadioOff").releaseRef();
    static Image* const disabledCheckedImage = Image::loadPlatformResource("linuxRadioDisabledOn").releaseRef();
    static Image* const disabledUncheckedImage = Image::loadPlatformResource("linuxRadioDisabledOff").releaseRef();

    Image* image;
    if (this->isEnabled(o))
        image = this->isChecked(o) ? checkedImage : uncheckedImage;
    else
        image = this->isChecked(o) ? disabledCheckedImage : disabledUncheckedImage;

    i.context->drawImage(image, o->style()->colorSpace(), center(rect, widgetStandardHeight, widgetStandardWidth));
    return false;
}

void RenderThemeChromiumSkia::setRadioSize(RenderStyle* style) const
{
    // Use same sizing for radio box as checkbox.
    setCheckboxSize(style);
}

static SkColor brightenColor(double h, double s, double l, float brightenAmount)
{
    l += brightenAmount;
    if (l > 1.0)
        l = 1.0;
    if (l < 0.0)
        l = 0.0;

    return makeRGBAFromHSLA(h, s, l, 1.0);
}

static void paintButtonLike(RenderTheme* theme, RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    SkCanvas* const canvas = i.context->platformContext()->canvas();
    SkPaint paint;
    SkRect skrect;
    const int right = rect.x() + rect.width();
    const int bottom = rect.y() + rect.height();
    SkColor baseColor = SkColorSetARGB(0xff, 0xdd, 0xdd, 0xdd);
    if (o->hasBackground())
        baseColor = o->style()->visitedDependentColor(CSSPropertyBackgroundColor).rgb();
    double h, s, l;
    Color(baseColor).getHSL(h, s, l);
    // Our standard gradient is from 0xdd to 0xf8. This is the amount of
    // increased luminance between those values.
    SkColor lightColor(brightenColor(h, s, l, 0.105));

    // If the button is too small, fallback to drawing a single, solid color
    if (rect.width() < 5 || rect.height() < 5) {
        paint.setColor(baseColor);
        skrect.set(rect.x(), rect.y(), right, bottom);
        canvas->drawRect(skrect, paint);
        return;
    }

    const int borderAlpha = theme->isHovered(o) ? 0x80 : 0x55;
    paint.setARGB(borderAlpha, 0, 0, 0);
    canvas->drawLine(rect.x() + 1, rect.y(), right - 1, rect.y(), paint);
    canvas->drawLine(right - 1, rect.y() + 1, right - 1, bottom - 1, paint);
    canvas->drawLine(rect.x() + 1, bottom - 1, right - 1, bottom - 1, paint);
    canvas->drawLine(rect.x(), rect.y() + 1, rect.x(), bottom - 1, paint);

    paint.setColor(SK_ColorBLACK);
    SkPoint p[2];
    const int lightEnd = theme->isPressed(o) ? 1 : 0;
    const int darkEnd = !lightEnd;
    p[lightEnd].set(SkIntToScalar(rect.x()), SkIntToScalar(rect.y()));
    p[darkEnd].set(SkIntToScalar(rect.x()), SkIntToScalar(bottom - 1));
    SkColor colors[2];
    colors[0] = lightColor;
    colors[1] = baseColor;

    SkShader* shader = SkGradientShader::CreateLinear(
        p, colors, NULL, 2, SkShader::kClamp_TileMode, NULL);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setShader(shader);
    shader->unref();

    skrect.set(rect.x() + 1, rect.y() + 1, right - 1, bottom - 1);
    canvas->drawRect(skrect, paint);

    paint.setShader(NULL);
    paint.setColor(brightenColor(h, s, l, -0.0588));
    canvas->drawPoint(rect.x() + 1, rect.y() + 1, paint);
    canvas->drawPoint(right - 2, rect.y() + 1, paint);
    canvas->drawPoint(rect.x() + 1, bottom - 2, paint);
    canvas->drawPoint(right - 2, bottom - 2, paint);
}

bool RenderThemeChromiumSkia::paintButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    paintButtonLike(this, o, i, rect);
    return false;
}

void RenderThemeChromiumSkia::adjustButtonStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    if (style->appearance() == PushButtonPart) {
        // Ignore line-height.
        style->setLineHeight(RenderStyle::initialLineHeight());
    }
}


bool RenderThemeChromiumSkia::paintTextField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return true;
}

bool RenderThemeChromiumSkia::paintTextArea(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

void RenderThemeChromiumSkia::adjustSearchFieldStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
     // Ignore line-height.
     style->setLineHeight(RenderStyle::initialLineHeight());
}

bool RenderThemeChromiumSkia::paintSearchField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

void RenderThemeChromiumSkia::adjustSearchFieldCancelButtonStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    // Scale the button size based on the font size
    float fontScale = style->fontSize() / defaultControlFontPixelSize;
    int cancelButtonSize = lroundf(std::min(std::max(minCancelButtonSize, defaultCancelButtonSize * fontScale), maxCancelButtonSize));
    style->setWidth(Length(cancelButtonSize, Fixed));
    style->setHeight(Length(cancelButtonSize, Fixed));
}

IntRect RenderThemeChromiumSkia::convertToPaintingRect(RenderObject* inputRenderer, const RenderObject* partRenderer, IntRect partRect, const IntRect& localOffset) const
{
    // Compute an offset between the part renderer and the input renderer.
    IntSize offsetFromInputRenderer = -(partRenderer->offsetFromAncestorContainer(inputRenderer));
    // Move the rect into partRenderer's coords.
    partRect.move(offsetFromInputRenderer);
    // Account for the local drawing offset.
    partRect.move(localOffset.x(), localOffset.y());

    return partRect;
}

bool RenderThemeChromiumSkia::paintSearchFieldCancelButton(RenderObject* cancelButtonObject, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    // Get the renderer of <input> element.
    Node* input = cancelButtonObject->node()->shadowAncestorNode();
    if (!input->renderer()->isBox())
        return false;
    RenderBox* inputRenderBox = toRenderBox(input->renderer());
    IntRect inputContentBox = inputRenderBox->contentBoxRect();

    // Make sure the scaled button stays square and will fit in its parent's box.
    int cancelButtonSize = std::min(inputContentBox.width(), std::min(inputContentBox.height(), r.height()));
    // Calculate cancel button's coordinates relative to the input element.
    // Center the button vertically.  Round up though, so if it has to be one pixel off-center, it will
    // be one pixel closer to the bottom of the field.  This tends to look better with the text.
    IntRect cancelButtonRect(cancelButtonObject->offsetFromAncestorContainer(inputRenderBox).width(),
                             inputContentBox.y() + (inputContentBox.height() - cancelButtonSize + 1) / 2,
                             cancelButtonSize, cancelButtonSize);
    IntRect paintingRect = convertToPaintingRect(inputRenderBox, cancelButtonObject, cancelButtonRect, r);

    static Image* cancelImage = Image::loadPlatformResource("searchCancel").releaseRef();
    static Image* cancelPressedImage = Image::loadPlatformResource("searchCancelPressed").releaseRef();
    paintInfo.context->drawImage(isPressed(cancelButtonObject) ? cancelPressedImage : cancelImage,
                                 cancelButtonObject->style()->colorSpace(), paintingRect);
    return false;
}

void RenderThemeChromiumSkia::adjustSearchFieldDecorationStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    IntSize emptySize(1, 11);
    style->setWidth(Length(emptySize.width(), Fixed));
    style->setHeight(Length(emptySize.height(), Fixed));
}

void RenderThemeChromiumSkia::adjustSearchFieldResultsDecorationStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    // Scale the decoration size based on the font size
    float fontScale = style->fontSize() / defaultControlFontPixelSize;
    int magnifierSize = lroundf(std::min(std::max(minSearchFieldResultsDecorationSize, defaultSearchFieldResultsDecorationSize * fontScale),
                                         maxSearchFieldResultsDecorationSize));
    style->setWidth(Length(magnifierSize, Fixed));
    style->setHeight(Length(magnifierSize, Fixed));
}

bool RenderThemeChromiumSkia::paintSearchFieldResultsDecoration(RenderObject* magnifierObject, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    // Get the renderer of <input> element.
    Node* input = magnifierObject->node()->shadowAncestorNode();
    if (!input->renderer()->isBox())
        return false;
    RenderBox* inputRenderBox = toRenderBox(input->renderer());
    IntRect inputContentBox = inputRenderBox->contentBoxRect();

    // Make sure the scaled decoration stays square and will fit in its parent's box.
    int magnifierSize = std::min(inputContentBox.width(), std::min(inputContentBox.height(), r.height()));
    // Calculate decoration's coordinates relative to the input element.
    // Center the decoration vertically.  Round up though, so if it has to be one pixel off-center, it will
    // be one pixel closer to the bottom of the field.  This tends to look better with the text.
    IntRect magnifierRect(magnifierObject->offsetFromAncestorContainer(inputRenderBox).width(),
                          inputContentBox.y() + (inputContentBox.height() - magnifierSize + 1) / 2,
                          magnifierSize, magnifierSize);
    IntRect paintingRect = convertToPaintingRect(inputRenderBox, magnifierObject, magnifierRect, r);

    static Image* magnifierImage = Image::loadPlatformResource("searchMagnifier").releaseRef();
    paintInfo.context->drawImage(magnifierImage, magnifierObject->style()->colorSpace(), paintingRect);
    return false;
}

void RenderThemeChromiumSkia::adjustSearchFieldResultsButtonStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    // Scale the button size based on the font size
    float fontScale = style->fontSize() / defaultControlFontPixelSize;
    int magnifierHeight = lroundf(std::min(std::max(minSearchFieldResultsDecorationSize, defaultSearchFieldResultsDecorationSize * fontScale),
                                           maxSearchFieldResultsDecorationSize));
    int magnifierWidth = lroundf(magnifierHeight * defaultSearchFieldResultsButtonWidth / defaultSearchFieldResultsDecorationSize);
    style->setWidth(Length(magnifierWidth, Fixed));
    style->setHeight(Length(magnifierHeight, Fixed));
}

bool RenderThemeChromiumSkia::paintSearchFieldResultsButton(RenderObject* magnifierObject, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    // Get the renderer of <input> element.
    Node* input = magnifierObject->node()->shadowAncestorNode();
    if (!input->renderer()->isBox())
        return false;
    RenderBox* inputRenderBox = toRenderBox(input->renderer());
    IntRect inputContentBox = inputRenderBox->contentBoxRect();

    // Make sure the scaled decoration will fit in its parent's box.
    int magnifierHeight = std::min(inputContentBox.height(), r.height());
    int magnifierWidth = std::min(inputContentBox.width(), static_cast<int>(magnifierHeight * defaultSearchFieldResultsButtonWidth / defaultSearchFieldResultsDecorationSize));
    IntRect magnifierRect(magnifierObject->offsetFromAncestorContainer(inputRenderBox).width(),
                          inputContentBox.y() + (inputContentBox.height() - magnifierHeight + 1) / 2,
                          magnifierWidth, magnifierHeight);
    IntRect paintingRect = convertToPaintingRect(inputRenderBox, magnifierObject, magnifierRect, r);

    static Image* magnifierImage = Image::loadPlatformResource("searchMagnifierResults").releaseRef();
    paintInfo.context->drawImage(magnifierImage, magnifierObject->style()->colorSpace(), paintingRect);
    return false;
}

bool RenderThemeChromiumSkia::paintMediaControlsBackground(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    return RenderMediaControlsChromium::paintMediaControlsPart(MediaTimelineContainer, object, paintInfo, rect);
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

bool RenderThemeChromiumSkia::paintMediaSliderTrack(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    return RenderMediaControlsChromium::paintMediaControlsPart(MediaSlider, object, paintInfo, rect);
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

bool RenderThemeChromiumSkia::paintMediaVolumeSliderTrack(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    return RenderMediaControlsChromium::paintMediaControlsPart(MediaVolumeSlider, object, paintInfo, rect);
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

void RenderThemeChromiumSkia::adjustSliderThumbSize(RenderObject* object) const
{
#if ENABLE(VIDEO)
    RenderMediaControlsChromium::adjustMediaSliderThumbSize(object);
#else
    UNUSED_PARAM(object);
#endif
}

bool RenderThemeChromiumSkia::paintMediaSliderThumb(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    return RenderMediaControlsChromium::paintMediaControlsPart(MediaSliderThumb, object, paintInfo, rect);
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

bool RenderThemeChromiumSkia::paintMediaVolumeSliderThumb(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    return RenderMediaControlsChromium::paintMediaControlsPart(MediaVolumeSliderThumb, object, paintInfo, rect);
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

bool RenderThemeChromiumSkia::paintMediaPlayButton(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    return RenderMediaControlsChromium::paintMediaControlsPart(MediaPlayButton, object, paintInfo, rect);
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

bool RenderThemeChromiumSkia::paintMediaMuteButton(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    return RenderMediaControlsChromium::paintMediaControlsPart(MediaMuteButton, object, paintInfo, rect);
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

void RenderThemeChromiumSkia::adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const
{
    // Height is locked to auto on all browsers.
    style->setLineHeight(RenderStyle::initialLineHeight());
}

bool RenderThemeChromiumSkia::paintMenuList(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    SkCanvas* const canvas = i.context->platformContext()->canvas();
    const int right = rect.x() + rect.width();
    const int middle = rect.y() + rect.height() / 2;

    paintButtonLike(this, o, i, rect);

    SkPaint paint;
    paint.setColor(SK_ColorBLACK);
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);

    int arrowXPosition = (o->style()->direction() == RTL) ? rect.x() + 7 : right - 13;
    SkPath path;
    path.moveTo(arrowXPosition, middle - 3);
    path.rLineTo(6, 0);
    path.rLineTo(-3, 6);
    path.close();
    canvas->drawPath(path, paint);

    return false;
}

void RenderThemeChromiumSkia::adjustMenuListButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    adjustMenuListStyle(selector, style, e);
}

// Used to paint styled menulists (i.e. with a non-default border)
bool RenderThemeChromiumSkia::paintMenuListButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMenuList(o, i, rect);
}

bool RenderThemeChromiumSkia::paintSliderTrack(RenderObject*, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    // Just paint a grey box for now (matches the color of a scrollbar background.
    SkCanvas* const canvas = i.context->platformContext()->canvas();
    int verticalCenter = rect.y() + rect.height() / 2;
    int top = std::max(rect.y(), verticalCenter - 2);
    int bottom = std::min(rect.y() + rect.height(), verticalCenter + 2);

    SkPaint paint;
    const SkColor grey = SkColorSetARGB(0xff, 0xe3, 0xdd, 0xd8);
    paint.setColor(grey);

    SkRect skrect;
    skrect.set(rect.x(), top, rect.x() + rect.width(), bottom);
    canvas->drawRect(skrect, paint);

    return false;
}

bool RenderThemeChromiumSkia::paintSliderThumb(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    // Make a thumb similar to the scrollbar thumb.
    const bool hovered = isHovered(o) || toRenderSlider(o->parent())->inDragMode();
    const int midx = rect.x() + rect.width() / 2;
    const int midy = rect.y() + rect.height() / 2;
    const bool vertical = (o->style()->appearance() == SliderThumbVerticalPart);
    SkCanvas* const canvas = i.context->platformContext()->canvas();

    const SkColor thumbLightGrey = SkColorSetARGB(0xff, 0xf4, 0xf2, 0xef);
    const SkColor thumbDarkGrey = SkColorSetARGB(0xff, 0xea, 0xe5, 0xe0);
    SkPaint paint;
    paint.setColor(hovered ? SK_ColorWHITE : thumbLightGrey);

    SkIRect skrect;
    if (vertical)
        skrect.set(rect.x(), rect.y(), midx + 1, rect.bottom());
    else
        skrect.set(rect.x(), rect.y(), rect.right(), midy + 1);

    canvas->drawIRect(skrect, paint);

    paint.setColor(hovered ? thumbLightGrey : thumbDarkGrey);

    if (vertical)
        skrect.set(midx + 1, rect.y(), rect.right(), rect.bottom());
    else
        skrect.set(rect.x(), midy + 1, rect.right(), rect.bottom());

    canvas->drawIRect(skrect, paint);

    const SkColor borderDarkGrey = SkColorSetARGB(0xff, 0x9d, 0x96, 0x8e);
    paint.setColor(borderDarkGrey);
    drawBox(canvas, rect, paint);

    if (rect.height() > 10 && rect.width() > 10) {
        drawHorizLine(canvas, midx - 2, midx + 2, midy, paint);
        drawHorizLine(canvas, midx - 2, midx + 2, midy - 3, paint);
        drawHorizLine(canvas, midx - 2, midx + 2, midy + 3, paint);
    }

    return false;
}

int RenderThemeChromiumSkia::popupInternalPaddingLeft(RenderStyle* style) const
{
    return menuListInternalPadding(style, LeftPadding);
}

int RenderThemeChromiumSkia::popupInternalPaddingRight(RenderStyle* style) const
{
    return menuListInternalPadding(style, RightPadding);
}

int RenderThemeChromiumSkia::popupInternalPaddingTop(RenderStyle* style) const
{
    return menuListInternalPadding(style, TopPadding);
}

int RenderThemeChromiumSkia::popupInternalPaddingBottom(RenderStyle* style) const
{
    return menuListInternalPadding(style, BottomPadding);
}

#if ENABLE(VIDEO)
bool RenderThemeChromiumSkia::shouldRenderMediaControlPart(ControlPart part, Element* e)
{
    return RenderMediaControlsChromium::shouldRenderMediaControlPart(part, e);
}
#endif

// static
void RenderThemeChromiumSkia::setDefaultFontSize(int fontSize)
{
    defaultFontSize = static_cast<float>(fontSize);
}

double RenderThemeChromiumSkia::caretBlinkIntervalInternal() const
{
    return RenderTheme::caretBlinkInterval();
}

int RenderThemeChromiumSkia::menuListInternalPadding(RenderStyle* style, int paddingType) const
{
    // This internal padding is in addition to the user-supplied padding.
    // Matches the FF behavior.
    int padding = styledMenuListInternalPadding[paddingType];

    // Reserve the space for right arrow here. The rest of the padding is
    // set by adjustMenuListStyle, since PopMenuWin.cpp uses the padding from
    // RenderMenuList to lay out the individual items in the popup.
    // If the MenuList actually has appearance "NoAppearance", then that means
    // we don't draw a button, so don't reserve space for it.
    const int barType = style->direction() == LTR ? RightPadding : LeftPadding;
    if (paddingType == barType && style->appearance() != NoControlPart)
        padding += ScrollbarTheme::nativeTheme()->scrollbarThickness();

    return padding;
}

} // namespace WebCore
