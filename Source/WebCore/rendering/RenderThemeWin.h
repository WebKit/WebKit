/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
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

#pragma once

#include "RenderTheme.h"

#ifdef WIN32
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

class RenderThemeWin final : public RenderTheme {
public:
    friend NeverDestroyed<RenderThemeWin>;

    String extraDefaultStyleSheet() override;
    String extraQuirksStyleSheet() override;

    // A method asking if the theme's controls actually care about redrawing when hovered.
    bool supportsHover(const RenderStyle&) const override;

    Color platformActiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const override;
    Color platformInactiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const override;
    Color platformActiveSelectionForegroundColor(OptionSet<StyleColor::Options>) const override;
    Color platformInactiveSelectionForegroundColor(OptionSet<StyleColor::Options>) const override;

    Color systemColor(CSSValueID, OptionSet<StyleColor::Options>) const override;

    bool paintCheckbox(const RenderObject& o, const PaintInfo& i, const IntRect& r) override
    { return paintButton(o, i, r); }
    void setCheckboxSize(RenderStyle&) const override;

    bool paintRadio(const RenderObject& o, const PaintInfo& i, const IntRect& r) override
    { return paintButton(o, i, r); }
    void setRadioSize(RenderStyle& style) const override
    { return setCheckboxSize(style); }

    bool paintButton(const RenderObject&, const PaintInfo&, const IntRect&) override;

    void adjustInnerSpinButtonStyle(RenderStyle&, const Element*) const override;
    bool paintInnerSpinButton(const RenderObject&, const PaintInfo&, const IntRect&) override;

    bool paintTextField(const RenderObject&, const PaintInfo&, const FloatRect&) override;

    bool paintTextArea(const RenderObject& o, const PaintInfo& i, const FloatRect& r) override
    { return paintTextField(o, i, r); }

    void adjustMenuListStyle(RenderStyle&, const Element*) const override;
    bool paintMenuList(const RenderObject&, const PaintInfo&, const FloatRect&) override;
    void adjustMenuListButtonStyle(RenderStyle&, const Element*) const override;

    bool paintMenuListButtonDecorations(const RenderBox&, const PaintInfo&, const FloatRect&) override;

    bool paintSliderTrack(const RenderObject&, const PaintInfo&, const IntRect&) override;
    bool paintSliderThumb(const RenderObject&, const PaintInfo&, const IntRect&) override;
    void adjustSliderThumbSize(RenderStyle&, const Element*) const override;

    bool popupOptionSupportsTextIndent() const override { return true; }

    void adjustSearchFieldStyle(RenderStyle&, const Element*) const override;
    bool paintSearchField(const RenderObject&, const PaintInfo&, const IntRect&) override;

    void adjustSearchFieldCancelButtonStyle(RenderStyle&, const Element*) const override;
    bool paintSearchFieldCancelButton(const RenderBox&, const PaintInfo&, const IntRect&) override;

    void adjustSearchFieldDecorationPartStyle(RenderStyle&, const Element*) const override;
    bool paintSearchFieldDecorationPart(const RenderObject&, const PaintInfo&, const IntRect&) override { return false; }

    void adjustSearchFieldResultsDecorationPartStyle(RenderStyle&, const Element*) const override;
    bool paintSearchFieldResultsDecorationPart(const RenderBox&, const PaintInfo&, const IntRect&) override;

    void adjustSearchFieldResultsButtonStyle(RenderStyle&, const Element*) const override;
    bool paintSearchFieldResultsButton(const RenderBox&, const PaintInfo&, const IntRect&) override;

    void themeChanged() override;

    void adjustButtonStyle(RenderStyle& style, const Element*) const override { }
    void adjustTextFieldStyle(RenderStyle& style, const Element*) const override { }
    void adjustTextAreaStyle(RenderStyle& style, const Element*) const override { }

    static void setWebKitIsBeingUnloaded();

    static String stringWithContentsOfFile(const String& name, const String& type);

    bool supportsFocusRing(const RenderStyle&) const override;

#if ENABLE(VIDEO)
    String mediaControlsStyleSheet() override;
    String mediaControlsScript() override;
#endif

    IntSize meterSizeForBounds(const RenderMeter&, const IntRect&) const override;
    bool supportsMeter(ControlPart) const override;
    void adjustMeterStyle(RenderStyle&, const Element*) const override;
    bool paintMeter(const RenderObject&, const PaintInfo&, const IntRect&) override;

private:
    enum ControlSubPart {
        None,
        SpinButtonDown,
        SpinButtonUp,
    };

    RenderThemeWin();
    virtual ~RenderThemeWin();

    bool canPaint(const PaintInfo&) const final { return true; }

    // System fonts.
    void updateCachedSystemFontDescription(CSSValueID, FontCascadeDescription&) const override;

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

} // namespace WebCore
