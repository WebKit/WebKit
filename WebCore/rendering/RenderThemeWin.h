/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006, 2008 Apple Computer, Inc.
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
    RenderThemeWin();
    ~RenderThemeWin();

    virtual String extraDefaultStyleSheet();
    virtual String extraQuirksStyleSheet();

    // A method asking if the theme's controls actually care about redrawing when hovered.
    virtual bool supportsHover(const RenderStyle*) const;

    virtual Color platformActiveSelectionBackgroundColor() const;
    virtual Color platformInactiveSelectionBackgroundColor() const;
    virtual Color platformActiveSelectionForegroundColor() const;
    virtual Color platformInactiveSelectionForegroundColor() const;

    // System fonts.
    virtual void systemFont(int propId, FontDescription&) const;
    virtual Color systemColor(int cssValueId) const;

    virtual bool paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
    { return paintButton(o, i, r); }
    virtual void setCheckboxSize(RenderStyle*) const;

    virtual bool paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
    { return paintButton(o, i, r); }
    virtual void setRadioSize(RenderStyle* style) const
    { return setCheckboxSize(style); }

    virtual bool paintButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual bool paintTextField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual bool paintTextArea(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
    { return paintTextField(o, i, r); }

    virtual void adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const;
    virtual bool paintMenuList(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    virtual void adjustMenuListButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const;

    virtual bool paintMenuListButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual bool paintSliderTrack(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r);
    virtual bool paintSliderThumb(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r);
    virtual void adjustSliderThumbSize(RenderObject*) const;

    virtual bool popupOptionSupportsTextIndent() const { return true; }

    virtual int buttonInternalPaddingLeft() const;
    virtual int buttonInternalPaddingRight() const;
    virtual int buttonInternalPaddingTop() const;
    virtual int buttonInternalPaddingBottom() const;

    virtual void adjustSearchFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldCancelButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldCancelButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldDecorationStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldDecoration(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return false; }

    virtual void adjustSearchFieldResultsDecorationStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldResultsDecoration(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldResultsButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldResultsButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void themeChanged();

    virtual void adjustButtonStyle(CSSStyleSelector*, RenderStyle* style, Element*) const {}
    virtual void adjustTextFieldStyle(CSSStyleSelector*, RenderStyle* style, Element*) const {}
    virtual void adjustTextAreaStyle(CSSStyleSelector*, RenderStyle* style, Element*) const {}

    static void setWebKitIsBeingUnloaded();

    virtual bool supportsFocusRing(const RenderStyle*) const;

#if ENABLE(VIDEO)
    virtual bool paintMediaFullscreenButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    virtual bool paintMediaPlayButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    virtual bool paintMediaMuteButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    virtual bool paintMediaSeekBackButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    virtual bool paintMediaSeekForwardButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    virtual bool paintMediaSliderTrack(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    virtual bool paintMediaSliderThumb(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
#endif

private:
    void addIntrinsicMargins(RenderStyle*) const;
    void close();

    unsigned determineState(RenderObject*);
    unsigned determineClassicState(RenderObject*);
    unsigned determineSliderThumbState(RenderObject*);
    unsigned determineButtonState(RenderObject*);

    bool supportsFocus(ControlPart) const;

    ThemeData getThemeData(RenderObject*);
    ThemeData getClassicThemeData(RenderObject* o);

    HANDLE buttonTheme() const;
    HANDLE textFieldTheme() const;
    HANDLE menuListTheme() const;
    HANDLE sliderTheme() const;

    mutable HANDLE m_buttonTheme;
    mutable HANDLE m_textFieldTheme;
    mutable HANDLE m_menuListTheme;
    mutable HANDLE m_sliderTheme;
};

};

#endif
