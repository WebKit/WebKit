/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007 Alp Toker <alp.toker@collabora.co.uk>
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "RenderThemeGdk.h"

#include "NotImplemented.h"
#include "RenderObject.h"

#include <gdk/gdk.h>

#define THEME_COLOR 204
#define THEME_FONT  210

// Button constants
#define BP_BUTTON    1
#define BP_RADIO     2
#define BP_CHECKBOX  3

// Textfield constants
#define TFP_TEXTFIELD 1
#define TFS_READONLY  6

/*
 * Approach to theming:
 *  a) keep one copy of each to be drawn widget, GtkEntry, GtkButton, Gtk...
 *     + the button will look like the native control
 *     + we don't need to worry about style updates and loading the right GtkStyle
 *     - resources are wasted. The native windows will not be used, we might have issues
 *       with
 *
 *  b) Use GtkStyle directly and copy and paste Gtk+ code
 *
 *
 * We will mix a and b
 *
 * - Create GtkWidgets to hold the state (disabled/enabled), selected, not selected.
 * - Use a GdkPixmap to make the GtkStyle draw to and then try to convert set it the
 *   source of the current operation.
 *
 */

namespace WebCore {

RenderTheme* theme()
{
    static RenderThemeGdk gdkTheme;
    return &gdkTheme;
}

RenderThemeGdk::RenderThemeGdk()
    : m_gtkButton(0)
    , m_gtkCheckbox(0)
    , m_gtkRadioButton(0)
    , m_gtkEntry(0)
    , m_gtkEditable(0)
    , m_unmappedWindow(0)
    , m_container(0)
{
}

void RenderThemeGdk::close()
{
}

void RenderThemeGdk::addIntrinsicMargins(RenderStyle* style) const
{
    // Cut out the intrinsic margins completely if we end up using a small font size
    if (style->fontSize() < 11)
        return;

    // Intrinsic margin value.
    const int m = 2;

    // FIXME: Using width/height alone and not also dealing with min-width/max-width is flawed.
    if (style->width().isIntrinsicOrAuto()) {
        if (style->marginLeft().quirk())
            style->setMarginLeft(Length(m, Fixed));
        if (style->marginRight().quirk())
            style->setMarginRight(Length(m, Fixed));
    }

    if (style->height().isAuto()) {
        if (style->marginTop().quirk())
            style->setMarginTop(Length(m, Fixed));
        if (style->marginBottom().quirk())
            style->setMarginBottom(Length(m, Fixed));
    }
}

bool RenderThemeGdk::supportsFocus(EAppearance appearance)
{
    switch (appearance) {
        case PushButtonAppearance:
        case ButtonAppearance:
        case TextFieldAppearance:
            return true;
        default:
            return false;
    }

    return false;
}

GtkStateType RenderThemeGdk::determineState(RenderObject* o)
{
    GtkStateType result = GTK_STATE_NORMAL;
    if (!isEnabled(o))
        result = GTK_STATE_INSENSITIVE;
    else if (isPressed(o))
        result = GTK_STATE_ACTIVE;
    else if (isHovered(o))
        result = GTK_STATE_PRELIGHT;
    return result;
}

ThemeData RenderThemeGdk::getThemeData(RenderObject* o)
{
    ThemeData result;
    switch (o->style()->appearance()) {
        case PushButtonAppearance:
        case ButtonAppearance:
            result.m_part = BP_BUTTON;
            result.m_state = determineState(o);
            break;
        case CheckboxAppearance:
            result.m_part = BP_CHECKBOX;
            result.m_state = determineState(o);
            break;
        case RadioAppearance:
            result.m_part = BP_RADIO;
            result.m_state = determineState(o);
            break;
        case TextFieldAppearance:
            result.m_part = TFP_TEXTFIELD;
            result.m_state = determineState(o);
            break;
        default:
            // FIXME: much more?
            break;
    }

    return result;
}

void RenderThemeGdk::setCheckboxSize(RenderStyle* style) const 
{ 
    setRadioSize(style);
}

bool RenderThemeGdk::paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    // FIXME: is it the right thing to do?
    GtkWidget *checkbox = gtkCheckbox();
    gtk_paint_check(checkbox->style, i.context->gdkDrawable(),
                    determineState(o), isChecked(o) ? GTK_SHADOW_IN : GTK_SHADOW_OUT,
                    NULL, checkbox, "checkbutton",
                    rect.x(), rect.y(), rect.width(), rect.height());

