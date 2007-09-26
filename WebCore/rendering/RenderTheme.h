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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef RenderTheme_h
#define RenderTheme_h

#include "RenderObject.h"

namespace WebCore {

class Element;
class PopupMenu;
class RenderMenuList;

enum ControlState {
    HoverState,
    PressedState,
    FocusState,
    EnabledState,
    CheckedState,
    ReadOnlyState
};

class RenderTheme {
public:
    RenderTheme() { }
    virtual ~RenderTheme() { }

    // This method is called whenever style has been computed for an element and the appearance
    // property has been set to a value other than "none".  The theme should map in all of the appropriate
    // metrics and defaults given the contents of the style.  This includes sophisticated operations like
    // selection of control size based off the font, the disabling of appearance when certain other properties like
    // "border" are set, or if the appearance is not supported by the theme.
    void adjustStyle(CSSStyleSelector*, RenderStyle*, Element*,  bool UAHasAppearance,
                     const BorderData&, const BackgroundLayer&, const Color& backgroundColor);

    // This method is called to paint the widget as a background of the RenderObject.  A widget's foreground, e.g., the
    // text of a button, is always rendered by the engine itself.  The boolean return value indicates
    // whether the CSS border/background should also be painted.
    bool paint(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    bool paintBorderOnly(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    bool paintDecorations(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    // The remaining methods should be implemented by the platform-specific portion of the theme, e.g.,
    // RenderThemeMac.cpp for Mac OS X.

    // A method to obtain the baseline position for a "leaf" control.  This will only be used if a baseline
    // position cannot be determined by examining child content. Checkboxes and radio buttons are examples of
    // controls that need to do this.
    virtual short baselinePosition(const RenderObject*) const;

    // A method for asking if a control is a container or not.  Leaf controls have to have some special behavior (like
    // the baseline position API above).
    virtual bool isControlContainer(EAppearance) const;

    // A method asking if the control changes its tint when the window has focus or not.
    virtual bool controlSupportsTints(const RenderObject*) const { return false; }

    // Whether or not the control has been styled enough by the author to disable the native appearance.
    virtual bool isControlStyled(const RenderStyle*, const BorderData&, const BackgroundLayer&, const Color& backgroundColor) const;

    // A general method asking if any control tinting is supported at all.
    virtual bool supportsControlTints() const { return false; }

    // Some controls may spill out of their containers (e.g., the check on an OS X checkbox).  When these controls repaint,
    // the theme needs to communicate this inflated rect to the engine so that it can invalidate the whole control.
    virtual void adjustRepaintRect(const RenderObject*, IntRect&) { }

    // This method is called whenever a relevant state changes on a particular themed object, e.g., the mouse becomes pressed
    // or a control becomes disabled.
    virtual bool stateChanged(RenderObject*, ControlState) const;

    // This method is called whenever the theme changes on the system in order to flush cached resources from the
    // old theme.
    virtual void themeChanged() { }

    // A method asking if the theme is able to draw the focus ring.
    virtual bool supportsFocusRing(const RenderStyle*) const;

    // A method asking if the theme's controls actually care about redrawing when hovered.
    virtual bool supportsHover(const RenderStyle*) const { return false; }

    // The selection color.
    Color activeSelectionBackgroundColor() const;
    Color inactiveSelectionBackgroundColor() const;

    // The platform selection color.
    virtual Color platformActiveSelectionBackgroundColor() const;
    virtual Color platformInactiveSelectionBackgroundColor() const;
    virtual Color platformActiveSelectionForegroundColor() const;
    virtual Color platformInactiveSelectionForegroundColor() const;

    // List Box selection color
    virtual Color activeListBoxSelectionBackgroundColor() const;
    virtual Color activeListBoxSelectionForegroundColor() const;
    virtual Color inactiveListBoxSelectionBackgroundColor() const;
    virtual Color inactiveListBoxSelectionForegroundColor() const;

    void platformColorsDidChange();

    // System fonts.
    virtual void systemFont(int propId, FontDescription&) const = 0;

    virtual int minimumMenuListSize(RenderStyle*) const { return 0; }

    virtual void adjustSliderThumbSize(RenderObject*) const;

    // Methods for state querying
    bool isChecked(const RenderObject*) const;
    bool isIndeterminate(const RenderObject*) const;
    bool isEnabled(const RenderObject*) const;
    bool isFocused(const RenderObject*) const;
    bool isPressed(const RenderObject*) const;
    bool isHovered(const RenderObject*) const;
    bool isReadOnlyControl(const RenderObject*) const;
    
    virtual int popupInternalPaddingLeft(RenderStyle*) const { return 0; }
    virtual int popupInternalPaddingRight(RenderStyle*) const { return 0; }
    virtual int popupInternalPaddingTop(RenderStyle*) const { return 0; }
    virtual int popupInternalPaddingBottom(RenderStyle*) const { return 0; }

protected:
    // Methods for each appearance value.
    virtual void adjustCheckboxStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintCheckbox(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }
    virtual void setCheckboxSize(RenderStyle*) const { }

    virtual void adjustRadioStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintRadio(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }
    virtual void setRadioSize(RenderStyle*) const { }

    virtual void adjustButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }
    virtual void setButtonSize(RenderStyle*) const { }

    virtual void adjustTextFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintTextField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }

    virtual void adjustTextAreaStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintTextArea(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }

    virtual void adjustMenuListStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintMenuList(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }

    virtual void adjustMenuListButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintMenuListButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }

    virtual void adjustSliderTrackStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSliderTrack(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }

    virtual void adjustSliderThumbStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSliderThumb(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }

    virtual void adjustSearchFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }

    virtual void adjustSearchFieldCancelButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldCancelButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }

    virtual void adjustSearchFieldDecorationStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldDecoration(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }

    virtual void adjustSearchFieldResultsDecorationStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldResultsDecoration(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }

    virtual void adjustSearchFieldResultsButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldResultsButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }

private:
    mutable Color m_activeSelectionColor;
    mutable Color m_inactiveSelectionColor;
};

// Function to obtain the theme.  This is implemented in your platform-specific theme implementation to hand
// back the appropriate platform theme.
RenderTheme* theme();

} // namespace WebCore

#endif // RenderTheme_h
