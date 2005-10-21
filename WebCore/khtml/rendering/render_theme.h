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

#ifndef RENDER_THEME_H
#define RENDER_THEME_H

#include "render_object.h"

namespace DOM {
class ElementImpl;
};

namespace khtml {

class RenderStyle;

enum ControlState { HoverState, PressedState, FocusState, EnabledState, CheckedState };

class RenderTheme {
public:
    RenderTheme() {}
    virtual ~RenderTheme() {}

    // This method is called whenever style has been computed for an element and the appearance
    // property has been set to a value other than "none".  The theme should map in all of the appropriate
    // metrics and defaults given the contents of the style.  This includes sophisticated operations like
    // selection of control size based off the font, the disabling of appearance when certain other properties like
    // "border" are set, or if the appearance is not supported by the theme.
    void adjustStyle(CSSStyleSelector* selector, RenderStyle* style, DOM::ElementImpl* e);
    
    // This method is called to paint the widget as a background of the RenderObject.  A widget's foreground, e.g., the
    // text of a button, is always rendered by the engine itself.
    void paint(RenderObject* o, const RenderObject::PaintInfo& i, const QRect& r);

    // The remaining methods should be implemented by the platform-specific portion of the theme, e.g.,
    // render_theme_mac.cpp for Mac OS X.
    
    // An API to obtain the baseline position for a "leaf" control.  This will only be used if a baseline
    // position cannot be determined by examining child content. Checkboxes and radio buttons are examples of
    // controls that need to do this.
    virtual short baselinePosition(const RenderObject* o) const;

    // An API for asking if a control is a container or not.  Leaf controls have to have some special behavior (like
    // the baseline position API above).
    virtual bool isControlContainer(EAppearance appearance) const;

    // An API asking if the control changes its tint when the window has focus or not.
    virtual bool controlSupportsTints(const RenderObject* o) const { return false; }

    // A general API asking if any control tinting is supported at all.
    virtual bool supportsControlTints() const { return false; }

    // Some controls may spill out of their containers (e.g., the check on an OS X checkbox).  When these controls repaint,
    // the theme needs to communicate this inflated rect to the engine so that it can invalidate the whole control.
    virtual void adjustRepaintRect(const RenderObject* o, QRect& r) { }
    
    // This method is called whenever a relevant state changes on a particular themed object, e.g., the mouse becomes pressed
    // or a control becomes disabled.
    virtual bool stateChanged(RenderObject* o, ControlState state) const;

    // This method is called whenever the theme changes on the system in order to flush cached resources from the
    // old theme.
    void themeChanged();

protected:
    // Methods for state querying
    bool isChecked(const RenderObject* o) const;
    bool isEnabled(const RenderObject* o) const;
    bool isFocused(const RenderObject* o) const;
    bool isPressed(const RenderObject* o) const;

    // Methods for each appearance value.
    virtual void adjustCheckboxStyle(CSSStyleSelector* selector, RenderStyle* style, DOM::ElementImpl* e) const;
    virtual void paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const QRect& r) = 0;
    virtual void setCheckboxSize(RenderStyle* style) const {};
    
    virtual void adjustRadioStyle(CSSStyleSelector* selector, RenderStyle* style, DOM::ElementImpl* e) const;
    virtual void paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const QRect& r) = 0;
    virtual void setRadioSize(RenderStyle* style) const {};
    
    virtual void adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, DOM::ElementImpl* e) const;
    virtual void paintButton(RenderObject* o, const RenderObject::PaintInfo& i, const QRect& r) = 0;
    virtual void setButtonSize(RenderStyle* style) const {};
};

// Function to obtain the theme.  This is implemented in your platform-specific theme implementation to hand
// back the appropriate platform theme.
RenderTheme* theme();

}

#endif
