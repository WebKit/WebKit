/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2008, 2009 Google, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "RenderThemeChromiumWin.h"

#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>

#include "ChromiumBridge.h"
#include "CSSStyleSheet.h"
#include "CSSValueKeywords.h"
#include "FontSelector.h"
#include "FontUtilsChromiumWin.h"
#include "GraphicsContext.h"
#include "RenderBox.h"
#include "RenderSlider.h"
#include "ScrollbarTheme.h"
#include "SkiaUtils.h"
#include "TransparencyWin.h"
#include "UserAgentStyleSheets.h"
#include "WindowsVersion.h"

// FIXME: This dependency should eventually be removed.
#include <skia/ext/skia_utils_win.h>

#define SIZEOF_STRUCT_WITH_SPECIFIED_LAST_MEMBER(structName, member) \
    offsetof(structName, member) + \
    (sizeof static_cast<structName*>(0)->member)
#define NONCLIENTMETRICS_SIZE_PRE_VISTA \
    SIZEOF_STRUCT_WITH_SPECIFIED_LAST_MEMBER(NONCLIENTMETRICS, lfMessageFont)

namespace WebCore {

namespace {

// These values all match Safari/Win.
static const float defaultControlFontPixelSize = 13;
static const float defaultCancelButtonSize = 9;
static const float minCancelButtonSize = 5;
static const float maxCancelButtonSize = 21;
static const float defaultSearchFieldResultsDecorationSize = 13;
static const float minSearchFieldResultsDecorationSize = 9;
static const float maxSearchFieldResultsDecorationSize = 30;
static const float defaultSearchFieldResultsButtonWidth = 18;

bool canvasHasMultipleLayers(const SkCanvas* canvas)
{
    SkCanvas::LayerIter iter(const_cast<SkCanvas*>(canvas), false);
    iter.next();  // There is always at least one layer.
    return !iter.done();  // There is > 1 layer if the the iterator can stil advance.
}

class ThemePainter : public TransparencyWin {
public:
    ThemePainter(GraphicsContext* context, const IntRect& r)
    {
        TransformMode transformMode = getTransformMode(context->getCTM());
        init(context, getLayerMode(context, transformMode), transformMode, r);
    }

    ~ThemePainter()
    {
        composite();
    }

private:
    static LayerMode getLayerMode(GraphicsContext* context, TransformMode transformMode)
    {
        if (context->platformContext()->isDrawingToImageBuffer())  // Might have transparent background.
            return WhiteLayer;
        else if (canvasHasMultipleLayers(context->platformContext()->canvas()))  // Needs antialiasing help.
            return OpaqueCompositeLayer;
        else  // Nothing interesting.
            return transformMode == KeepTransform ? NoLayer : OpaqueCompositeLayer;
    }

