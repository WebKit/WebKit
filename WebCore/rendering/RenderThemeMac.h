/*
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2005 Apple Computer, Inc.
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

#ifndef RENDER_THEME_MAC_H
#define RENDER_THEME_MAC_H

#import "RenderTheme.h"

namespace WebCore {

class RenderStyle;
class RenderPopupMenu;

class RenderThemeMac : public RenderTheme {
public:
    RenderThemeMac();
    virtual ~RenderThemeMac() { /* Have to just leak the cells, since statics are destroyed with no autorelease pool available */ }

    // A method to obtain the baseline position for a "leaf" control.  This will only be used if a baseline
    // position cannot be determined by examining child content. Checkboxes and radio buttons are examples of
    // controls that need to do this.
    virtual short baselinePosition(const RenderObject*) const;

    // A method asking if the control changes its tint when the window has focus or not.
    virtual bool controlSupportsTints(const RenderObject*) const;

    // A general method asking if any control tinting is supported at all.
    virtual bool supportsControlTints() const { return true; }

    virtual void adjustRepaintRect(const RenderObject*, IntRect&);

    virtual bool isControlStyled(const RenderStyle* style, const BorderData& border, 
                                 const BackgroundLayer& background, const Color& backgroundColor) const;
                                
    virtual void paintResizeControl(GraphicsContext*, const IntRect&);

    virtual Color platformActiveSelectionBackgroundColor() const;
    virtual Color platformInactiveSelectionBackgroundColor() const;
    
    virtual int minimumTextSize(RenderStyle*) const;
    virtual RenderPopupMenu* createPopupMenu(RenderArena*, Document*, RenderMenuList*);

protected:
    // Methods for each appearance value.
    virtual bool paintCheckbox(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    virtual void setCheckboxSize(RenderStyle*) const;
    
    virtual bool paintRadio(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    virtual void setRadioSize(RenderStyle*) const;
    
    virtual void adjustButtonStyle(CSSStyleSelector*, RenderStyle*, WebCore::Element*) const;
    virtual bool paintButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    virtual void setButtonSize(RenderStyle*) const;
    
    virtual bool paintTextField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    virtual void adjustTextFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;

    virtual bool paintTextArea(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    virtual void adjustTextAreaStyle(CSSStyleSelector*, RenderStyle*, Element*) const;

    virtual bool paintMenuList(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    virtual void adjustMenuListStyle(CSSStyleSelector*, RenderStyle*, Element*) const;

private:
    IntRect inflateRect(const IntRect&, const IntSize&, const int* margins) const;

    // Get the control size based off the font.  Used by some of the controls (like buttons).
    NSControlSize controlSizeForFont(RenderStyle*) const;
    void setControlSize(NSCell*, const IntSize* sizes, const IntSize& minSize);
    void setSizeFromFont(RenderStyle*, const IntSize* sizes) const;
    IntSize sizeForFont(RenderStyle*, const IntSize* sizes) const;
    void setFontFromControlSize(CSSStyleSelector*, RenderStyle*, NSControlSize) const;
    
    void addIntrinsicMargins(RenderStyle*, NSControlSize) const;
    
    void updateCheckedState(NSCell*, const RenderObject*);
    void updateEnabledState(NSCell*, const RenderObject*);
    void updateFocusedState(NSCell*, const RenderObject*);
    void updatePressedState(NSCell*, const RenderObject*);

    // Helpers for adjusting appearance and for painting
    const IntSize* checkboxSizes() const;
    const int* checkboxMargins() const;
    void setCheckboxCellState(const RenderObject*, const IntRect&);

    const IntSize* radioSizes() const;
    const int* radioMargins() const;
    void setRadioCellState(const RenderObject*, const IntRect&);

    void setButtonPaddingFromControlSize(RenderStyle*, NSControlSize) const;
    const IntSize* buttonSizes() const;
    const int* buttonMargins() const;
    void setButtonCellState(const RenderObject*, const IntRect&);
    
    void setPopupPaddingFromControlSize(RenderStyle*, NSControlSize) const;
    void setPopupButtonCellState(const RenderObject*, const IntRect&);
    const IntSize* popupButtonSizes() const;
    const int* popupButtonMargins() const;
    const int* popupButtonPadding(NSControlSize) const;

private:
    NSButtonCell* checkbox;
    NSButtonCell* radio;
    NSButtonCell* button;
    NSPopUpButtonCell* popupButton;
    Image* resizeCornerImage;
};

}

#endif
