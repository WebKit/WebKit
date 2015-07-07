/*
 * Copyright (C) 2007, 2008, 2013, 2014 Apple Inc.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef RenderThemeSafari_h
#define RenderThemeSafari_h

#if USE(SAFARI_THEME)

#include "RenderTheme.h"

// If you have an empty placeholder SafariThemeConstants.h, then include SafariTheme.h
// This is a workaround until a version of WebKitSupportLibrary is released with an updated SafariThemeConstants.h 
#include <SafariTheme/SafariThemeConstants.h>
#ifndef SafariThemeConstants_h
#include <SafariTheme/SafariTheme.h>
#endif

#if PLATFORM(WIN)
typedef void* HANDLE;
typedef struct HINSTANCE__* HINSTANCE;
typedef HINSTANCE HMODULE;
#endif

namespace WebCore {

using namespace SafariTheme;

class RenderStyle;

class RenderThemeSafari : public RenderTheme {
public:
    static PassRefPtr<RenderTheme> create();

    // A method to obtain the baseline position for a "leaf" control.  This will only be used if a baseline
    // position cannot be determined by examining child content. Checkboxes and radio buttons are examples of
    // controls that need to do this.
    virtual int baselinePosition(const RenderObject&) const;

    // A method asking if the control changes its tint when the window has focus or not.
    virtual bool controlSupportsTints(const RenderObject&) const;

    // A general method asking if any control tinting is supported at all.
    virtual bool supportsControlTints() const { return true; }

    virtual void adjustRepaintRect(const RenderObject&, IntRect&);

    virtual bool isControlStyled(const RenderStyle&, const BorderData&,
                                 const FillLayer&, const Color& backgroundColor) const;

    virtual Color platformActiveSelectionBackgroundColor() const;
    virtual Color platformInactiveSelectionBackgroundColor() const;
    virtual Color activeListBoxSelectionBackgroundColor() const;

    virtual Color platformFocusRingColor() const;

    // System fonts.
    virtual void systemFont(CSSValueID, FontDescription&) const;

    virtual int minimumMenuListSize(RenderStyle&) const;

    virtual void adjustSliderThumbSize(RenderStyle&, Element&) const;
    virtual void adjustSliderThumbStyle(StyleResolver&, RenderStyle&, Element&) const; 
    
    virtual int popupInternalPaddingLeft(RenderStyle&) const;
    virtual int popupInternalPaddingRight(RenderStyle&) const;
    virtual int popupInternalPaddingTop(RenderStyle&) const;
    virtual int popupInternalPaddingBottom(RenderStyle&) const;

protected:
    // Methods for each appearance value.
    virtual bool paintCheckbox(const RenderObject&, const PaintInfo&, const IntRect&);
    virtual void setCheckboxSize(RenderStyle&) const;

    virtual bool paintRadio(const RenderObject&, const PaintInfo&, const IntRect&);
    virtual void setRadioSize(RenderStyle&) const;

    virtual void adjustButtonStyle(StyleResolver&, RenderStyle&, WebCore::Element&) const;
    virtual bool paintButton(const RenderObject&, const PaintInfo&, const IntRect&);
    virtual void setButtonSize(RenderStyle&) const;

    virtual bool paintTextField(const RenderObject&, const PaintInfo&, const IntRect&);
    virtual void adjustTextFieldStyle(StyleResolver&, RenderStyle&, Element&) const;

    virtual bool paintTextArea(const RenderObject&, const PaintInfo&, const IntRect&);
    virtual void adjustTextAreaStyle(StyleResolver&, RenderStyle&, Element&) const;

    virtual bool paintMenuList(const RenderObject&, const PaintInfo&, const IntRect&);
    virtual void adjustMenuListStyle(StyleResolver&, RenderStyle&, Element&) const;

    virtual bool paintMenuListButtonDecorations(const RenderObject&, const PaintInfo&, const FloatRect&);
    virtual void adjustMenuListButtonStyle(StyleResolver&, RenderStyle&, Element&) const;

    virtual bool paintSliderTrack(const RenderObject&, const PaintInfo&, const IntRect&);
    virtual bool paintSliderThumb(const RenderObject&, const PaintInfo&, const IntRect&);

    virtual bool paintSearchField(const RenderObject&, const PaintInfo&, const IntRect&);
    virtual void adjustSearchFieldStyle(StyleResolver&, RenderStyle&, Element&) const;

    virtual void adjustSearchFieldCancelButtonStyle(StyleResolver&, RenderStyle&, Element&) const;
    virtual bool paintSearchFieldCancelButton(const RenderObject&, const PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldDecorationPartStyle(StyleResolver&, RenderStyle&, Element&) const;
    virtual bool paintSearchFieldDecorationPart(const RenderObject&, const PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldResultsDecorationPartStyle(StyleResolver&, RenderStyle&, Element&) const;
    virtual bool paintSearchFieldResultsDecorationPart(const RenderObject&, const PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldResultsButtonStyle(StyleResolver&, RenderStyle&, Element&) const;
    virtual bool paintSearchFieldResultsButton(const RenderObject&, const PaintInfo&, const IntRect&);
 
    virtual bool paintCapsLockIndicator(const RenderObject&, const PaintInfo&, const IntRect&);

#if ENABLE(VIDEO)
    virtual String mediaControlsStyleSheet() override;
    virtual String mediaControlsScript() override;
#endif

#if ENABLE(METER_ELEMENT)
    virtual IntSize meterSizeForBounds(const RenderMeter&, const IntRect&) const override;
    virtual bool supportsMeter(ControlPart) const override;
    virtual void adjustMeterStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintMeter(const RenderObject&, const PaintInfo&, const IntRect&) override;
#endif

    virtual bool shouldShowPlaceholderWhenFocused() const { return true; }

private:
    RenderThemeSafari();
    virtual ~RenderThemeSafari();

    IntRect inflateRect(const IntRect&, const IntSize&, const int* margins) const;

    // Get the control size based off the font.  Used by some of the controls (like buttons).

    NSControlSize controlSizeForFont(RenderStyle&) const;
    NSControlSize controlSizeForSystemFont(RenderStyle&) const;
    //void setControlSize(NSCell*, const IntSize* sizes, const IntSize& minSize);
    void setSizeFromFont(RenderStyle&, const IntSize* sizes) const;
    IntSize sizeForFont(RenderStyle&, const IntSize* sizes) const;
    IntSize sizeForSystemFont(RenderStyle&, const IntSize* sizes) const;
    void setFontFromControlSize(StyleResolver&, RenderStyle&, NSControlSize) const;

    // Helpers for adjusting appearance and for painting
    const IntSize* checkboxSizes() const;
    const int* checkboxMargins(NSControlSize) const;

    const IntSize* radioSizes() const;
    const int* radioMargins(NSControlSize) const;

    void setButtonPaddingFromControlSize(RenderStyle&, NSControlSize) const;
    const IntSize* buttonSizes() const;
    const int* buttonMargins(NSControlSize) const;

    const IntSize* popupButtonSizes() const;
    const int* popupButtonMargins(NSControlSize) const;
    const int* popupButtonPadding(NSControlSize) const;
    void paintMenuListButtonGradients(const RenderObject&, const PaintInfo&, const IntRect&);
    const IntSize* menuListSizes() const;

    const IntSize* searchFieldSizes() const;
    const IntSize* cancelButtonSizes() const;
    const IntSize* resultsButtonSizes() const;
    void setSearchFieldSize(RenderStyle&) const;

    ThemeControlState determineState(const RenderObject&) const;

    String m_mediaControlsScript;
    String m_mediaControlsStyleSheet;
};

} // namespace WebCore

#endif // #if USE(SAFARI_THEME)

#endif // RenderThemeSafari_h