    return false;
}

void RenderThemeGdk::setRadioSize(RenderStyle* style) const 
{ 
    notImplemented(); 
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;


    // FIXME:  A hard-coded size of 13 is used.  This is wrong but necessary for now.  It matches Firefox.
    // At different DPI settings on Windows, querying the theme gives you a larger size that accounts for
    // the higher DPI.  Until our entire engine honors a DPI setting other than 96, we can't rely on the theme's
    // metrics.
    const int ff = 13;
    if (style->width().isIntrinsicOrAuto())
        style->setWidth(Length(ff, Fixed));

    if (style->height().isAuto())
        style->setHeight(Length(ff, Fixed));
}

bool RenderThemeGdk::paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{ 
    // FIXME: is it the right thing to do?
    GtkWidget *radio = gtkRadioButton();
    gtk_paint_option(radio->style, i.context->gdkDrawable(),
                     determineState(o), isChecked(o) ? GTK_SHADOW_IN : GTK_SHADOW_OUT,
                     NULL, radio, "radiobutton",
                     rect.x(), rect.y(), rect.width(), rect.height());

    return false;
}

bool RenderThemeGdk::paintButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect) 
{ 
    // FIXME: should use theme-aware drawing. This should honor the state as well
    GtkWidget *button = gtkButton();
    gtk_paint_box(button->style, i.context->gdkDrawable(),
                  determineState(o), isChecked(o) ? GTK_SHADOW_IN : GTK_SHADOW_OUT,
                  NULL, button, "button",
                  rect.x(), rect.y(), rect.width(), rect.height());
    return false;
}

void RenderThemeGdk::adjustTextFieldStyle(CSSStyleSelector*, RenderStyle*, Element* e) const 
{ 
    notImplemented(); 
}

bool RenderThemeGdk::paintTextField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    // FIXME: should use theme-aware drawing
    return true;
}

bool RenderThemeGdk::paintTextArea(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

void RenderThemeGdk::adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const
{
    addIntrinsicMargins(style);
}

void RenderThemeGdk::systemFont(int propId, FontDescription&) const
{
}

GtkWidget* RenderThemeGdk::gtkButton() const
{
    if (!m_gtkButton) {
        m_gtkButton = gtk_button_new();
        gtk_container_add(GTK_CONTAINER(gtkWindowContainer()), m_gtkButton);
        gtk_widget_realize(m_gtkButton);
    }

    return m_gtkButton;
}

GtkWidget* RenderThemeGdk::gtkCheckbox() const
{
    if (!m_gtkCheckbox) {
        m_gtkCheckbox = gtk_check_button_new();
        gtk_container_add(GTK_CONTAINER(gtkWindowContainer()), m_gtkCheckbox);
        gtk_widget_realize(m_gtkCheckbox);
    }

    return m_gtkCheckbox;
}

GtkWidget* RenderThemeGdk::gtkRadioButton() const
{
    if (!m_gtkRadioButton) {
        m_gtkRadioButton = gtk_radio_button_new(NULL);
        gtk_container_add(GTK_CONTAINER(gtkWindowContainer()), m_gtkRadioButton);
        gtk_widget_realize(m_gtkRadioButton);
    }

    return m_gtkRadioButton;
}

GtkWidget* RenderThemeGdk::gtkWindowContainer() const
{
    if (!m_container) {
        m_unmappedWindow = gtk_window_new(GTK_WINDOW_POPUP);
        m_container = gtk_fixed_new();
        gtk_container_add(GTK_CONTAINER(m_unmappedWindow), m_container);
        gtk_widget_realize(m_unmappedWindow);
    }

    return m_container;
}
}