    static TransformMode getTransformMode(const TransformationMatrix& matrix)
    {
        if (matrix.b() != 0 || matrix.c() != 0)  // Skew.
            return Untransform;
        else if (matrix.a() != 1.0 || matrix.d() != 1.0)  // Scale.
            return ScaleTransform;
        else  // Nothing interesting.
            return KeepTransform;
    }
};

}  // namespace

static void getNonClientMetrics(NONCLIENTMETRICS* metrics) {
    static UINT size = WebCore::isVistaOrNewer() ?
        sizeof(NONCLIENTMETRICS) : NONCLIENTMETRICS_SIZE_PRE_VISTA;
    metrics->cbSize = size;
    bool success = !!SystemParametersInfo(SPI_GETNONCLIENTMETRICS, size, metrics, 0);
    ASSERT(success);
}

enum PaddingType {
    TopPadding,
    RightPadding,
    BottomPadding,
    LeftPadding
};

static const int styledMenuListInternalPadding[4] = { 1, 4, 1, 4 };

// The default variable-width font size.  We use this as the default font
// size for the "system font", and as a base size (which we then shrink) for
// form control fonts.
static float defaultFontSize = 16.0;

static FontDescription smallSystemFont;
static FontDescription menuFont;
static FontDescription labelFont;

// Internal static helper functions.  We don't put them in an anonymous
// namespace so they have easier access to the WebCore namespace.

static bool supportsFocus(ControlPart appearance)
{
    switch (appearance) {
    case PushButtonPart:
    case ButtonPart:
    case DefaultButtonPart:
    case SearchFieldPart:
    case TextFieldPart:
    case TextAreaPart:
        return true;
    }
    return false;
}

static void setFixedPadding(RenderStyle* style, const int padding[4])
{
    style->setPaddingLeft(Length(padding[LeftPadding], Fixed));
    style->setPaddingRight(Length(padding[RightPadding], Fixed));
    style->setPaddingTop(Length(padding[TopPadding], Fixed));
    style->setPaddingBottom(Length(padding[BottomPadding], Fixed));
}

// Return the height of system font |font| in pixels.  We use this size by
// default for some non-form-control elements.
static float systemFontSize(const LOGFONT& font)
{
    float size = -font.lfHeight;
    if (size < 0) {
        HFONT hFont = CreateFontIndirect(&font);
        if (hFont) {
            HDC hdc = GetDC(0);  // What about printing?  Is this the right DC?
            if (hdc) {
                HGDIOBJ hObject = SelectObject(hdc, hFont);
                TEXTMETRIC tm;
                GetTextMetrics(hdc, &tm);
                SelectObject(hdc, hObject);
                ReleaseDC(0, hdc);
                size = tm.tmAscent;
            }
            DeleteObject(hFont);
        }
    }

    // The "codepage 936" bit here is from Gecko; apparently this helps make
    // fonts more legible in Simplified Chinese where the default font size is
    // too small.
    //
    // FIXME: http://b/1119883 Since this is only used for "small caption",
    // "menu", and "status bar" objects, I'm not sure how much this even
    // matters.  Plus the Gecko patch went in back in 2002, and maybe this
    // isn't even relevant anymore.  We should investigate whether this should
    // be removed, or perhaps broadened to be "any CJK locale".
    //
    return ((size < 12.0f) && (GetACP() == 936)) ? 12.0f : size;
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
static wchar_t* defaultGUIFont()
{
    return L"Arial";
}

// Converts |points| to pixels.  One point is 1/72 of an inch.
static float pointsToPixels(float points)
{
    static float pixelsPerInch = 0.0f;
    if (!pixelsPerInch) {
        HDC hdc = GetDC(0);  // What about printing?  Is this the right DC?
        if (hdc) {  // Can this ever actually be NULL?
            pixelsPerInch = GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(0, hdc);
        } else {
            pixelsPerInch = 96.0f;
        }
    }

    static const float pointsPerInch = 72.0f;
    return points / pointsPerInch * pixelsPerInch;
}

static void setSizeIfAuto(RenderStyle* style, const IntSize& size)
{
    if (style->width().isIntrinsicOrAuto())
        style->setWidth(Length(size.width(), Fixed));
    if (style->height().isAuto())
        style->setHeight(Length(size.height(), Fixed));
}

static double querySystemBlinkInterval(double defaultInterval)
{
    UINT blinkTime = GetCaretBlinkTime();
    if (blinkTime == 0)
        return defaultInterval;
    if (blinkTime == INFINITE)
        return 0;
    return blinkTime / 1000.0;
}

// Implement WebCore::theme() for getting the global RenderTheme.
RenderTheme* theme()
{
    static RenderThemeChromiumWin winTheme;
    return &winTheme;
}

String RenderThemeChromiumWin::extraDefaultStyleSheet()
{
    return String(themeWinUserAgentStyleSheet, sizeof(themeWinUserAgentStyleSheet));
}

String RenderThemeChromiumWin::extraQuirksStyleSheet()
{
    return String(themeWinQuirksUserAgentStyleSheet, sizeof(themeWinQuirksUserAgentStyleSheet));
}

#if ENABLE(VIDEO)
String RenderThemeChromiumWin::extraMediaControlsStyleSheet()
{
    return String(mediaControlsUserAgentStyleSheet, sizeof(mediaControlsUserAgentStyleSheet));
}
#endif

bool RenderThemeChromiumWin::supportsFocusRing(const RenderStyle* style) const
{
   // Let webkit draw one of its halo rings around any focused element,
   // except push buttons. For buttons we use the windows PBS_DEFAULTED
   // styling to give it a blue border.
   return style->appearance() == ButtonPart
       || style->appearance() == PushButtonPart;
}

Color RenderThemeChromiumWin::platformActiveSelectionBackgroundColor() const
{
    if (ChromiumBridge::layoutTestMode())
        return Color("#0000FF");  // Royal blue.
    COLORREF color = GetSysColor(COLOR_HIGHLIGHT);
    return Color(GetRValue(color), GetGValue(color), GetBValue(color), 255);
}

Color RenderThemeChromiumWin::platformInactiveSelectionBackgroundColor() const
{
    if (ChromiumBridge::layoutTestMode())
        return Color("#999999");  // Medium gray.
    COLORREF color = GetSysColor(COLOR_GRAYTEXT);
    return Color(GetRValue(color), GetGValue(color), GetBValue(color), 255);
}

Color RenderThemeChromiumWin::platformActiveSelectionForegroundColor() const
{
    if (ChromiumBridge::layoutTestMode())
        return Color("#FFFFCC");  // Pale yellow.
    COLORREF color = GetSysColor(COLOR_HIGHLIGHTTEXT);
    return Color(GetRValue(color), GetGValue(color), GetBValue(color), 255);
}

Color RenderThemeChromiumWin::platformInactiveSelectionForegroundColor() const
{
    return Color::white;
}

Color RenderThemeChromiumWin::platformActiveTextSearchHighlightColor() const
{
    return Color(255, 150, 50);  // Orange.
}

Color RenderThemeChromiumWin::platformInactiveTextSearchHighlightColor() const
{
    return Color(255, 255, 150); // Yellow.
}

double RenderThemeChromiumWin::caretBlinkInterval() const
{
    // Disable the blinking caret in layout test mode, as it introduces
    // a race condition for the pixel tests. http://b/1198440
    if (ChromiumBridge::layoutTestMode())
        return 0;

    // This involves a system call, so we cache the result.
    static double blinkInterval = querySystemBlinkInterval(RenderTheme::caretBlinkInterval());
    return blinkInterval;
}

void RenderThemeChromiumWin::systemFont(int propId, FontDescription& fontDescription) const
{
    // This logic owes much to RenderThemeSafari.cpp.
    FontDescription* cachedDesc = NULL;
    wchar_t* faceName = 0;
    float fontSize = 0;
    switch (propId) {
    case CSSValueSmallCaption:
        cachedDesc = &smallSystemFont;
        if (!smallSystemFont.isAbsoluteSize()) {
            NONCLIENTMETRICS metrics;
            getNonClientMetrics(&metrics);
            faceName = metrics.lfSmCaptionFont.lfFaceName;
            fontSize = systemFontSize(metrics.lfSmCaptionFont);
        }
        break;
    case CSSValueMenu:
        cachedDesc = &menuFont;
        if (!menuFont.isAbsoluteSize()) {
            NONCLIENTMETRICS metrics;
            getNonClientMetrics(&metrics);
            faceName = metrics.lfMenuFont.lfFaceName;
            fontSize = systemFontSize(metrics.lfMenuFont);
        }
        break;
    case CSSValueStatusBar:
        cachedDesc = &labelFont;
        if (!labelFont.isAbsoluteSize()) {
            NONCLIENTMETRICS metrics;
            getNonClientMetrics(&metrics);
            faceName = metrics.lfStatusFont.lfFaceName;
            fontSize = systemFontSize(metrics.lfStatusFont);
        }
        break;
    case CSSValueWebkitMiniControl:
    case CSSValueWebkitSmallControl:
    case CSSValueWebkitControl:
        faceName = defaultGUIFont();
        // Why 2 points smaller?  Because that's what Gecko does.
        fontSize = defaultFontSize - pointsToPixels(2);
        break;
    default:
        faceName = defaultGUIFont();
        fontSize = defaultFontSize;
        break;
    }

    if (!cachedDesc)
        cachedDesc = &fontDescription;

    if (fontSize) {
        ASSERT(faceName);
        cachedDesc->firstFamily().setFamily(AtomicString(faceName,
                                                         wcslen(faceName)));
        cachedDesc->setIsAbsoluteSize(true);
        cachedDesc->setGenericFamily(FontDescription::NoFamily);
        cachedDesc->setSpecifiedSize(fontSize);
        cachedDesc->setWeight(FontWeightNormal);
        cachedDesc->setItalic(false);
    }
    fontDescription = *cachedDesc;
}

int RenderThemeChromiumWin::minimumMenuListSize(RenderStyle* style) const
{
    return 0;
}

void RenderThemeChromiumWin::adjustSliderThumbSize(RenderObject* o) const
{
    // These sizes match what WinXP draws for various menus.
    const int sliderThumbAlongAxis = 11;
    const int sliderThumbAcrossAxis = 21;
    if (o->style()->appearance() == SliderThumbHorizontalPart || o->style()->appearance() == MediaSliderThumbPart) {
        o->style()->setWidth(Length(sliderThumbAlongAxis, Fixed));
        o->style()->setHeight(Length(sliderThumbAcrossAxis, Fixed));
    } else if (o->style()->appearance() == SliderThumbVerticalPart) {
        o->style()->setWidth(Length(sliderThumbAcrossAxis, Fixed));
        o->style()->setHeight(Length(sliderThumbAlongAxis, Fixed));
    }
}

void RenderThemeChromiumWin::setCheckboxSize(RenderStyle* style) const
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

void RenderThemeChromiumWin::setRadioSize(RenderStyle* style) const
{
    // Use same sizing for radio box as checkbox.
    setCheckboxSize(style);
}

bool RenderThemeChromiumWin::paintButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    const ThemeData& themeData = getThemeData(o);

    WebCore::ThemePainter painter(i.context, r);
    ChromiumBridge::paintButton(painter.context(),
                                themeData.m_part,
                                themeData.m_state,
                                themeData.m_classicState,
                                painter.drawRect());
    return false;
}

bool RenderThemeChromiumWin::paintTextField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintTextFieldInternal(o, i, r, true);
}

