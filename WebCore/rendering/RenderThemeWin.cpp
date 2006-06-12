/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
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
#include "RenderThemeWin.h"

#include <cairo-win32.h>
#include "GraphicsContext.h"

/* 
 * The following constants are used to determine how a widget is drawn using
 * Windows' Theme API. For more information on theme parts and states see
 * http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/commctls/userex/topics/partsandstates.asp
 */
#define THEME_COLOR 204
#define THEME_FONT  210

// Generic state constants
#define TS_NORMAL    1
#define TS_HOVER     2
#define TS_ACTIVE    3
#define TS_DISABLED  4
#define TS_FOCUSED   5

// Button constants
#define BP_BUTTON    1
#define BP_RADIO     2
#define BP_CHECKBOX  3

// Textfield constants
#define TFP_TEXTFIELD 1
#define TFS_READONLY  6

typedef HANDLE (WINAPI*openThemeDataPtr)(HWND hwnd, LPCWSTR pszClassList);
typedef HRESULT (WINAPI*closeThemeDataPtr)(HANDLE hTheme);
typedef HRESULT (WINAPI*drawThemeBackgroundPtr)(HANDLE hTheme, HDC hdc, int iPartId, 
                                          int iStateId, const RECT *pRect,
                                          const RECT* pClipRect);
typedef HRESULT (WINAPI*drawThemeEdgePtr)(HANDLE hTheme, HDC hdc, int iPartId, 
                                          int iStateId, const RECT *pRect,
                                          unsigned uEdge, unsigned uFlags,
                                          const RECT* pClipRect);
typedef HRESULT (WINAPI*getThemeContentRectPtr)(HANDLE hTheme, HDC hdc, int iPartId,
                                          int iStateId, const RECT* pRect,
                                          RECT* pContentRect);
typedef HRESULT (WINAPI*getThemePartSizePtr)(HANDLE hTheme, HDC hdc, int iPartId,
                                       int iStateId, RECT* prc, int ts,
                                       SIZE* psz);
typedef HRESULT (WINAPI*getThemeSysFontPtr)(HANDLE hTheme, int iFontId, OUT LOGFONT* pFont);
typedef HRESULT (WINAPI*getThemeColorPtr)(HANDLE hTheme, HDC hdc, int iPartId,
                                   int iStateId, int iPropId, OUT COLORREF* pFont);

static openThemeDataPtr openTheme = 0;
static closeThemeDataPtr closeTheme = 0;
static drawThemeBackgroundPtr drawThemeBG = 0;
static drawThemeEdgePtr drawThemeEdge = 0;
static getThemeContentRectPtr getThemeContentRect = 0;
static getThemePartSizePtr getThemePartSize = 0;
static getThemeSysFontPtr getThemeSysFont = 0;
static getThemeColorPtr getThemeColor = 0;

