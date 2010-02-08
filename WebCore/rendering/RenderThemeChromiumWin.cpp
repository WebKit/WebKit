/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2008, 2009 Google, Inc.
 * Copyright (C) 2009 Kenneth Rohde Christiansen
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
#include "CSSValueKeywords.h"
#include "FontSelector.h"
#include "FontUtilsChromiumWin.h"
#include "GraphicsContext.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "MediaControlElements.h"
#include "RenderBox.h"
#include "RenderSlider.h"
#include "ScrollbarTheme.h"
#include "TransparencyWin.h"
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
    static bool canvasHasMultipleLayers(const SkCanvas* canvas)
    {
        SkCanvas::LayerIter iter(const_cast<SkCanvas*>(canvas), false);
        iter.next();  // There is always at least one layer.
        return !iter.done();  // There is > 1 layer if the the iterator can stil advance.
    }

    static LayerMode getLayerMode(GraphicsContext* context, TransformMode transformMode)
    {
        if (context->platformContext()->isDrawingToImageBuffer())  // Might have transparent background.
            return WhiteLayer;
        else if (canvasHasMultipleLayers(context->platformContext()->canvas()))  // Needs antialiasing help.
            return OpaqueCompositeLayer;
        else  // Nothing interesting.
            return transformMode == KeepTransform ? NoLayer : OpaqueCompositeLayer;
    }

    static TransformMode getTransformMode(const AffineTransform& matrix)
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

static void getNonClientMetrics(NONCLIENTMETRICS* metrics)
{
    static UINT size = WebCore::isVistaOrNewer() ?
        sizeof(NONCLIENTMETRICS) : NONCLIENTMETRICS_SIZE_PRE_VISTA;
    metrics->cbSize = size;
    bool success = !!SystemParametersInfo(SPI_GETNONCLIENTMETRICS, size, metrics, 0);
    ASSERT(success);
}

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

static double querySystemBlinkInterval(double defaultInterval)
{
    UINT blinkTime = GetCaretBlinkTime();
    if (blinkTime == 0)
        return defaultInterval;
    if (blinkTime == INFINITE)
        return 0;
    return blinkTime / 1000.0;
}

PassRefPtr<RenderTheme> RenderThemeChromiumWin::create()
{
    return adoptRef(new RenderThemeChromiumWin);
}

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page* page)
{
    static RenderTheme* rt = RenderThemeChromiumWin::create().releaseRef();
    return rt;
}

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
        return Color(0x00, 0x00, 0xff);  // Royal blue.
    COLORREF color = GetSysColor(COLOR_HIGHLIGHT);
    return Color(GetRValue(color), GetGValue(color), GetBValue(color), 0xff);
}

Color RenderThemeChromiumWin::platformInactiveSelectionBackgroundColor() const
{
    if (ChromiumBridge::layoutTestMode())
        return Color(0x99, 0x99, 0x99);  // Medium gray.
    COLORREF color = GetSysColor(COLOR_GRAYTEXT);
    return Color(GetRValue(color), GetGValue(color), GetBValue(color), 0xff);
}

Color RenderThemeChromiumWin::platformActiveSelectionForegroundColor() const
{
    if (ChromiumBridge::layoutTestMode())
        return Color(0xff, 0xff, 0xcc);  // Pale yellow.
    COLORREF color = GetSysColor(COLOR_HIGHLIGHTTEXT);
    return Color(GetRValue(color), GetGValue(color), GetBValue(color), 0xff);
}

Color RenderThemeChromiumWin::platformInactiveSelectionForegroundColor() const
{
    return Color::white;
}

Color RenderThemeChromiumWin::platformActiveTextSearchHighlightColor() const
{
    return Color(0xff, 0x96, 0x32);  // Orange.
}

Color RenderThemeChromiumWin::platformInactiveTextSearchHighlightColor() const
{
    return Color(0xff, 0xff, 0x96); // Yellow.
}