bool RenderThemeChromiumWin::paintSliderTrack(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    const ThemeData& themeData = getThemeData(o);

    WebCore::ThemePainter painter(i.context, r);
    ChromiumBridge::paintTrackbar(painter.context(),
                                  themeData.m_part,
                                  themeData.m_state,
                                  themeData.m_classicState,
                                  painter.drawRect());
    return false;
}

void RenderThemeChromiumWin::adjustSearchFieldCancelButtonStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    // Scale the button size based on the font size
    float fontScale = style->fontSize() / defaultControlFontPixelSize;
    int cancelButtonSize = lroundf(std::min(std::max(minCancelButtonSize, defaultCancelButtonSize * fontScale), maxCancelButtonSize));
    style->setWidth(Length(cancelButtonSize, Fixed));
    style->setHeight(Length(cancelButtonSize, Fixed));
}

bool RenderThemeChromiumWin::paintSearchFieldCancelButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
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

void RenderThemeChromiumWin::adjustSearchFieldDecorationStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    IntSize emptySize(1, 11);
    style->setWidth(Length(emptySize.width(), Fixed));
    style->setHeight(Length(emptySize.height(), Fixed));
}

void RenderThemeChromiumWin::adjustSearchFieldResultsDecorationStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    // Scale the decoration size based on the font size
    float fontScale = style->fontSize() / defaultControlFontPixelSize;
    int magnifierSize = lroundf(std::min(std::max(minSearchFieldResultsDecorationSize, defaultSearchFieldResultsDecorationSize * fontScale), 
                                         maxSearchFieldResultsDecorationSize));
    style->setWidth(Length(magnifierSize, Fixed));
    style->setHeight(Length(magnifierSize, Fixed));
}

