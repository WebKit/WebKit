/*
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Computer, Inc.
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
#if USE(NEW_THEME)
#include "Theme.h"
#else
#include "ThemeTypes.h"
#endif
#include "ScrollTypes.h"

namespace WebCore {

class Element;
class PopupMenu;
class RenderMenuList;
class CSSStyleSheet;

class RenderTheme {
public:
    RenderTheme();
    virtual ~RenderTheme() { }

    // This method is called whenever style has been computed for an element and the appearance
    // property has been set to a value other than "none".  The theme should map in all of the appropriate
    // metrics and defaults given the contents of the style.  This includes sophisticated operations like
    // selection of control size based off the font, the disabling of appearance when certain other properties like
    // "border" are set, or if the appearance is not supported by the theme.
    void adjustStyle(CSSStyleSelector*, RenderStyle*, Element*,  bool UAHasAppearance,
                     const BorderData&, const FillLayer&, const Color& backgroundColor);

    // This method is called to paint the widget as a background of the RenderObject.  A widget's foreground, e.g., the
    // text of a button, is always rendered by the engine itself.  The boolean return value indicates
    // whether the CSS border/background should also be painted.
    bool paint(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    bool paintBorderOnly(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    bool paintDecorations(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    // The remaining methods should be implemented by the platform-specific portion of the theme, e.g.,
    // RenderThemeMac.cpp for Mac OS X.

    // These methods return the theme's extra style sheets rules, to let each platform
    // adjust the default CSS rules in html4.css, quirks.css, or mediaControls.css
    virtual String extraDefaultStyleSheet() { return String(); }
    virtual String extraQuirksStyleSheet() { return String(); }
#if ENABLE(VIDEO)
    virtual String extraMediaControlsStyleSheet() { return String(); };
#endif

    // A method to obtain the baseline position for a "leaf" control.  This will only be used if a baseline
    // position cannot be determined by examining child content. Checkboxes and radio buttons are examples of
    // controls that need to do this.
    virtual int baselinePosition(const RenderObject*) const;

    // A method for asking if a control is a container or not.  Leaf controls have to have some special behavior (like
    // the baseline position API above).
    bool isControlContainer(ControlPart) const;

    // A method asking if the control changes its tint when the window has focus or not.
    virtual bool controlSupportsTints(const RenderObject*) const { return false; }

    // Whether or not the control has been styled enough by the author to disable the native appearance.
    virtual bool isControlStyled(const RenderStyle*, const BorderData&, const FillLayer&, const Color& backgroundColor) const;

    // A general method asking if any control tinting is supported at all.
    virtual bool supportsControlTints() const { return false; }

    // Some controls may spill out of their containers (e.g., the check on an OS X checkbox).  When these controls repaint,
    // the theme needs to communicate this inflated rect to the engine so that it can invalidate the whole control.
    virtual void adjustRepaintRect(const RenderObject*, IntRect&);

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

    // Text selection colors.
    Color activeSelectionBackgroundColor() const;
    Color inactiveSelectionBackgroundColor() const;
    Color activeSelectionForegroundColor() const;
    Color inactiveSelectionForegroundColor() const;

    // List box selection colors
    Color activeListBoxSelectionBackgroundColor() const;
    Color activeListBoxSelectionForegroundColor() const;
    Color inactiveListBoxSelectionBackgroundColor() const;
    Color inactiveListBoxSelectionForegroundColor() const;

    // Highlighting colors for TextMatches.
    virtual Color platformActiveTextSearchHighlightColor() const;
    virtual Color platformInactiveTextSearchHighlightColor() const;

    virtual void platformColorsDidChange();

    virtual double caretBlinkInterval() const { return 0.5; }

    // System fonts and colors for CSS.
    virtual void systemFont(int cssValueId, FontDescription&) const = 0;
    virtual Color systemColor(int cssValueId) const;

    virtual int minimumMenuListSize(RenderStyle*) const { return 0; }

    virtual void adjustSliderThumbSize(RenderObject*) const;

    virtual int popupInternalPaddingLeft(RenderStyle*) const { return 0; }
    virtual int popupInternalPaddingRight(RenderStyle*) const { return 0; }
    virtual int popupInternalPaddingTop(RenderStyle*) const { return 0; }
    virtual int popupInternalPaddingBottom(RenderStyle*) const { return 0; }
    virtual bool popupOptionSupportsTextIndent() const { return false; }

    virtual int buttonInternalPaddingLeft() const { return 0; }
    virtual int buttonInternalPaddingRight() const { return 0; }
    virtual int buttonInternalPaddingTop() const { return 0; }
    virtual int buttonInternalPaddingBottom() const { return 0; }

    virtual ScrollbarControlSize scrollbarControlSizeForPart(ControlPart) { return RegularScrollbar; }

    // Method for painting the caps lock indicator
    virtual bool paintCapsLockIndicator(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return 0; };

#if ENABLE(VIDEO)
    // Media controls
    virtual bool hitTestMediaControlPart(RenderObject*, const IntPoint& absPoint);
#endif

protected:
    // The platform selection color.
    virtual Color platformActiveSelectionBackgroundColor() const;
    virtual Color platformInactiveSelectionBackgroundColor() const;
    virtual Color platformActiveSelectionForegroundColor() const;
    virtual Color platformInactiveSelectionForegroundColor() const;

    virtual Color platformActiveListBoxSelectionBackgroundColor() const;
    virtual Color platformInactiveListBoxSelectionBackgroundColor() const;
    virtual Color platformActiveListBoxSelectionForegroundColor() const;
    virtual Color platformInactiveListBoxSelectionForegroundColor() const;

    virtual bool supportsSelectionForegroundColors() const { return true; }
    virtual bool supportsListBoxSelectionForegroundColors() const { return true; }

#if !USE(NEW_THEME)
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
#endif

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

    virtual bool paintMediaFullscreenButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaPlayButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaMuteButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaSeekBackButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaSeekForwardButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaSliderTrack(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaSliderThumb(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaTimelineContainer(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaCurrentTime(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaTimeRemaining(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return true; }

public:
    // Methods for state querying
    ControlStates controlStatesForRenderer(const RenderObject* o) const;
    bool isActive(const RenderObject*) const;
    bool isChecked(const RenderObject*) const;
    bool isIndeterminate(const RenderObject*) const;
    bool isEnabled(const RenderObject*) const;
    bool isFocused(const RenderObject*) const;
    bool isPressed(const RenderObject*) const;
    bool isHovered(const RenderObject*) const;
    bool isReadOnlyControl(const RenderObject*) const;
    bool isDefault(const RenderObject*) const;

private:
    mutable Color m_activeSelectionBackgroundColor;
    mutable Color m_inactiveSelectionBackgroundColor;
    mutable Color m_activeSelectionForegroundColor;
    mutable Color m_inactiveSelectionForegroundColor;

    mutable Color m_activeListBoxSelectionBackgroundColor;
    mutable Color m_inactiveListBoxSelectionBackgroundColor;
    mutable Color m_activeListBoxSelectionForegroundColor;
    mutable Color m_inactiveListBoxSelectionForegroundColor;

#if USE(NEW_THEME)
    Theme* m_theme; // The platform-specific theme.
#endif
};

// Function to obtain the theme.  This is implemented in your platform-specific theme implementation to hand
// back the appropriate platform theme.
RenderTheme* theme();

} // namespace WebCore

#endif // RenderTheme_h