void RenderThemeChromiumWin::systemFont(int propId, FontDescription& fontDescription) const
{
    // This logic owes much to RenderThemeSafari.cpp.
    FontDescription* cachedDesc = 0;
    AtomicString faceName;
    float fontSize = 0;
    switch (propId) {
    case CSSValueSmallCaption:
        cachedDesc = &smallSystemFont;
        if (!smallSystemFont.isAbsoluteSize()) {
            NONCLIENTMETRICS metrics;
            getNonClientMetrics(&metrics);
            faceName = AtomicString(metrics.lfSmCaptionFont.lfFaceName, wcslen(metrics.lfSmCaptionFont.lfFaceName));
            fontSize = systemFontSize(metrics.lfSmCaptionFont);
        }
        break;
    case CSSValueMenu:
        cachedDesc = &menuFont;
        if (!menuFont.isAbsoluteSize()) {
            NONCLIENTMETRICS metrics;
            getNonClientMetrics(&metrics);
            faceName = AtomicString(metrics.lfMenuFont.lfFaceName, wcslen(metrics.lfMenuFont.lfFaceName));
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
        cachedDesc->firstFamily().setFamily(faceName);
        cachedDesc->setIsAbsoluteSize(true);
        cachedDesc->setGenericFamily(FontDescription::NoFamily);
        cachedDesc->setSpecifiedSize(fontSize);
        cachedDesc->setWeight(FontWeightNormal);
        cachedDesc->setItalic(false);
    }
    fontDescription = *cachedDesc;
}

// Map a CSSValue* system color to an index understood by GetSysColor().
static int cssValueIdToSysColorIndex(int cssValueId)
{
    switch (cssValueId) {
    case CSSValueActiveborder: return COLOR_ACTIVEBORDER;
    case CSSValueActivecaption: return COLOR_ACTIVECAPTION;
    case CSSValueAppworkspace: return COLOR_APPWORKSPACE;
    case CSSValueBackground: return COLOR_BACKGROUND;
    case CSSValueButtonface: return COLOR_BTNFACE;
    case CSSValueButtonhighlight: return COLOR_BTNHIGHLIGHT;
    case CSSValueButtonshadow: return COLOR_BTNSHADOW;
    case CSSValueButtontext: return COLOR_BTNTEXT;
    case CSSValueCaptiontext: return COLOR_CAPTIONTEXT;
    case CSSValueGraytext: return COLOR_GRAYTEXT;
    case CSSValueHighlight: return COLOR_HIGHLIGHT;
    case CSSValueHighlighttext: return COLOR_HIGHLIGHTTEXT;
    case CSSValueInactiveborder: return COLOR_INACTIVEBORDER;
    case CSSValueInactivecaption: return COLOR_INACTIVECAPTION;
    case CSSValueInactivecaptiontext: return COLOR_INACTIVECAPTIONTEXT;
    case CSSValueInfobackground: return COLOR_INFOBK;
    case CSSValueInfotext: return COLOR_INFOTEXT;
    case CSSValueMenu: return COLOR_MENU;
    case CSSValueMenutext: return COLOR_MENUTEXT;
    case CSSValueScrollbar: return COLOR_SCROLLBAR;
    case CSSValueThreeddarkshadow: return COLOR_3DDKSHADOW;
    case CSSValueThreedface: return COLOR_3DFACE;
    case CSSValueThreedhighlight: return COLOR_3DHIGHLIGHT;
    case CSSValueThreedlightshadow: return COLOR_3DLIGHT;
    case CSSValueThreedshadow: return COLOR_3DSHADOW;
    case CSSValueWindow: return COLOR_WINDOW;
    case CSSValueWindowframe: return COLOR_WINDOWFRAME;
    case CSSValueWindowtext: return COLOR_WINDOWTEXT;
    default: return -1; // Unsupported CSSValue
    }
}

Color RenderThemeChromiumWin::systemColor(int cssValueId) const
{
    int sysColorIndex = cssValueIdToSysColorIndex(cssValueId);
    if (ChromiumBridge::layoutTestMode() || (sysColorIndex == -1))
        return RenderTheme::systemColor(cssValueId);

    COLORREF color = GetSysColor(sysColorIndex);
    return Color(GetRValue(color), GetGValue(color), GetBValue(color));
}

void RenderThemeChromiumWin::adjustSliderThumbSize(RenderObject* o) const
{
    // These sizes match what WinXP draws for various menus.
    const int sliderThumbAlongAxis = 11;
    const int sliderThumbAcrossAxis = 21;
    if (o->style()->appearance() == SliderThumbHorizontalPart) {
        o->style()->setWidth(Length(sliderThumbAlongAxis, Fixed));
        o->style()->setHeight(Length(sliderThumbAcrossAxis, Fixed));
    } else if (o->style()->appearance() == SliderThumbVerticalPart) {
        o->style()->setWidth(Length(sliderThumbAcrossAxis, Fixed));
        o->style()->setHeight(Length(sliderThumbAlongAxis, Fixed));
    } else
        RenderThemeChromiumSkia::adjustSliderThumbSize(o);
}

bool RenderThemeChromiumWin::paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintButton(o, i, r);
}
bool RenderThemeChromiumWin::paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintButton(o, i, r);
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

