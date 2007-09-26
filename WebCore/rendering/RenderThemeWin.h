/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
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

    unsigned m_part;
    unsigned m_state;
    unsigned m_classicState;
};

class RenderThemeWin : public RenderTheme {
public:
    RenderThemeWin();
    ~RenderThemeWin();
       
    // A method asking if the theme's controls actually care about redrawing when hovered.
    virtual bool supportsHover(const RenderStyle*) const { return true; }

    virtual Color platformActiveSelectionBackgroundColor() const;
    virtual Color platformInactiveSelectionBackgroundColor() const;
    virtual Color platformActiveSelectionForegroundColor() const;
    virtual Color platformInactiveSelectionForegroundColor() const;

    // System fonts.
    virtual void systemFont(int propId, FontDescription&) const;

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

    void adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const;
    virtual bool paintMenuList(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual bool paintMenuListButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

private:
    void addIntrinsicMargins(RenderStyle*) const;
    void close();

    unsigned determineState(RenderObject*);
    unsigned determineClassicState(RenderObject*);
    bool supportsFocus(EAppearance);

    ThemeData getThemeData(RenderObject*);
    
    HMODULE m_themeDLL;
    mutable HANDLE m_buttonTheme;
    mutable HANDLE m_textFieldTheme;
    mutable HANDLE m_menuListTheme;
};

};

#endif
