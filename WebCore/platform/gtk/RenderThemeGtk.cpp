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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderThemeGtk.h"

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
    static RenderThemeGtk gdkTheme;
    return &gdkTheme;
}

RenderThemeGtk::RenderThemeGtk()
    : m_gtkButton(0)
    , m_gtkCheckbox(0)
    , m_gtkRadioButton(0)
    , m_gtkEntry(0)
    , m_gtkEditable(0)
    , m_gtkTreeView(0)
    , m_unmappedWindow(0)
    , m_container(0)
{
}

void RenderThemeGtk::close()
{
}

void RenderThemeGtk::addIntrinsicMargins(RenderStyle* style) const
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

bool RenderThemeGtk::supportsFocus(EAppearance appearance) const
{
    switch (appearance) {
        case PushButtonAppearance:
        case ButtonAppearance:
        case TextFieldAppearance:
        case TextAreaAppearance:
        case MenulistAppearance:
        case RadioAppearance:
        case CheckboxAppearance:
            return true;
        default:
            return false;
    }
}

bool RenderThemeGtk::supportsFocusRing(const RenderStyle* style) const 
{
    return supportsFocus(style->appearance());
}

bool RenderThemeGtk::controlSupportsTints(const RenderObject* o) const
{
    if (!isEnabled(o))
        return false;

    // Checkboxes have tints when enabled.
    if (o->style()->appearance() == CheckboxAppearance)
        return isChecked(o);

    return true;
}

short RenderThemeGtk::baselinePosition(const RenderObject* o) const 
{
    if (o->style()->appearance() == CheckboxAppearance ||
        o->style()->appearance() == RadioAppearance)
        return o->marginTop() + o->height() - 2; // Same as in old khtml
    return RenderTheme::baselinePosition(o);
}
    
GtkStateType RenderThemeGtk::determineState(RenderObject* o)
{
    if (!isEnabled(o) || isReadOnlyControl(o))
        return GTK_STATE_INSENSITIVE;
    if (isPressed(o) || isFocused(o))
        return GTK_STATE_ACTIVE;
    if (isHovered(o))
        return GTK_STATE_PRELIGHT;
    if (isChecked(o))
        return GTK_STATE_SELECTED;

    return GTK_STATE_NORMAL;
}

GtkShadowType RenderThemeGtk::determineShadow(RenderObject* o)
{
    return isChecked(o) ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
}

ThemeData RenderThemeGtk::getThemeData(RenderObject* o)
{
    ThemeData result;
    switch (o->style()->appearance()) {
        case PushButtonAppearance:
        case ButtonAppearance:
            result.m_part = BP_BUTTON;
            break;
        case CheckboxAppearance:
            result.m_part = BP_CHECKBOX;
            break;
        case RadioAppearance:
            result.m_part = BP_RADIO;
            break;
        case TextFieldAppearance:
        case ListboxAppearance:
        case MenulistAppearance:
        case TextAreaAppearance:
            result.m_part = TFP_TEXTFIELD;
            break;
        default:
            break; // FIXME: much more?
    }

    result.m_state = determineState(o);

    return result;
}

void RenderThemeGtk::setCheckboxSize(RenderStyle* style) const 
{ 
    setRadioSize(style);
}

bool RenderThemeGtk::paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    paintButton(o, i, rect);    
    return false;
}

bool RenderThemeGtk::paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    paintButton(o, i, rect);
    return false;
}

