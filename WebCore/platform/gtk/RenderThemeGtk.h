/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * All rights reserved.
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

#ifndef RenderThemeGdk_h
#define RenderThemeGdk_h

#include "RenderTheme.h"
#include "GraphicsContext.h"

#include <gtk/gtk.h>

namespace WebCore {

struct ThemeData {
    ThemeData() : m_part(0), m_state(0) {}

    unsigned m_part;
    unsigned m_state;
};

class RenderThemeGtk : public RenderTheme {
public:
    RenderThemeGtk();

    // A method asking if the theme's controls actually care about redrawing when hovered.
    virtual bool supportsHover(const RenderStyle* style) const { return true; }

    // System fonts.
    virtual void systemFont(int propId, FontDescription&) const;

protected:
    virtual bool paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r);
    virtual void setCheckboxSize(RenderStyle* style) const;

    virtual bool paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r);
    virtual void setRadioSize(RenderStyle* style) const;

    virtual void adjustButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustTextFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintTextField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual bool paintTextArea(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

private:
    void addIntrinsicMargins(RenderStyle*) const;
    void close();

    GtkStateType determineState(RenderObject*);
    GtkShadowType determineShadow(RenderObject*);
    bool supportsFocus(EAppearance);

    ThemeData getThemeData(RenderObject*);


    /*
     * hold the state
     */
    GtkWidget* gtkButton() const;
    GtkWidget* gtkCheckbox() const;
    GtkWidget* gtkRadioButton() const;
    GtkWidget* gtkEntry() const;
    GtkWidget* gtkEditable() const;

    /*
     * unmapped GdkWindow having a container. This is holding all
     * our fake widgets
     */
    GtkWidget* gtkWindowContainer() const;

private:
    mutable GtkWidget* m_gtkButton;
    mutable GtkWidget* m_gtkCheckbox;
    mutable GtkWidget* m_gtkRadioButton;
    mutable GtkWidget* m_gtkEntry;
    mutable GtkWidget* m_gtkEditable;

    mutable GtkWidget* m_unmappedWindow;
    mutable GtkWidget* m_container;
};

}

#endif
