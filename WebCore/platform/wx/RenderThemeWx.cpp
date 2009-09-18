/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "RenderTheme.h"

#include "Document.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HostWindow.h"
#include "NotImplemented.h"
#include "RenderView.h"

#include <wx/defs.h>

#include <wx/dc.h>
#include <wx/dcgraph.h>
#include <wx/renderer.h>
#include <wx/dcclient.h>
#include <wx/scrolwin.h>
#include <wx/settings.h>

namespace WebCore {

class RenderThemeWx : public RenderTheme {
private:
    RenderThemeWx() : RenderTheme() { }
    virtual ~RenderThemeWx();

public:
    static PassRefPtr<RenderTheme> create();

    // A method asking if the theme's controls actually care about redrawing when hovered.
    virtual bool supportsHover(const RenderStyle*) const { return true; }

    virtual bool paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
    {
        return paintButton(o, i, r);
    }
 
    virtual void setCheckboxSize(RenderStyle*) const;

    virtual bool paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
    {
        return paintButton(o, i, r);
    }

    virtual void setRadioSize(RenderStyle*) const;

    virtual void adjustRepaintRect(const RenderObject*, IntRect&);

    virtual void adjustButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustTextFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintTextField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual int minimumMenuListSize(RenderStyle*) const;

    virtual void adjustMenuListStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintMenuList(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustMenuListButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintMenuListButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual bool isControlStyled(const RenderStyle*, const BorderData&,
                                 const FillLayer&, const Color&) const;

    virtual bool controlSupportsTints(const RenderObject*) const;

    virtual void systemFont(int propId, FontDescription&) const;

    virtual Color platformActiveSelectionBackgroundColor() const;
    virtual Color platformInactiveSelectionBackgroundColor() const;
    
    virtual Color platformActiveSelectionForegroundColor() const;
    virtual Color platformInactiveSelectionForegroundColor() const;
    
    virtual int popupInternalPaddingLeft(RenderStyle*) const;
    virtual int popupInternalPaddingRight(RenderStyle*) const;
    virtual int popupInternalPaddingTop(RenderStyle*) const;
    virtual int popupInternalPaddingBottom(RenderStyle*) const;

private:
    void addIntrinsicMargins(RenderStyle*) const;
    void close();

    bool supportsFocus(ControlPart) const;
};


// Constants

#define MINIMUM_MENU_LIST_SIZE 21
#define POPUP_INTERNAL_PADDING_LEFT 6
#define POPUP_INTERNAL_PADDING_TOP 2
#define POPUP_INTERNAL_PADDING_BOTTOM 2

#ifdef __WXMAC__
#define POPUP_INTERNAL_PADDING_RIGHT 22
#else
#define POPUP_INTERNAL_PADDING_RIGHT 20
#endif

RenderThemeWx::~RenderThemeWx()
{
}

PassRefPtr<RenderTheme> RenderThemeWx::create()
{
    return adoptRef(new RenderThemeWx());
}

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page* page)
{
    static RenderTheme* rt = RenderThemeWx::create().releaseRef();
    return rt;
}

wxWindow* nativeWindowForRenderObject(RenderObject* o)
{
    FrameView* frameView = o->view()->frameView();
    ASSERT(frameView);
    ASSERT(frameView->hostWindow());
    return frameView->hostWindow()->platformPageClient();
}


bool RenderThemeWx::isControlStyled(const RenderStyle* style, const BorderData& border,
                                     const FillLayer& background, const Color& backgroundColor) const
{
    if (style->appearance() == TextFieldPart || style->appearance() == TextAreaPart)
        return style->border() != border;

    // Normally CSS can be used to set properties of form controls (such as adding a background bitmap).
    // However, for this to work RenderThemeWx needs to adjust uncustomized elements (e.g. buttons) to reflect the
    // changes made by CSS. Since we don't do that right now, the native parts of form elements appear in odd places. 
    // Until we have time to implement that support, we return false here, so that we ignore customizations 
    // and always use the native theme drawing to draw form controls.
    return false;
}

void RenderThemeWx::adjustRepaintRect(const RenderObject* o, IntRect& r)
{
    switch (o->style()->appearance()) {
        case MenulistPart: {
            r.setWidth(r.width() + 100);
            break;
        }
        default:
            break;
    }
}

bool RenderThemeWx::controlSupportsTints(const RenderObject* o) const
{
    if (!isEnabled(o))
        return false;

    // Checkboxes only have tint when checked.
    if (o->style()->appearance() == CheckboxPart)
        return isChecked(o);

    // For now assume other controls have tint if enabled.
    return true;
}

void RenderThemeWx::systemFont(int propId, FontDescription& fontDescription) const
{
    // no-op
}

void RenderThemeWx::addIntrinsicMargins(RenderStyle* style) const
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

void RenderThemeWx::setCheckboxSize(RenderStyle* style) const
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

void RenderThemeWx::setRadioSize(RenderStyle* style) const
{
    // This is the same as checkboxes.
    setCheckboxSize(style);
}

bool RenderThemeWx::supportsFocus(ControlPart part) const
{
    switch (part) {
        case PushButtonPart:
        case ButtonPart:
        case TextFieldPart:
            return true;
        default: // No for all others...
            return false;
    }
}

void RenderThemeWx::adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeWx::paintButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    wxWindow* window = nativeWindowForRenderObject(o);
    wxDC* dc = static_cast<wxDC*>(i.context->platformContext());
    int flags = 0;
    