namespace WebCore {

RenderTheme* theme()
{
    static RenderThemeWin winTheme;
    return &winTheme;
}

RenderThemeWin::RenderThemeWin()
:m_themeDLL(0), m_buttonTheme(0), m_textFieldTheme(0)
{
    m_themeDLL = ::LoadLibrary(L"uxtheme.dll");
    if (m_themeDLL) {
        openTheme = (openThemeDataPtr)GetProcAddress(m_themeDLL, "OpenThemeData");
        closeTheme = (closeThemeDataPtr)GetProcAddress(m_themeDLL, "CloseThemeData");
        drawThemeBG = (drawThemeBackgroundPtr)GetProcAddress(m_themeDLL, "DrawThemeBackground");
        drawThemeEdge = (drawThemeEdgePtr)GetProcAddress(m_themeDLL, "DrawThemeEdge");
        getThemeContentRect = (getThemeContentRectPtr)GetProcAddress(m_themeDLL, "GetThemeBackgroundContentRect");
        getThemePartSize = (getThemePartSizePtr)GetProcAddress(m_themeDLL, "GetThemePartSize");
        getThemeSysFont = (getThemeSysFontPtr)GetProcAddress(m_themeDLL, "GetThemeSysFont");
        getThemeColor = (getThemeColorPtr)GetProcAddress(m_themeDLL, "GetThemeColor");
    }
}

RenderThemeWin::~RenderThemeWin()
{
    if (!m_themeDLL)
        return;

    close();

    ::FreeLibrary(m_themeDLL);
}

void RenderThemeWin::close()
{
    // This method will need to be called when the OS theme changes to flush our cached themes.
    if (m_buttonTheme)
        closeTheme(m_buttonTheme);
    if (m_textFieldTheme)
        closeTheme(m_textFieldTheme);
    m_buttonTheme = m_textFieldTheme = 0;
}

Color RenderThemeWin::platformActiveSelectionBackgroundColor() const
{
    COLORREF color = GetSysColor(COLOR_HIGHLIGHT);
    return Color(GetRValue(color), GetGValue(color), GetBValue(color), 255);
}

Color RenderThemeWin::platformInactiveSelectionBackgroundColor() const
{
    COLORREF color = GetSysColor(COLOR_GRAYTEXT);
    return Color(GetRValue(color), GetGValue(color), GetBValue(color), 255);
}

Color RenderThemeWin::platformActiveSelectionForegroundColor() const
{
    COLORREF color = GetSysColor(COLOR_HIGHLIGHTTEXT);
    return Color(GetRValue(color), GetGValue(color), GetBValue(color), 255);
}

Color RenderThemeWin::platformInactiveSelectionForegroundColor() const
{
    return Color::white;
}

void RenderThemeWin::addIntrinsicMargins(RenderStyle* style) const
{
    // Cut out the intrinsic margins completely if we end up using a small font size
    if (style->fontSize() < 11)
        return;
    
    // Intrinsic margin value.
    const int m = 2;
    
    // FIXME: Using width/height alone and not also dealing with min-width/max-width is flawed.
    if (style->width().isIntrinsicOrAuto()) {
        if (style->marginLeft().quirk())
            style->setMarginLeft(Length(m, Fixed));
        if (style->marginRight().quirk())
            style->setMarginRight(Length(m, Fixed));
    }

    if (style->height().isAuto()) {
        if (style->marginTop().quirk())
            style->setMarginTop(Length(m, Fixed));
        if (style->marginBottom().quirk())
            style->setMarginBottom(Length(m, Fixed));
    }
}

bool RenderThemeWin::supportsFocus(EAppearance appearance)
{
    switch (appearance) {
        case PushButtonAppearance:
        case ButtonAppearance:
        case TextFieldAppearance:
        case TextAreaAppearance:
            return true;
        default:
            return false;
    }

    return false;
}

unsigned RenderThemeWin::determineState(RenderObject* o)
{
    unsigned result = TS_NORMAL;
    if (!isEnabled(o))
        result = TS_DISABLED;
    else if (isReadOnlyControl(o))
        result = TFS_READONLY; // Readonly is supported on textfields.
    else if (supportsFocus(o->style()->appearance()) && isFocused(o))
        result = TS_FOCUSED;
    else if (isHovered(o))
        result = TS_HOVER;
    else if (isPressed(o))
        result = TS_ACTIVE;
    if (isChecked(o))
        result += 4; // 4 unchecked states, 4 checked states.
    return result;
}

ThemeData RenderThemeWin::getThemeData(RenderObject* o)
{
    ThemeData result;
    switch (o->style()->appearance()) {
        case PushButtonAppearance:
        case ButtonAppearance:
            result.m_part = BP_BUTTON;
            result.m_state = determineState(o);
            break;
        case CheckboxAppearance:
            result.m_part = BP_CHECKBOX;
            result.m_state = determineState(o);
            break;
        case RadioAppearance:
            result.m_part = BP_RADIO;
            result.m_state = determineState(o);
            break;
        case TextFieldAppearance:
        case TextAreaAppearance:
            result.m_part = TFP_TEXTFIELD;
            result.m_state = determineState(o);
            break;
    }

    return result;
}

void RenderThemeWin::adjustButtonStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    addIntrinsicMargins(style);
}