bool RenderThemeChromiumWin::paintSliderThumb(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintSliderTrack(o, i, r);
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

// static
void RenderThemeChromiumWin::setDefaultFontSize(int fontSize)
{
    RenderThemeChromiumSkia::setDefaultFontSize(fontSize);

    // Reset cached fonts.
    smallSystemFont = menuFont = labelFont = FontDescription();
}

double RenderThemeChromiumWin::caretBlinkIntervalInternal() const
{
    // This involves a system call, so we cache the result.
    static double blinkInterval = querySystemBlinkInterval(RenderTheme::caretBlinkInterval());
    return blinkInterval;
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
    else if (toRenderSlider(o->parent())->inDragMode())
        result = TUS_PRESSED;
    else if (isHovered(o))
        result = TUS_HOT;
    return result;
}

unsigned RenderThemeChromiumWin::determineClassicState(RenderObject* o)
{
    unsigned result = 0;

    ControlPart part = o->style()->appearance();

    // Sliders are always in the normal state.
    if (part == SliderHorizontalPart || part == SliderVerticalPart)
        return result;

    // So are readonly text fields.
    if (isReadOnlyControl(o) && (part == TextFieldPart || part == TextAreaPart || part == SearchFieldPart))
        return result;   

    if (part == SliderThumbHorizontalPart || part == SliderThumbVerticalPart) {
        if (!isEnabled(o->parent()))
            result = DFCS_INACTIVE;
        else if (toRenderSlider(o->parent())->inDragMode()) // Active supersedes hover
            result = DFCS_PUSHED;
        else if (isHovered(o))
            result = DFCS_HOT;
    } else {
        if (!isEnabled(o))
            result = DFCS_INACTIVE;
        else if (isPressed(o)) // Active supersedes hover
            result = DFCS_PUSHED;
        else if (supportsFocus(part) && isFocused(o)) // So does focused
            result = 0;
        else if (isHovered(o))
            result = DFCS_HOT;
        if (isChecked(o))
            result |= DFCS_CHECKED;
    }
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
    case MenulistButtonPart:
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
    // Fallback to white if the specified color object is invalid.
    // (Note ChromiumBridge::paintTextField duplicates this check).
    Color backgroundColor(Color::white);
    if (o->style()->backgroundColor().isValid())
        backgroundColor = o->style()->backgroundColor();

    // If we have background-image, don't fill the content area to expose the
    // parent's background. Also, we shouldn't fill the content area if the
    // alpha of the color is 0. The API of Windows GDI ignores the alpha.
    //
    // Note that we should paint the content area white if we have neither the
    // background color nor background image explicitly specified to keep the
    // appearance of select element consistent with other browsers.
    bool fillContentArea = !o->style()->hasBackgroundImage() && backgroundColor.alpha();

    if (o->style()->hasBorderRadius()) {
        // If the style has rounded borders, setup the context to clip the
        // background (themed or filled) appropriately.
        // FIXME: make sure we do the right thing if css background-clip is set.
        i.context->save();
        IntSize topLeft, topRight, bottomLeft, bottomRight;
        o->style()->getBorderRadiiForRect(r, topLeft, topRight, bottomLeft, bottomRight);
        i.context->addRoundedRectClip(r, topLeft, topRight, bottomLeft, bottomRight);
    }
    {
        const ThemeData& themeData = getThemeData(o);
        WebCore::ThemePainter painter(i.context, r);
        ChromiumBridge::paintTextField(painter.context(),
                                       themeData.m_part,
                                       themeData.m_state,
                                       themeData.m_classicState,
                                       painter.drawRect(),
                                       backgroundColor,
                                       fillContentArea,
                                       drawEdges);
        // End of block commits the painter before restoring context.
    }
    if (o->style()->hasBorderRadius())
        i.context->restore();
    return false;
}

} // namespace WebCore
