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
#include "RenderView.h"

#include <wx/defs.h>
#include <wx/renderer.h>
#include <wx/dcclient.h>
#include <wx/scrolwin.h>
#include <wx/settings.h>

namespace WebCore {

class RenderThemeWx : public RenderTheme {
public:
    RenderThemeWx() : RenderTheme() { }

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

    virtual void adjustButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustTextFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintTextField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual bool isControlStyled(const RenderStyle*, const BorderData&,
                                 const BackgroundLayer&, const Color&) const;

    virtual bool controlSupportsTints(const RenderObject*) const;

    virtual void systemFont(int propId, FontDescription&) const;

    virtual Color platformActiveSelectionBackgroundColor() const;
    virtual Color platformInactiveSelectionBackgroundColor() const;
    
    virtual Color platformActiveSelectionForegroundColor() const;
    virtual Color platformInactiveSelectionForegroundColor() const;

private:
    void addIntrinsicMargins(RenderStyle*) const;
    void close();

    bool supportsFocus(EAppearance) const;
};

RenderTheme* theme()
{
    static RenderThemeWx rt;
    return &rt;
}

bool RenderThemeWx::isControlStyled(const RenderStyle* style, const BorderData& border,
                                     const BackgroundLayer& background, const Color& backgroundColor) const
{
    if (style->appearance() == TextFieldAppearance || style->appearance() == TextAreaAppearance)
        return style->border() != border;

    return RenderTheme::isControlStyled(style, border, background, backgroundColor);
}

bool RenderThemeWx::controlSupportsTints(const RenderObject* o) const
{
    if (!isEnabled(o))
        return false;

    // Checkboxes only have tint when checked.
    if (o->style()->appearance() == CheckboxAppearance)
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

bool RenderThemeWx::supportsFocus(EAppearance appearance) const
{
    switch (appearance) {
        case PushButtonAppearance:
        case ButtonAppearance:
        case TextFieldAppearance:
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
    wxWindow* window = o->view()->frameView()->nativeWindow();
    wxDC* dc = static_cast<wxDC*>(i.context->platformContext());
    wxASSERT(dc->IsOk());

    int flags = 0;
    
    if (!isEnabled(o))
        flags |= wxCONTROL_DISABLED;

    EAppearance appearance = o->style()->appearance();
    if (supportsFocus(o->style()->appearance()) && isFocused(o))
        flags |= wxCONTROL_FOCUSED;

    if (isPressed(o))
        flags |= wxCONTROL_PRESSED;
    
    if (appearance == PushButtonAppearance || appearance == ButtonAppearance)
        wxRendererNative::Get().DrawPushButton(window, *dc, r, flags);
    // TODO: add a radio button rendering API to wx
    //else if(appearance == RadioAppearance)
    else if(appearance == CheckboxAppearance) {
        if (isChecked(o))
            flags |= wxCONTROL_CHECKED;
        wxRendererNative::Get().DrawCheckBox(window, *dc, r, flags);
    }
        
    return false;
}

void RenderThemeWx::adjustTextFieldStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    
}

bool RenderThemeWx::paintTextField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    i.context->setStrokeThickness(1);
    i.context->setStrokeColor(Color(0, 0, 0));
    i.context->drawRect(r);
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

}