bool RenderThemeChromiumWin::paintSearchFieldResultsDecoration(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
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

void RenderThemeChromiumWin::adjustSearchFieldResultsButtonStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    // Scale the button size based on the font size
    float fontScale = style->fontSize() / defaultControlFontPixelSize;
    int magnifierHeight = lroundf(std::min(std::max(minSearchFieldResultsDecorationSize, defaultSearchFieldResultsDecorationSize * fontScale), 
                                           maxSearchFieldResultsDecorationSize));
    int magnifierWidth = lroundf(magnifierHeight * defaultSearchFieldResultsButtonWidth / defaultSearchFieldResultsDecorationSize);
    style->setWidth(Length(magnifierWidth, Fixed));
    style->setHeight(Length(magnifierHeight, Fixed));
}

bool RenderThemeChromiumWin::paintSearchFieldResultsButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
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

void RenderThemeChromiumWin::adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    // Height is locked to auto on all browsers.
    style->setLineHeight(RenderStyle::initialLineHeight());
}

// Used to paint unstyled menulists (i.e. with the default border)
bool RenderThemeChromiumWin::paintMenuList(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    if (!o->isBox())
        return false;

    const RenderBox* box = toRenderBox(o);
    int borderRight = box->borderRight();
    int borderLeft = box->borderLeft();
    int borderTop = box->borderTop();
    int borderBottom = box->borderBottom();

    // If all the borders are 0, then tell skia not to paint the border on the
    // textfield.  FIXME: http://b/1210017 Figure out how to get Windows to not
    // draw individual borders and then pass that to skia so we can avoid
    // drawing any borders that are set to 0. For non-zero borders, we draw the
    // border, but webkit just draws over it.
    bool drawEdges = !(borderRight == 0 && borderLeft == 0 && borderTop == 0 && borderBottom == 0);

    paintTextFieldInternal(o, i, r, drawEdges);

    // Take padding and border into account.  If the MenuList is smaller than
    // the size of a button, make sure to shrink it appropriately and not put
    // its x position to the left of the menulist.
    const int buttonWidth = GetSystemMetrics(SM_CXVSCROLL);
    int spacingLeft = borderLeft + box->paddingLeft();
    int spacingRight = borderRight + box->paddingRight();
    int spacingTop = borderTop + box->paddingTop();
    int spacingBottom = borderBottom + box->paddingBottom();

    int buttonX;
    if (r.right() - r.x() < buttonWidth)
        buttonX = r.x();
    else
        buttonX = o->style()->direction() == LTR ? r.right() - spacingRight - buttonWidth : r.x() + spacingLeft;

    // Compute the rectangle of the button in the destination image.
    IntRect rect(buttonX,
                 r.y() + spacingTop,
                 std::min(buttonWidth, r.right() - r.x()),
                 r.height() - (spacingTop + spacingBottom));

    // Get the correct theme data for a textfield and paint the menu.
    WebCore::ThemePainter painter(i.context, rect);
    ChromiumBridge::paintMenuList(painter.context(),
                                  CP_DROPDOWNBUTTON,
                                  determineState(o),
                                  determineClassicState(o),
                                  painter.drawRect());
    return false;
}