void RenderThemeGtk::setRadioSize(RenderStyle* style) const 
{ 
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

bool RenderThemeGtk::paintButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect) 
{
    EAppearance appearance = o->style()->appearance();

    if (appearance == PushButtonAppearance || appearance == ButtonAppearance) {
        GtkWidget* button = gtkButton();
        IntPoint pos = i.context->translatePoint(rect.location());
        gtk_paint_box(button->style, i.context->gdkDrawable(),
                      determineState(o), determineShadow(o),
                      0, button, "button",
                      pos.x(), pos.y(), rect.width(), rect.height());
    } else if (appearance == CheckboxAppearance) {
        GtkWidget* checkbox = gtkCheckbox();
        IntPoint pos = i.context->translatePoint(rect.location());
        gtk_paint_check(checkbox->style, i.context->gdkDrawable(),
                        determineState(o), determineShadow(o),
                        0, checkbox, "checkbutton",
                        pos.x(), pos.y(), rect.width(), rect.height());
    } else if (appearance == RadioAppearance) {
        GtkWidget* radio = gtkRadioButton();
        IntPoint pos = i.context->translatePoint(rect.location());
        gtk_paint_option(radio->style, i.context->gdkDrawable(),
                         determineState(o), determineShadow(o),
                         0, radio, "radiobutton",
                         pos.x(), pos.y(), rect.width(), rect.height());
    }

    return false;
}

void RenderThemeGtk::adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeGtk::paintMenuList(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    // TODO: Render a real menu list button, not just a box
    GtkWidget* button = gtkButton();
    IntPoint pos = i.context->translatePoint(rect.location());
    gtk_paint_box(button->style, i.context->gdkDrawable(),
                  determineState(o), determineShadow(o),
                  0, button, 0,
                  pos.x(), pos.y(), rect.width(), rect.height());
    return false;
}

void RenderThemeGtk::adjustTextFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const 
{ 
    addIntrinsicMargins(style);
}

bool RenderThemeGtk::paintTextField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    GtkWidget* entry = gtkEntry();
    IntPoint pos = i.context->translatePoint(rect.location());

    gtk_paint_shadow(entry->style, i.context->gdkDrawable(),
                     determineState(o), determineShadow(o),
                     0, entry, "entry",
                     pos.x(), pos.y(), rect.width(), rect.height());
    return false;
}

bool RenderThemeGtk::paintTextArea(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

void RenderThemeGtk::adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const
{
    addIntrinsicMargins(style);
}

Color RenderThemeGtk::platformActiveSelectionBackgroundColor() const
{
    GtkWidget* entry = gtkEntry();
    GdkColor color = entry->style->base[GTK_STATE_SELECTED];
    return Color(color.red >> 8, color.green >> 8, color.blue >> 8);
}

Color RenderThemeGtk::platformInactiveSelectionBackgroundColor() const
{
    GtkWidget* entry = gtkEntry();
    GdkColor color = entry->style->base[GTK_STATE_ACTIVE];
    return Color(color.red >> 8, color.green >> 8, color.blue >> 8);
}

Color RenderThemeGtk::platformActiveSelectionForegroundColor() const
{
    GtkWidget* entry = gtkEntry();
    GdkColor color = entry->style->text[GTK_STATE_SELECTED];
    return Color(color.red >> 8, color.green >> 8, color.blue >> 8);
}

Color RenderThemeGtk::platformInactiveSelectionForegroundColor() const
{
    GtkWidget* entry = gtkEntry();
    GdkColor color = entry->style->text[GTK_STATE_ACTIVE];
    return Color(color.red >> 8, color.green >> 8, color.blue >> 8);
}

Color RenderThemeGtk::activeListBoxSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    GdkColor color = widget->style->base[GTK_STATE_SELECTED];
    return Color(color.red >> 8, color.green >> 8, color.blue >> 8);
}

Color RenderThemeGtk::inactiveListBoxSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    GdkColor color = widget->style->base[GTK_STATE_ACTIVE];
    return Color(color.red >> 8, color.green >> 8, color.blue >> 8);
}

Color RenderThemeGtk::activeListBoxSelectionForegroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    GdkColor color = widget->style->text[GTK_STATE_SELECTED];
    return Color(color.red >> 8, color.green >> 8, color.blue >> 8);
}