    IntRect rect = r; 

#if USE(WXGC)
    double xtrans = 0;
    double ytrans = 0;
    
    wxGCDC* gcdc = static_cast<wxGCDC*>(dc);
    wxGraphicsContext* gc = gcdc->GetGraphicsContext();
    gc->GetTransform().TransformPoint(&xtrans, &ytrans);
    rect.setX(r.x() + (int)xtrans);
    rect.setY(r.y() + (int)ytrans);
#endif

    if (!isEnabled(o))
        flags |= wxCONTROL_DISABLED;

    ControlPart part = o->style()->appearance();
    if (supportsFocus(part) && isFocused(o))
        flags |= wxCONTROL_FOCUSED;

    if (isPressed(o))
        flags |= wxCONTROL_PRESSED;
    
    if (part == PushButtonPart || part == ButtonPart)
        wxRendererNative::Get().DrawPushButton(window, *dc, rect, flags);
    else if(part == RadioPart) {
        if (isChecked(o))
            flags |= wxCONTROL_CHECKED;
#if wxCHECK_VERSION(2,9,1)
        wxRendererNative::Get().DrawRadioBitmap(window, *dc, r, flags);
#elif wxCHECK_VERSION(2,9,0)
        wxRendererNative::Get().DrawRadioButton(window, *dc, rect, flags);
#else
        wxRenderer_DrawRadioButton(window, *dc, rect, flags);
#endif
    }
    else if(part == CheckboxPart) {
        if (isChecked(o))
            flags |= wxCONTROL_CHECKED;
        wxRendererNative::Get().DrawCheckBox(window, *dc, rect, flags);
    }
    return false;
}

void RenderThemeWx::adjustTextFieldStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    
}

bool RenderThemeWx::paintTextField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    wxWindow* window = nativeWindowForRenderObject(o);
    wxDC* dc = static_cast<wxDC*>(i.context->platformContext());
#if wxCHECK_VERSION(2,9,0)
    wxRendererNative::Get().DrawTextCtrl(window, *dc, r, 0);
#else
    wxRenderer_DrawTextCtrl(window, *dc, r, 0);
#endif

    return false;
}

int RenderThemeWx::minimumMenuListSize(RenderStyle*) const 
{ 
    return MINIMUM_MENU_LIST_SIZE; 
}

void RenderThemeWx::adjustMenuListStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
}
    
bool RenderThemeWx::paintMenuList(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    wxWindow* window = nativeWindowForRenderObject(o);
    wxDC* dc = static_cast<wxDC*>(i.context->platformContext());
    
    int flags = 0;      
    if (!isEnabled(o))
        flags |= wxCONTROL_DISABLED;
        
    if (supportsFocus(o->style()->appearance()) && isFocused(o))
        flags |= wxCONTROL_FOCUSED;

    if (isPressed(o))
        flags |= wxCONTROL_PRESSED;

#if wxCHECK_VERSION(2,9,0)
    wxRendererNative::Get().DrawChoice(window, *dc, r, flags);
#else
    wxRenderer_DrawChoice(window, *dc, r, flags);
#endif

    return false;
}

void RenderThemeWx::adjustMenuListButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const
{
    notImplemented();
}
    
bool RenderThemeWx::paintMenuListButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    wxWindow* window = nativeWindowForRenderObject(o);
    wxDC* dc = static_cast<wxDC*>(i.context->platformContext());
    
    int flags = 0;      
    if (!isEnabled(o))
        flags |= wxCONTROL_DISABLED;
        
    if (supportsFocus(o->style()->appearance()) && isFocused(o))
        flags |= wxCONTROL_FOCUSED;
    
    if (isPressed(o))
        flags |= wxCONTROL_PRESSED;

    wxRendererNative::Get().DrawComboBoxDropButton(window, *dc, r, flags);
            
    return false;
}

    
Color RenderThemeWx::platformActiveSelectionBackgroundColor() const
{
    return wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT);
}

Color RenderThemeWx::platformInactiveSelectionBackgroundColor() const
{
    return wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW);
}

Color RenderThemeWx::platformActiveSelectionForegroundColor() const
{
    // FIXME: Get wx to return the correct value for each platform.
#if __WXMAC__
    return Color();
#else
    return Color(255, 255, 255);
#endif
}

Color RenderThemeWx::platformInactiveSelectionForegroundColor() const
{
#if __WXMAC__
    return Color();
#else
    return Color(255, 255, 255);
#endif
}

int RenderThemeWx::popupInternalPaddingLeft(RenderStyle*) const 
{ 
    return POPUP_INTERNAL_PADDING_LEFT; 
}

int RenderThemeWx::popupInternalPaddingRight(RenderStyle*) const 
{
    return POPUP_INTERNAL_PADDING_RIGHT;
}

int RenderThemeWx::popupInternalPaddingTop(RenderStyle*) const 
{
    return POPUP_INTERNAL_PADDING_TOP;
}

int RenderThemeWx::popupInternalPaddingBottom(RenderStyle*) const
{ 
    return POPUP_INTERNAL_PADDING_BOTTOM; 
}

}

