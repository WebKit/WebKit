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

#if ENABLE(VIDEO)
// Attempt to retrieve a HTMLMediaElement from a Node. Returns NULL if one cannot be found.
static HTMLMediaElement* mediaElementParent(Node* node)
{
    if (!node)
        return 0;
    Node* mediaNode = node->shadowAncestorNode();
    if (!mediaNode || (!mediaNode->hasTagName(HTMLNames::videoTag) && !mediaNode->hasTagName(HTMLNames::audioTag)))
        return 0;

    return static_cast<HTMLMediaElement*>(mediaNode);
}
#endif

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

    i.context->drawImage(image, rect);
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
    const IntSize size(13, 13);
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

    i.context->drawImage(image, rect);
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
    if (o->style()->hasBackground())
        baseColor = o->style()->backgroundColor().rgb();
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

bool RenderThemeChromiumSkia::paintTextField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return true;
}

bool RenderThemeChromiumSkia::paintTextArea(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
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

bool RenderThemeChromiumSkia::paintSearchFieldCancelButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    IntRect bounds = r;
    ASSERT(o->parent());
    if (!o->parent() || !o->parent()->isBox())
        return false;

    RenderBox* parentRenderBox = toRenderBox(o->parent());

    IntRect parentBox = parentRenderBox->absoluteContentBox();

    // Make sure the scaled button stays square and will fit in its parent's box
    bounds.setHeight(std::min(parentBox.width(), std::min(parentBox.height(), bounds.height())));
    bounds.setWidth(bounds.height());

    // Center the button vertically.  Round up though, so if it has to be one pixel off-center, it will
    // be one pixel closer to the bottom of the field.  This tends to look better with the text.
    bounds.setY(parentBox.y() + (parentBox.height() - bounds.height() + 1) / 2);

    static Image* cancelImage = Image::loadPlatformResource("searchCancel").releaseRef();
    static Image* cancelPressedImage = Image::loadPlatformResource("searchCancelPressed").releaseRef();
    i.context->drawImage(isPressed(o) ? cancelPressedImage : cancelImage, bounds);
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

bool RenderThemeChromiumSkia::paintSearchFieldResultsDecoration(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    IntRect bounds = r;
    ASSERT(o->parent());
    if (!o->parent() || !o->parent()->isBox())
        return false;

    RenderBox* parentRenderBox = toRenderBox(o->parent());
    IntRect parentBox = parentRenderBox->absoluteContentBox();

    // Make sure the scaled decoration stays square and will fit in its parent's box
    bounds.setHeight(std::min(parentBox.width(), std::min(parentBox.height(), bounds.height())));
    bounds.setWidth(bounds.height());

    // Center the decoration vertically.  Round up though, so if it has to be one pixel off-center, it will
    // be one pixel closer to the bottom of the field.  This tends to look better with the text.
    bounds.setY(parentBox.y() + (parentBox.height() - bounds.height() + 1) / 2);

    static Image* magnifierImage = Image::loadPlatformResource("searchMagnifier").releaseRef();
    i.context->drawImage(magnifierImage, bounds);
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

bool RenderThemeChromiumSkia::paintSearchFieldResultsButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    IntRect bounds = r;
    ASSERT(o->parent());
    if (!o->parent())
        return false;
    if (!o->parent() || !o->parent()->isBox())
        return false;

    RenderBox* parentRenderBox = toRenderBox(o->parent());
    IntRect parentBox = parentRenderBox->absoluteContentBox();

    // Make sure the scaled decoration will fit in its parent's box
    bounds.setHeight(std::min(parentBox.height(), bounds.height()));
    bounds.setWidth(std::min(parentBox.width(), static_cast<int>(bounds.height() * defaultSearchFieldResultsButtonWidth / defaultSearchFieldResultsDecorationSize)));

    // Center the button vertically.  Round up though, so if it has to be one pixel off-center, it will
    // be one pixel closer to the bottom of the field.  This tends to look better with the text.
    bounds.setY(parentBox.y() + (parentBox.height() - bounds.height() + 1) / 2);

    static Image* magnifierImage = Image::loadPlatformResource("searchMagnifierResults").releaseRef();
    i.context->drawImage(magnifierImage, bounds);
    return false;
}

bool RenderThemeChromiumSkia::paintMediaButtonInternal(GraphicsContext* context, const IntRect& rect, Image* image)
{
    // Create a destination rectangle for the image that is centered in the drawing rectangle, rounded left, and down.
    IntRect imageRect = image->rect();
    imageRect.setY(rect.y() + (rect.height() - image->height() + 1) / 2);
    imageRect.setX(rect.x() + (rect.width() - image->width() + 1) / 2);

    context->drawImage(image, imageRect);
    return true;
}

bool RenderThemeChromiumSkia::paintMediaControlsBackground(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    HTMLMediaElement* mediaElement = mediaElementParent(object->node());
    if (!mediaElement)
        return false;

    if (!rect.isEmpty())
    {
        SkCanvas* canvas = paintInfo.context->platformContext()->canvas();
        SkPaint paint;

        // Draws the left border, it is always 1px wide.
        paint.setColor(object->style()->borderLeftColor().rgb());
        canvas->drawLine(rect.x() + 1, rect.y(),
                         rect.x() + 1, rect.y() + rect.height(),
                         paint);

        // Draws the right border, it is always 1px wide.
        paint.setColor(object->style()->borderRightColor().rgb());
        canvas->drawLine(rect.x() + rect.width() - 1, rect.y(),
                         rect.x() + rect.width() - 1, rect.y() + rect.height(),
                         paint);
    }
    return true;
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
    HTMLMediaElement* mediaElement = mediaElementParent(object->node());
    if (!mediaElement)
        return false;

    SkCanvas* canvas = paintInfo.context->platformContext()->canvas();
    SkRect backgroundRect;
    backgroundRect.set(rect.x(), rect.y(), rect.x() + rect.width(), rect.y() + rect.height());

    SkPaint paint;
    paint.setAntiAlias(true);

    // Draw the border of the time bar. The border only has one single color,
    // width and radius. So use the property of the left border.
    SkColor borderColor = object->style()->borderLeftColor().rgb();
    int borderWidth = object->style()->borderLeftWidth();
    IntSize borderRadius = object->style()->borderTopLeftRadius();
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(borderWidth);
    paint.setColor(borderColor);
    canvas->drawRoundRect(backgroundRect, borderRadius.width(), borderRadius.height(), paint);

    // Draw the background of the time bar.
    SkColor backgroundColor = object->style()->backgroundColor().rgb();
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(backgroundColor);
    canvas->drawRoundRect(backgroundRect, borderRadius.width(), borderRadius.height(), paint);

    if (backgroundRect.width() >= 3 && backgroundRect.height() >= 3)
    {
        // Draw the buffered ranges.
        // FIXME: Draw multiple ranges if there are multiple buffered ranges.
        SkRect bufferedRect;
        bufferedRect.set(backgroundRect.fLeft + 2, backgroundRect.fTop + 2,
                         backgroundRect.fRight - 1, backgroundRect.fBottom - 1);
        int width = static_cast<int>(bufferedRect.width() * mediaElement->percentLoaded());
        bufferedRect.fRight = bufferedRect.fLeft + width;

        SkPoint points[2] = { { 0, bufferedRect.fTop }, { 0, bufferedRect.fBottom } };
        SkColor startColor = object->style()->color().rgb();
        SkColor endColor = SkColorSetRGB(SkColorGetR(startColor) / 2,
                                         SkColorGetG(startColor) / 2,
                                         SkColorGetB(startColor) / 2);
        SkColor colors[2] = { startColor, endColor };
        SkShader* gradient = SkGradientShader::CreateLinear(points, colors, 0,
                                                            sizeof(points) / sizeof(points[0]),
                                                            SkShader::kMirror_TileMode, 0);

        paint.reset();
        paint.setShader(gradient);
        paint.setAntiAlias(true);
        // Check for round rect with zero width or height, otherwise Skia will assert
        if (bufferedRect.width() > 0 && bufferedRect.height() > 0)
            canvas->drawRoundRect(bufferedRect, borderRadius.width(), borderRadius.height(), paint);
        gradient->unref();
    }
    return true;
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
    HTMLMediaElement* mediaElement = mediaElementParent(object->node());
    if (!mediaElement)
        return false;

    SkCanvas* canvas = paintInfo.context->platformContext()->canvas();
    SkPaint paint;
    paint.setColor(SK_ColorWHITE);

    int x = rect.x() + rect.width() / 2;
    canvas->drawLine(x, rect.y(), x, rect.y() + rect.height(), paint);
    return true;
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

static Image* mediaSliderThumbImage()
{
    static Image* mediaSliderThumb = Image::loadPlatformResource("mediaSliderThumb").releaseRef();
    return mediaSliderThumb;
}

static Image* mediaVolumeSliderThumbImage()
{
    static Image* mediaVolumeSliderThumb = Image::loadPlatformResource("mediaVolumeSliderThumb").releaseRef();
    return mediaVolumeSliderThumb;
}

void RenderThemeChromiumSkia::adjustSliderThumbSize(RenderObject* object) const
{
#if ENABLE(VIDEO)
    Image* thumbImage = 0;
    if (object->style()->appearance() == MediaSliderThumbPart)
        thumbImage = mediaSliderThumbImage();
    else if (object->style()->appearance() == MediaVolumeSliderThumbPart)
        thumbImage = mediaVolumeSliderThumbImage();

    if (thumbImage) {
        object->style()->setWidth(Length(thumbImage->width(), Fixed));
        object->style()->setHeight(Length(thumbImage->height(), Fixed));
    }
#else
    UNUSED_PARAM(object);
#endif
}

bool RenderThemeChromiumSkia::paintMediaSliderThumb(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    if (!object->parent()->isSlider())
        return false;

    return paintMediaButtonInternal(paintInfo.context, rect, mediaSliderThumbImage());
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
    if (!object->parent()->isSlider())
        return false;

    return paintMediaButtonInternal(paintInfo.context, rect, mediaVolumeSliderThumbImage());
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
    HTMLMediaElement* mediaElement = mediaElementParent(object->node());
    if (!mediaElement)
        return false;

    static Image* mediaPlay = Image::loadPlatformResource("mediaPlay").releaseRef();
    static Image* mediaPause = Image::loadPlatformResource("mediaPause").releaseRef();
    static Image* mediaPlayDisabled = Image::loadPlatformResource("mediaPlayDisabled").releaseRef();

    if (mediaElement->networkState() == HTMLMediaElement::NETWORK_NO_SOURCE)
        return paintMediaButtonInternal(paintInfo.context, rect, mediaPlayDisabled);

    return paintMediaButtonInternal(paintInfo.context, rect, mediaElement->paused() ? mediaPlay : mediaPause);
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
    HTMLMediaElement* mediaElement = mediaElementParent(object->node());
    if (!mediaElement)
        return false;

    static Image* soundFull = Image::loadPlatformResource("mediaSoundFull").releaseRef();
    static Image* soundNone = Image::loadPlatformResource("mediaSoundNone").releaseRef();
    static Image* soundDisabled = Image::loadPlatformResource("mediaSoundDisabled").releaseRef();

    if (mediaElement->networkState() == HTMLMediaElement::NETWORK_NO_SOURCE || !mediaElement->hasAudio())
        return paintMediaButtonInternal(paintInfo.context, rect, soundDisabled);

    return paintMediaButtonInternal(paintInfo.context, rect, mediaElement->muted() ? soundNone: soundFull);
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

    SkPath path;
    path.moveTo(right - 13, middle - 3);
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

int RenderThemeChromiumSkia::buttonInternalPaddingLeft() const
{
    return 3;
}

int RenderThemeChromiumSkia::buttonInternalPaddingRight() const
{
    return 3;
}

int RenderThemeChromiumSkia::buttonInternalPaddingTop() const
{
    return 1;
}

int RenderThemeChromiumSkia::buttonInternalPaddingBottom() const
{
    return 1;
}

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