Color RenderThemeGtk::inactiveListBoxSelectionForegroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    GdkColor color = widget->style->text[GTK_STATE_ACTIVE];
    return Color(color.red >> 8, color.green >> 8, color.blue >> 8);
}

bool RenderThemeGtk::caretShouldBlink() const
{
    GtkSettings* settings = gtk_widget_get_settings(gtkEntry());

    gboolean value;
    g_object_get(settings, "gtk-cursor-blink", &value, 0);

    return value;
}

double RenderThemeGtk::caretBlinkFrequency() const
{
    if (!caretShouldBlink())
        return 0;

    GtkSettings* settings = gtk_widget_get_settings(gtkEntry());

    gint time;
    g_object_get(settings, "gtk-cursor-blink-time", &time, 0);

    return time / 2000.;
}

void RenderThemeGtk::systemFont(int propId, FontDescription&) const
{
}

GtkWidget* RenderThemeGtk::gtkButton() const
{
    if (!m_gtkButton) {
        m_gtkButton = gtk_button_new();
        gtk_container_add(GTK_CONTAINER(gtkWindowContainer()), m_gtkButton);
        gtk_widget_realize(m_gtkButton);
    }

    return m_gtkButton;
}

GtkWidget* RenderThemeGtk::gtkCheckbox() const
{
    if (!m_gtkCheckbox) {
        m_gtkCheckbox = gtk_check_button_new();
        gtk_container_add(GTK_CONTAINER(gtkWindowContainer()), m_gtkCheckbox);
        gtk_widget_realize(m_gtkCheckbox);
    }

    return m_gtkCheckbox;
}

GtkWidget* RenderThemeGtk::gtkRadioButton() const
{
    if (!m_gtkRadioButton) {
        m_gtkRadioButton = gtk_radio_button_new(NULL);
        gtk_container_add(GTK_CONTAINER(gtkWindowContainer()), m_gtkRadioButton);
        gtk_widget_realize(m_gtkRadioButton);
    }

    return m_gtkRadioButton;
}

void RenderThemeGtk::gtkStyleSet(GtkWidget* widget, GtkStyle* previous, RenderTheme* renderTheme)
{
    renderTheme->platformColorsDidChange();
}

GtkWidget* RenderThemeGtk::gtkEntry() const
{
    if (!m_gtkEntry) {
        m_gtkEntry = gtk_entry_new();
        g_signal_connect(m_gtkEntry, "style-set", G_CALLBACK(gtkStyleSet), theme());
        gtk_container_add(GTK_CONTAINER(gtkWindowContainer()), m_gtkEntry);
        gtk_widget_realize(m_gtkEntry);
    }

    return m_gtkEntry;
}

GtkWidget* RenderThemeGtk::gtkTreeView() const
{
    if (!m_gtkTreeView) {
        m_gtkTreeView = gtk_tree_view_new();
        g_signal_connect(m_gtkTreeView, "style-set", G_CALLBACK(gtkStyleSet), theme());
        gtk_container_add(GTK_CONTAINER(gtkWindowContainer()), m_gtkTreeView);
        gtk_widget_realize(m_gtkTreeView);
    }

    return m_gtkTreeView;
}

GtkWidget* RenderThemeGtk::gtkWindowContainer() const
{
    if (!m_container) {
        m_unmappedWindow = gtk_window_new(GTK_WINDOW_POPUP);
        // Some GTK themes (i.e. Clearlooks) draw the buttons differently
        // (in particular, call gtk_style_apply_default_background) if they
        // are unallocated and are children of a GtkFixed widget, which is
        // apparently what some "make Firefox buttons look like GTK" code
        // does.  To avoid this ugly look, we use a GtkHBox as a parent,
        // rather than a GtkFixed.
        m_container = gtk_hbox_new(false, 0);
        gtk_container_add(GTK_CONTAINER(m_unmappedWindow), m_container);
        gtk_widget_realize(m_unmappedWindow);
    }

    return m_container;
}
}