void RenderThemeChromiumWin::adjustMenuListButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    adjustMenuListStyle(selector, style, e);
}

// Used to paint styled menulists (i.e. with a non-default border)
bool RenderThemeChromiumWin::paintMenuListButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintMenuList(o, i, r);
}

int RenderThemeChromiumWin::popupInternalPaddingLeft(RenderStyle* style) const
{
    return menuListInternalPadding(style, LeftPadding);
}

int RenderThemeChromiumWin::popupInternalPaddingRight(RenderStyle* style) const
{
    return menuListInternalPadding(style, RightPadding);
}

int RenderThemeChromiumWin::popupInternalPaddingTop(RenderStyle* style) const
{
    return menuListInternalPadding(style, TopPadding);
}

int RenderThemeChromiumWin::popupInternalPaddingBottom(RenderStyle* style) const
{
    return menuListInternalPadding(style, BottomPadding);
}

int RenderThemeChromiumWin::buttonInternalPaddingLeft() const
{
    return 3;
}

int RenderThemeChromiumWin::buttonInternalPaddingRight() const
{
    return 3;
}

int RenderThemeChromiumWin::buttonInternalPaddingTop() const
{
    return 1;
}

int RenderThemeChromiumWin::buttonInternalPaddingBottom() const
{
    return 1;
}

// static
void RenderThemeChromiumWin::setDefaultFontSize(int fontSize) {
    defaultFontSize = static_cast<float>(fontSize);

    // Reset cached fonts.
    smallSystemFont = menuFont = labelFont = FontDescription();
}

unsigned RenderThemeChromiumWin::determineState(RenderObject* o)
{
    unsigned result = TS_NORMAL;
    ControlPart appearance = o->style()->appearance();
    if (!isEnabled(o))
        result = TS_DISABLED;
    else if (isReadOnlyControl(o) && (TextFieldPart == appearance || TextAreaPart == appearance || SearchFieldPart == appearance))
        result = ETS_READONLY; // Readonly is supported on textfields.
    else if (isPressed(o)) // Active overrides hover and focused.
        result = TS_PRESSED;
    else if (supportsFocus(appearance) && isFocused(o))
        result = ETS_FOCUSED;
    else if (isHovered(o))
        result = TS_HOT;
    if (isChecked(o))
        result += 4; // 4 unchecked states, 4 checked states.
    return result;
}

