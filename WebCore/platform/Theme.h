/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef Theme_h
#define Theme_h

#include "Color.h"
#include "Font.h"
#include "IntSize.h"
#include "IntRect.h"
#include "PlatformString.h"
#include "ThemeTypes.h"

namespace WebCore {

class GraphicsContext;

// Unlike other platform classes, Theme does extensively use virtual functions.  This design allows a platform to switch between multiple themes at runtime.
class Theme {
public:
    Theme() { }
    virtual ~Theme() { }

    // A method to obtain the baseline position adjustment for a "leaf" control.  This will only be used if a baseline
    // position cannot be determined by examining child content. Checkboxes and radio buttons are examples of
    // controls that need to do this.  The adjustment is an offset that adds to the baseline, e.g., marginTop() + height() + |offset|.
    virtual int baselinePositionAdjustment(ControlPart) const { return 0; }

    // A method asking if the control changes its appearance when the window is inactive.
    virtual bool controlHasInactiveAppearance(ControlPart) const { return false; }
    
    // General methods for whether or not any of the controls in the theme change appearance when the window is inactive or
    // when hovered over.
    virtual bool controlsCanHaveInactiveAppearance() const { return false; }
    virtual bool controlsCanHaveHoveredAppearance() const { return false; }

    // Used by RenderTheme::isControlStyled to figure out if the native look and feel should be turned off.
    virtual bool controlDrawsBorder(ControlPart) const { return true; }
    virtual bool controlDrawsBackground(ControlPart) const { return true; }
    virtual bool controlDrawsFocusOutline(ControlPart) const { return true; }

    // Methods for obtaining platform-specific colors.
    virtual Color selectionColor(ControlPart, ControlState, SelectionPart) const { return Color(); }
    virtual Color textSearchHighlightColor() const { return Color(); }
    
    // CSS system colors and fonts
    virtual Color systemColor(ThemeColor) const { return Color(); }
    virtual Font systemFont(ThemeFont, FontDescription&) const { return Font(); }
    
    // How fast the caret blinks in text fields.
    virtual double caretBlinkFrequency() const { return 0.5; }

    // Notification when the theme has changed
    virtual void themeChanged() { }

    // Methods used to adjust the RenderStyles of controls.    
    virtual IntSize controlSize(ControlPart, const Font&) { return IntSize(); }
    virtual Font controlFont(ControlPart, const Font& font) { return font; }
    virtual ControlBox controlPadding(ControlPart, ControlBox box, const Font&) { return box; }
    virtual ControlBox controlInternalPadding(ControlPart) { return ControlBox(); }
    virtual ControlBox controlBorder(ControlPart, ControlBox box, const Font&) { return box; }
    virtual int controlBorderRadius(ControlPart, const Font&) { return 0; }

    // Method for painting a control.
    virtual void paint(ControlPart, ControlStates, GraphicsContext*, const IntRect&) { };

    // Some controls may spill out of their containers (e.g., the check on an OS X checkbox).  When these controls repaint,
    // the theme needs to communicate this inflated rect to the engine so that it can invalidate the whole control.
    virtual void inflateControlPaintRect(ControlPart, ControlStates, IntRect&) { }
    
    // This method is called once, from RenderTheme::adjustDefaultStyleSheet(), to let each platform adjust
    // the default CSS rules in html4.css.
    static String defaultStyleSheet();

private:
    mutable Color m_activeSelectionColor;
    mutable Color m_inactiveSelectionColor;
};

// Function to obtain the theme.  This is implemented in the platform-specific subclasses.
Theme* platformTheme();

} // namespace WebCore

#endif // Theme_h
