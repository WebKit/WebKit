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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef RenderThemeWin_H
#define RenderThemeWin_H

#include "RenderTheme.h"

#if WIN32
typedef void* HANDLE;
typedef struct HINSTANCE__* HINSTANCE;
typedef HINSTANCE HMODULE;
#endif

namespace WebCore {

struct ThemeData {
    ThemeData() :m_part(0), m_state(0) {}

    unsigned m_part;
    unsigned m_state;
};

class RenderThemeWin : public RenderTheme {
public:
    RenderThemeWin();
    ~RenderThemeWin();
       
    // A method asking if the theme's controls actually care about redrawing when hovered.
    virtual bool supportsHover(const RenderStyle* style) const { return true; }

    virtual bool paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
    { return paintButton(o, i, r); }
    virtual void setCheckboxSize(RenderStyle* style) const;

    virtual bool paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
    { return paintButton(o, i, r); }
    virtual void setRadioSize(RenderStyle* style) const;

    virtual void adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const;
    virtual bool paintButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r);

    virtual void adjustTextFieldStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const;
    virtual bool paintTextField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r);

private:
    void addIntrinsicMargins(RenderStyle* style) const;
    void close();

    unsigned determineState(RenderObject* o);
    bool supportsFocus(EAppearance appearance);

    ThemeData getThemeData(RenderObject* o);
    
    HMODULE m_themeDLL;
    mutable HANDLE m_buttonTheme;
    mutable HANDLE m_textFieldTheme;
};

};

#endif