static HDC prepareForDrawing(GraphicsContext* g)
{
    cairo_surface_t* surface = cairo_get_target(g->platformContext());
    HDC hdc = cairo_win32_surface_get_dc(surface);
    SaveDC(hdc);
    
    // FIXME: We need to make sure a clip is really set on the HDC.  See what Mozilla does with
    // UpdateSurfaceClip on its GraphicsContext. (Cairo may not have put its clip into native form yet.)

    // Call SetWorldTransform to honor the current Cairo transform.
    SetGraphicsMode(hdc, GM_ADVANCED); // We need this call for themes to honor world transforms.
    cairo_matrix_t mat;
    cairo_get_matrix(g->platformContext(), &mat);
    XFORM xform;
    xform.eM11 = mat.xx;
    xform.eM12 = mat.xy;
    xform.eM21 = mat.yx;
    xform.eM22 = mat.yy;
    xform.eDx = mat.x0;
    xform.eDy = mat.y0;
    SetWorldTransform(hdc, &xform);

    return hdc;
}

static void doneDrawing(GraphicsContext* g)
{
    cairo_surface_t* surface = cairo_get_target(g->platformContext());
    HDC hdc = cairo_win32_surface_get_dc(surface);
    RestoreDC(hdc, -1);

    // We call this whenever we do drawing outside of cairo.    
    cairo_surface_mark_dirty(surface);
}

bool RenderThemeWin::paintButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    // FIXME: Need to fall back to painting a Win2k "Classic" look.  We will hit this situation if
    // a Windows XP user has turned on the Win2k "Classic" appearance or is running Win2k.
    if (!m_themeDLL)
        return true;

    if (!m_buttonTheme)
        m_buttonTheme = openTheme(0, L"Button");

    if (!m_buttonTheme || !drawThemeBG)
        return true;

    // Get the correct theme data for a button
    ThemeData themeData = getThemeData(o);
    
    // Now paint the button.
    HDC hdc = prepareForDrawing(i.p);  
    RECT widgetRect = r;
    drawThemeBG(m_buttonTheme, hdc, themeData.m_part, themeData.m_state, &widgetRect, NULL);
    doneDrawing(i.p);

    return false;
}

void RenderThemeWin::setCheckboxSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    // FIXME:  A hard-coded size of 13 is used.  This is wrong but necessary for now.  It matches Firefox.
    // At different DPI settings on Windows, querying the theme gives you a larger size that accounts for
    // the higher DPI.  Until our entire engine honors a DPI setting other than 96, we can't rely on the theme's
    // metrics.
    if (style->width().isIntrinsicOrAuto())
        style->setWidth(Length(13, Fixed));
    if (style->height().isAuto())
        style->setHeight(Length(13, Fixed));
}

void RenderThemeWin::setRadioSize(RenderStyle* style) const
{
    // This is the same as checkboxes.
    setCheckboxSize(style);
}

void RenderThemeWin::adjustTextFieldStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeWin::paintTextField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    // FIXME: Need to fall back to painting a Win2k "Classic" look.  We will hit this situation if
    // a Windows XP user has turned on the Win2k "Classic" appearance or is running Win2k.
    if (!m_themeDLL)
        return true;

    if (!m_textFieldTheme)
        m_textFieldTheme = openTheme(0, L"Edit");

    if (!m_textFieldTheme || !drawThemeBG)
        return true;

    // Get the correct theme data for a button
    ThemeData themeData = getThemeData(o);
    
    // Now paint the text field.
    HDC hdc = prepareForDrawing(i.p);
    RECT widgetRect = r;
    drawThemeBG(m_textFieldTheme, hdc, themeData.m_part, themeData.m_state, &widgetRect, NULL);
    doneDrawing(i.p);

    return false;
}

void RenderThemeWin::adjustTextAreaStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeWin::paintTextArea(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

}