unsigned RenderThemeChromiumWin::determineSliderThumbState(RenderObject* o)
{
    unsigned result = TUS_NORMAL;
    if (!isEnabled(o->parent()))
        result = TUS_DISABLED;
    else if (supportsFocus(o->style()->appearance()) && isFocused(o->parent()))
        result = TUS_FOCUSED;
    else if (static_cast<RenderSlider*>(o->parent())->inDragMode())
        result = TUS_PRESSED;
    else if (isHovered(o))
        result = TUS_HOT;
    return result;
}

unsigned RenderThemeChromiumWin::determineClassicState(RenderObject* o)
{
    unsigned result = 0;
    if (!isEnabled(o))
        result = DFCS_INACTIVE;
    else if (isPressed(o)) // Active supersedes hover
        result = DFCS_PUSHED;
    else if (isHovered(o))
        result = DFCS_HOT;
    if (isChecked(o))
        result |= DFCS_CHECKED;
    return result;
}

ThemeData RenderThemeChromiumWin::getThemeData(RenderObject* o)
{
    ThemeData result;
    switch (o->style()->appearance()) {
    case CheckboxPart:
        result.m_part = BP_CHECKBOX;
        result.m_state = determineState(o);
        result.m_classicState = DFCS_BUTTONCHECK;
        break;
    case RadioPart:
        result.m_part = BP_RADIOBUTTON;
        result.m_state = determineState(o);
        result.m_classicState = DFCS_BUTTONRADIO;
        break;
    case PushButtonPart:
    case ButtonPart:
        result.m_part = BP_PUSHBUTTON;
        result.m_state = determineState(o);
        result.m_classicState = DFCS_BUTTONPUSH;
        break;
    case SliderHorizontalPart:
        result.m_part = TKP_TRACK;
        result.m_state = TRS_NORMAL;
        break;
    case SliderVerticalPart:
        result.m_part = TKP_TRACKVERT;
        result.m_state = TRVS_NORMAL;
        break;
    case SliderThumbHorizontalPart:
        result.m_part = TKP_THUMBBOTTOM;
        result.m_state = determineSliderThumbState(o);
        break;
    case SliderThumbVerticalPart:
        result.m_part = TKP_THUMBVERT;
        result.m_state = determineSliderThumbState(o);
        break;
    case ListboxPart:
    case MenulistPart:
    case SearchFieldPart:
    case TextFieldPart:
    case TextAreaPart:
        result.m_part = EP_EDITTEXT;
        result.m_state = determineState(o);
        break;
    }

    result.m_classicState |= determineClassicState(o);

    return result;
}

bool RenderThemeChromiumWin::paintTextFieldInternal(RenderObject* o,
                                                    const RenderObject::PaintInfo& i,
                                                    const IntRect& r,
                                                    bool drawEdges)
{
    // Nasty hack to make us not paint the border on text fields with a
    // border-radius. Webkit paints elements with border-radius for us.
    // FIXME: Get rid of this if-check once we can properly clip rounded
    // borders: http://b/1112604 and http://b/1108635
    // FIXME: make sure we do the right thing if css background-clip is set.
    if (o->style()->hasBorderRadius())
        return false;

    const ThemeData& themeData = getThemeData(o);

    WebCore::ThemePainter painter(i.context, r);
    ChromiumBridge::paintTextField(painter.context(),
                                   themeData.m_part,
                                   themeData.m_state,
                                   themeData.m_classicState,
                                   painter.drawRect(),
                                   o->style()->backgroundColor(),
                                   true,
                                   drawEdges);
    return false;
}

int RenderThemeChromiumWin::menuListInternalPadding(RenderStyle* style, int paddingType) const
{
    // This internal padding is in addition to the user-supplied padding.
    // Matches the FF behavior.
    int padding = styledMenuListInternalPadding[paddingType];

    // Reserve the space for right arrow here. The rest of the padding is set
    // by adjustMenuListStyle, since PopupMenuChromium.cpp uses the padding
    // from RenderMenuList to lay out the individual items in the popup.  If
    // the MenuList actually has appearance "NoAppearance", then that means we
    // don't draw a button, so don't reserve space for it.
    const int barType = style->direction() == LTR ? RightPadding : LeftPadding;
    if (paddingType == barType && style->appearance() != NoControlPart)
        padding += ScrollbarTheme::nativeTheme()->scrollbarThickness();

    return padding;
}

} // namespace WebCore
