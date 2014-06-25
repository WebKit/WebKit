/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006, 2008, 2013, 2014 Apple Inc.
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

#ifndef RenderThemeWin_h
#define RenderThemeWin_h

#include "RenderTheme.h"

#if WIN32
typedef void* HANDLE;
typedef struct HINSTANCE__* HINSTANCE;
typedef HINSTANCE HMODULE;
#endif

namespace WebCore {

struct ThemeData {
    ThemeData() :m_part(0), m_state(0), m_classicState(0) {}
    ThemeData(int part, int state)
        : m_part(part)
        , m_state(state)
        , m_classicState(0)
    { }

    unsigned m_part;
    unsigned m_state;
    unsigned m_classicState;
};

class RenderThemeWin : public RenderTheme {
public:
    static PassRefPtr<RenderTheme> create();

    virtual String extraDefaultStyleSheet();
    virtual String extraQuirksStyleSheet();

    // A method asking if the theme's controls actually care about redrawing when hovered.
    virtual bool supportsHover(const RenderStyle&) const override;

    virtual Color platformActiveSelectionBackgroundColor() const override;
    virtual Color platformInactiveSelectionBackgroundColor() const override;
    virtual Color platformActiveSelectionForegroundColor() const override;
    virtual Color platformInactiveSelectionForegroundColor() const override;

    // System fonts.
    virtual void systemFont(CSSValueID, FontDescription&) const override;
    virtual Color systemColor(CSSValueID) const override;

    virtual bool paintCheckbox(const RenderObject& o, const PaintInfo& i, const IntRect& r) override
    { return paintButton(o, i, r); }
    virtual void setCheckboxSize(RenderStyle&) const override;

    virtual bool paintRadio(const RenderObject& o, const PaintInfo& i, const IntRect& r) override
    { return paintButton(o, i, r); }
    virtual void setRadioSize(RenderStyle& style) const override
    { return setCheckboxSize(style); }

    virtual bool paintButton(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual void adjustInnerSpinButtonStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintInnerSpinButton(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual bool paintTextField(const RenderObject&, const PaintInfo&, const FloatRect&) override;

    virtual bool paintTextArea(const RenderObject& o, const PaintInfo& i, const FloatRect& r) override
    { return paintTextField(o, i, r); }

    virtual void adjustMenuListStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintMenuList(const RenderObject&, const PaintInfo&, const FloatRect&) override;
    virtual void adjustMenuListButtonStyle(StyleResolver&, RenderStyle&, Element&) const override;

    virtual bool paintMenuListButtonDecorations(const RenderObject&, const PaintInfo&, const FloatRect&) override;

    virtual bool paintSliderTrack(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual bool paintSliderThumb(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual void adjustSliderThumbSize(RenderStyle&, Element&) const override;

    virtual bool popupOptionSupportsTextIndent() const override { return true; }

    virtual void adjustSearchFieldStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintSearchField(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual void adjustSearchFieldCancelButtonStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintSearchFieldCancelButton(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual void adjustSearchFieldDecorationPartStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintSearchFieldDecorationPart(const RenderObject&, const PaintInfo&, const IntRect&) override { return false; }

    virtual void adjustSearchFieldResultsDecorationPartStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintSearchFieldResultsDecorationPart(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual void adjustSearchFieldResultsButtonStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintSearchFieldResultsButton(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual void themeChanged() override;

    virtual void adjustButtonStyle(StyleResolver&, RenderStyle& style, Element&) const override { }
    virtual void adjustTextFieldStyle(StyleResolver&, RenderStyle& style, Element&) const override { }
    virtual void adjustTextAreaStyle(StyleResolver&, RenderStyle& style, Element&) const override { }

    static void setWebKitIsBeingUnloaded();

    static String stringWithContentsOfFile(CFStringRef name, CFStringRef type);

    virtual bool supportsFocusRing(const RenderStyle&) const override;

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

    virtual bool shouldShowPlaceholderWhenFocused() const override { return true; }

private:
    enum ControlSubPart {
        None,
        SpinButtonDown,
        SpinButtonUp,
    };

    RenderThemeWin();
    virtual ~RenderThemeWin();

    void addIntrinsicMargins(RenderStyle&) const;
    void close();

    unsigned determineState(const RenderObject&);
    unsigned determineClassicState(const RenderObject&, ControlSubPart = None);
    unsigned determineSliderThumbState(const RenderObject&);
    unsigned determineButtonState(const RenderObject&);
    unsigned determineSpinButtonState(const RenderObject&, ControlSubPart = None);

    bool supportsFocus(ControlPart) const;

    ThemeData getThemeData(const RenderObject&, ControlSubPart = None);
    ThemeData getClassicThemeData(const RenderObject&, ControlSubPart = None);

    HANDLE buttonTheme() const;
    HANDLE textFieldTheme() const;
    HANDLE menuListTheme() const;
    HANDLE sliderTheme() const;
    HANDLE spinButtonTheme() const;
    HANDLE progressBarTheme() const;

    mutable HANDLE m_buttonTheme;
    mutable HANDLE m_textFieldTheme;
    mutable HANDLE m_menuListTheme;
    mutable HANDLE m_sliderTheme;
    mutable HANDLE m_spinButtonTheme;
    mutable HANDLE m_progressBarTheme;

    String m_mediaControlsScript;
    String m_mediaControlsStyleSheet;
};

};

#endif
