/*
 * Copyright (C) 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2009 Kenneth Rohde Christiansen
 * Copyright (C) 2010 Igalia S.L.
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

#ifdef GTK_API_VERSION_2

#include "CSSValueKeywords.h"
#include "GraphicsContext.h"
#include "GtkVersioning.h"
#include "HTMLNames.h"
#include "MediaControlElements.h"
#include "PaintInfo.h"
#include "RenderObject.h"
#include "TextDirection.h"
#include "UserAgentStyleSheets.h"
#include "WidgetRenderingContext.h"
#include "gtkdrawing.h"
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#if ENABLE(PROGRESS_TAG)
#include "RenderProgress.h"
#endif

namespace WebCore {

// This is not a static method, because we want to avoid having GTK+ headers in RenderThemeGtk.h.
extern GtkTextDirection gtkTextDirection(TextDirection);

static int mozGtkRefCount = 0;
void RenderThemeGtk::platformInit()
{
    m_themePartsHaveRGBAColormap = true;
    m_gtkWindow = 0;
    m_gtkContainer = 0;
    m_gtkButton = 0;
    m_gtkEntry = 0;
    m_gtkTreeView = 0;
    m_gtkVScale = 0;
    m_gtkHScale = 0;

    memset(&m_themeParts, 0, sizeof(GtkThemeParts));
    GdkColormap* colormap = gdk_screen_get_rgba_colormap(gdk_screen_get_default());
    if (!colormap) {
        m_themePartsHaveRGBAColormap = false;
        colormap = gdk_screen_get_default_colormap(gdk_screen_get_default());
    }
    m_themeParts.colormap = colormap;

    // Initialize the Mozilla theme drawing code.
    if (!mozGtkRefCount) {
        moz_gtk_init();
        moz_gtk_use_theme_parts(&m_themeParts);
    }
    ++mozGtkRefCount;
}

RenderThemeGtk::~RenderThemeGtk()
{
    --mozGtkRefCount;

    if (!mozGtkRefCount)
        moz_gtk_shutdown();

    if (m_gtkWindow)
        gtk_widget_destroy(m_gtkWindow);
}

#if ENABLE(VIDEO)
void RenderThemeGtk::initMediaColors()
{
    GtkStyle* style = gtk_widget_get_style(GTK_WIDGET(gtkContainer()));
    m_panelColor = style->bg[GTK_STATE_NORMAL];
    m_sliderColor = style->bg[GTK_STATE_ACTIVE];
    m_sliderThumbColor = style->bg[GTK_STATE_SELECTED];
}
#endif

void RenderThemeGtk::adjustRepaintRect(const RenderObject*, IntRect&)
{
}

static GtkStateType getGtkStateType(RenderThemeGtk* theme, RenderObject* object)
{
    if (!theme->isEnabled(object) || theme->isReadOnlyControl(object))
        return GTK_STATE_INSENSITIVE;
    if (theme->isPressed(object))
        return GTK_STATE_ACTIVE;
    if (theme->isHovered(object))
        return GTK_STATE_PRELIGHT;
    return GTK_STATE_NORMAL;
}

bool RenderThemeGtk::paintRenderObject(GtkThemeWidgetType type, RenderObject* renderObject, GraphicsContext* context, const IntRect& rect, int flags)
{
    // Painting is disabled so just claim to have succeeded
    if (context->paintingDisabled())
        return false;

    GtkWidgetState widgetState;
    widgetState.active = isPressed(renderObject);
    widgetState.focused = isFocused(renderObject);

    // https://bugs.webkit.org/show_bug.cgi?id=18364
    // The Mozilla theme drawing code, only paints a button as pressed when it's pressed 
    // while hovered. Until we move away from the Mozila code, work-around the issue by
    // forcing a pressed button into the hovered state. This ensures that buttons activated
    // via the keyboard have the proper rendering.
    widgetState.inHover = isHovered(renderObject) || (type == MOZ_GTK_BUTTON && isPressed(renderObject));

    // FIXME: Disabled does not always give the correct appearance for ReadOnly
    widgetState.disabled = !isEnabled(renderObject) || isReadOnlyControl(renderObject);
    widgetState.isDefault = false;
    widgetState.canDefault = false;
    widgetState.depressed = false;

    WidgetRenderingContext widgetContext(context, rect);
    return !widgetContext.paintMozillaWidget(type, &widgetState, flags,
                                             gtkTextDirection(renderObject->style()->direction()));
}

void RenderThemeGtk::getIndicatorMetrics(ControlPart part, int& indicatorSize, int& indicatorSpacing)
{
    ASSERT(part == CheckboxPart || part == RadioPart);
    if (part == CheckboxPart) {
        moz_gtk_checkbox_get_metrics(&indicatorSize, &indicatorSpacing);
        return;
    }

    // RadioPart
    moz_gtk_radio_get_metrics(&indicatorSize, &indicatorSpacing);
}

static void setToggleSize(const RenderThemeGtk* theme, RenderStyle* style, ControlPart appearance)
{
    // The width and height are both specified, so we shouldn't change them.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    // FIXME: This is probably not correct use of indicatorSize and indicatorSpacing.
    gint indicatorSize, indicatorSpacing;
    RenderThemeGtk::getIndicatorMetrics(appearance, indicatorSize, indicatorSpacing);

    // Other ports hard-code this to 13, but GTK+ users tend to demand the native look.
    // It could be made a configuration option values other than 13 actually break site compatibility.
    int length = indicatorSize + indicatorSpacing;
    if (style->width().isIntrinsicOrAuto())
        style->setWidth(Length(length, Fixed));

    if (style->height().isAuto())
        style->setHeight(Length(length, Fixed));
}

void RenderThemeGtk::setCheckboxSize(RenderStyle* style) const
{
    setToggleSize(this, style, RadioPart);
}

bool RenderThemeGtk::paintCheckbox(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    return paintRenderObject(MOZ_GTK_CHECKBUTTON, object, info.context, rect, isChecked(object));
}

void RenderThemeGtk::setRadioSize(RenderStyle* style) const
{
    setToggleSize(this, style, RadioPart);
}

bool RenderThemeGtk::paintRadio(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    return paintRenderObject(MOZ_GTK_RADIOBUTTON, object, info.context, rect, isChecked(object));
}

bool RenderThemeGtk::paintButton(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    if (info.context->paintingDisabled())
        return false;

    GtkWidget* widget = gtkButton();
    IntRect buttonRect(IntPoint(), rect.size());
    IntRect focusRect(buttonRect);

    GtkStateType state = getGtkStateType(this, object);
    gtk_widget_set_state(widget, state);
    gtk_widget_set_direction(widget, gtkTextDirection(object->style()->direction()));

    if (isFocused(object)) {
        if (isEnabled(object)) {
#if !GTK_CHECK_VERSION(2, 22, 0)
            GTK_WIDGET_SET_FLAGS(widget, GTK_HAS_FOCUS);
#endif
            g_object_set(widget, "has-focus", TRUE, NULL);
        }

        gboolean interiorFocus = 0, focusWidth = 0, focusPadding = 0;
        gtk_widget_style_get(widget,
                             "interior-focus", &interiorFocus,
                             "focus-line-width", &focusWidth,
                             "focus-padding", &focusPadding, NULL);
        // If we are using exterior focus, we shrink the button rect down before
        // drawing. If we are using interior focus we shrink the focus rect. This
        // approach originates from the Mozilla theme drawing code (gtk2drawing.c).
        if (interiorFocus) {
            GtkStyle* style = gtk_widget_get_style(widget);
            focusRect.inflateX(-style->xthickness - focusPadding);
            focusRect.inflateY(-style->ythickness - focusPadding);
        } else {
            buttonRect.inflateX(-focusWidth - focusPadding);
            buttonRect.inflateY(-focusPadding - focusPadding);
        }
    }

    WidgetRenderingContext widgetContext(info.context, rect);
    GtkShadowType shadowType = state == GTK_STATE_ACTIVE ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
    widgetContext.gtkPaintBox(buttonRect, widget, state, shadowType, "button");
    if (isFocused(object))
        widgetContext.gtkPaintFocus(focusRect, widget, state, "button");

#if !GTK_CHECK_VERSION(2, 22, 0)
    GTK_WIDGET_UNSET_FLAGS(widget, GTK_HAS_FOCUS);
#endif
    g_object_set(widget, "has-focus", FALSE, NULL);
    return false;
}

static void getComboBoxPadding(RenderStyle* style, int& left, int& top, int& right, int& bottom)
{
    // If this menu list button isn't drawn using the native theme, we
    // don't add any extra padding beyond what WebCore already uses.
    if (style->appearance() == NoControlPart)
        return;
    moz_gtk_get_widget_border(MOZ_GTK_DROPDOWN, &left, &top, &right, &bottom,
                              gtkTextDirection(style->direction()), TRUE);
}

int RenderThemeGtk::popupInternalPaddingLeft(RenderStyle* style) const
{
    int left = 0, top = 0, right = 0, bottom = 0;
    getComboBoxPadding(style, left, top, right, bottom);
    return left;
}

int RenderThemeGtk::popupInternalPaddingRight(RenderStyle* style) const
{
    int left = 0, top = 0, right = 0, bottom = 0;
    getComboBoxPadding(style, left, top, right, bottom);
    return right;
}

int RenderThemeGtk::popupInternalPaddingTop(RenderStyle* style) const
{
    int left = 0, top = 0, right = 0, bottom = 0;
    getComboBoxPadding(style, left, top, right, bottom);
    return top;
}

int RenderThemeGtk::popupInternalPaddingBottom(RenderStyle* style) const
{
    int left = 0, top = 0, right = 0, bottom = 0;
    getComboBoxPadding(style, left, top, right, bottom);
    return bottom;
}

bool RenderThemeGtk::paintMenuList(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    return paintRenderObject(MOZ_GTK_DROPDOWN, object, info.context, rect);
}

bool RenderThemeGtk::paintTextField(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    return paintRenderObject(MOZ_GTK_ENTRY, object, info.context, rect);
}

bool RenderThemeGtk::paintSliderTrack(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    if (info.context->paintingDisabled())
        return false;

    ControlPart part = object->style()->appearance();
    ASSERT(part == SliderHorizontalPart || part == SliderVerticalPart || part == MediaVolumeSliderPart);

    // We shrink the trough rect slightly to make room for the focus indicator.
    IntRect troughRect(IntPoint(), rect.size()); // This is relative to rect.
    GtkWidget* widget = 0;
    if (part == SliderHorizontalPart) {
        widget = gtkHScale();
        troughRect.inflateX(-gtk_widget_get_style(widget)->xthickness);
    } else {
        widget = gtkVScale();
        troughRect.inflateY(-gtk_widget_get_style(widget)->ythickness);
    }
    gtk_widget_set_direction(widget, gtkTextDirection(object->style()->direction()));

    WidgetRenderingContext widgetContext(info.context, rect);
    widgetContext.gtkPaintBox(troughRect, widget, GTK_STATE_ACTIVE, GTK_SHADOW_OUT, "trough");
    if (isFocused(object))
        widgetContext.gtkPaintFocus(IntRect(IntPoint(), rect.size()), widget, getGtkStateType(this, object), "trough");

    return false;
}

bool RenderThemeGtk::paintSliderThumb(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    if (info.context->paintingDisabled())
        return false;

    ControlPart part = object->style()->appearance();
    ASSERT(part == SliderThumbHorizontalPart || part == SliderThumbVerticalPart || part == MediaVolumeSliderThumbPart);

    GtkWidget* widget = 0;
    const char* detail = 0;
    GtkOrientation orientation;
    if (part == SliderThumbHorizontalPart) {
        widget = gtkHScale();
        detail = "hscale";
        orientation = GTK_ORIENTATION_HORIZONTAL;
    } else {
        widget = gtkVScale();
        detail = "vscale";
        orientation = GTK_ORIENTATION_VERTICAL;
    }
    gtk_widget_set_direction(widget, gtkTextDirection(object->style()->direction()));

    // Only some themes have slider thumbs respond to clicks and some don't. This information is
    // gathered via the 'activate-slider' property, but it's deprecated in GTK+ 2.22 and removed in
    // GTK+ 3.x. The drawback of not honoring it is that slider thumbs change color when you click
    // on them. 
    IntRect thumbRect(IntPoint(), rect.size());
    WidgetRenderingContext widgetContext(info.context, rect);
    widgetContext.gtkPaintSlider(thumbRect, widget, getGtkStateType(this, object), GTK_SHADOW_OUT, detail, orientation);
    return false;
}

void RenderThemeGtk::adjustSliderThumbSize(RenderObject* o) const
{
    ControlPart part = o->style()->appearance();
#if ENABLE(VIDEO)
    if (part == MediaSliderThumbPart) {
        adjustMediaSliderThumbSize(o);
        return;
    }
#endif

    GtkWidget* widget = part == SliderThumbHorizontalPart ? gtkHScale() : gtkVScale();
    int length = 0, width = 0;
    gtk_widget_style_get(widget,
                         "slider_length", &length,
                         "slider_width", &width,
                         NULL);

    if (part == SliderThumbHorizontalPart) {
        o->style()->setWidth(Length(length, Fixed));
        o->style()->setHeight(Length(width, Fixed));
        return;
    }
    ASSERT(part == SliderThumbVerticalPart || part == MediaVolumeSliderThumbPart);
    o->style()->setWidth(Length(width, Fixed));
    o->style()->setHeight(Length(length, Fixed));
}

#if ENABLE(PROGRESS_TAG)
double RenderThemeGtk::animationRepeatIntervalForProgressBar(RenderProgress*) const
{
    // FIXME: It doesn't look like there is a good way yet to support animated
    // progress bars with the Mozilla theme drawing code.
    return 0;
}

double RenderThemeGtk::animationDurationForProgressBar(RenderProgress*) const
{
    // FIXME: It doesn't look like there is a good way yet to support animated
    // progress bars with the Mozilla theme drawing code.
    return 0;
}

bool RenderThemeGtk::paintProgressBar(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!renderObject->isProgress())
        return true;

    GtkWidget* progressBarWidget = moz_gtk_get_progress_widget();
    if (!progressBarWidget)
        return true;

    if (paintRenderObject(MOZ_GTK_PROGRESSBAR, renderObject, paintInfo.context, rect))
        return true;

    IntRect chunkRect(rect);
    RenderProgress* renderProgress = toRenderProgress(renderObject);

    GtkStyle* style = gtk_widget_get_style(progressBarWidget);
    chunkRect.setHeight(chunkRect.height() - (2 * style->ythickness));
    chunkRect.setY(chunkRect.y() + style->ythickness);
    chunkRect.setWidth((chunkRect.width() - (2 * style->xthickness)) * renderProgress->position());
    if (renderObject->style()->direction() == RTL)
        chunkRect.setX(rect.x() + rect.width() - chunkRect.width() - style->xthickness);
    else
        chunkRect.setX(chunkRect.x() + style->xthickness);

    return paintRenderObject(MOZ_GTK_PROGRESS_CHUNK, renderObject, paintInfo.context, chunkRect);
}
#endif

GRefPtr<GdkPixbuf> RenderThemeGtk::getStockIcon(GType widgetType, const char* iconName, gint direction, gint state, gint iconSize)
{
    ASSERT(widgetType == GTK_TYPE_CONTAINER || widgetType == GTK_TYPE_ENTRY);
    GtkWidget* widget = widgetType == GTK_TYPE_CONTAINER ? GTK_WIDGET(gtkContainer()) : gtkEntry();
    GtkStyle* style = gtk_widget_get_style(widget);
    GtkIconSet* iconSet = gtk_style_lookup_icon_set(style, iconName);
    return adoptGRef(gtk_icon_set_render_icon(iconSet, style,
                                              static_cast<GtkTextDirection>(direction),
                                              static_cast<GtkStateType>(state),
                                              static_cast<GtkIconSize>(iconSize), 0, 0));
}


Color RenderThemeGtk::platformActiveSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return gtk_widget_get_style(widget)->base[GTK_STATE_SELECTED];
}

Color RenderThemeGtk::platformInactiveSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return gtk_widget_get_style(widget)->base[GTK_STATE_ACTIVE];
}

Color RenderThemeGtk::platformActiveSelectionForegroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return gtk_widget_get_style(widget)->text[GTK_STATE_SELECTED];
}

Color RenderThemeGtk::platformInactiveSelectionForegroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return gtk_widget_get_style(widget)->text[GTK_STATE_ACTIVE];
}

Color RenderThemeGtk::activeListBoxSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return gtk_widget_get_style(widget)->base[GTK_STATE_SELECTED];
}

Color RenderThemeGtk::inactiveListBoxSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return gtk_widget_get_style(widget)->base[GTK_STATE_ACTIVE];
}

Color RenderThemeGtk::activeListBoxSelectionForegroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return gtk_widget_get_style(widget)->text[GTK_STATE_SELECTED];
}

Color RenderThemeGtk::inactiveListBoxSelectionForegroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return gtk_widget_get_style(widget)->text[GTK_STATE_ACTIVE];
}

Color RenderThemeGtk::systemColor(int cssValueId) const
{
    switch (cssValueId) {
    case CSSValueButtontext:
        return Color(gtk_widget_get_style(gtkButton())->fg[GTK_STATE_NORMAL]);
    case CSSValueCaptiontext:
        return Color(gtk_widget_get_style(gtkEntry())->fg[GTK_STATE_NORMAL]);
    default:
        return RenderTheme::systemColor(cssValueId);
    }
}

static void gtkStyleSetCallback(GtkWidget* widget, GtkStyle* previous, RenderTheme* renderTheme)
{
    // FIXME: Make sure this function doesn't get called many times for a single GTK+ style change signal.
    renderTheme->platformColorsDidChange();
}

void RenderThemeGtk::setupWidgetAndAddToContainer(GtkWidget* widget, GtkWidget* window) const
{
    gtk_container_add(GTK_CONTAINER(window), widget);
    gtk_widget_realize(widget);
    g_object_set_data(G_OBJECT(widget), "transparent-bg-hint", GINT_TO_POINTER(TRUE));

    // FIXME: Perhaps this should only be called for the containing window or parent container.
    g_signal_connect(widget, "style-set", G_CALLBACK(gtkStyleSetCallback), const_cast<RenderThemeGtk*>(this));
}

GtkWidget* RenderThemeGtk::gtkContainer() const
{
    if (m_gtkContainer)
        return m_gtkContainer;

    m_gtkWindow = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_widget_set_colormap(m_gtkWindow, m_themeParts.colormap);
    gtk_widget_realize(m_gtkWindow);
    gtk_widget_set_name(m_gtkWindow, "MozillaGtkWidget");

    m_gtkContainer = gtk_fixed_new();
    setupWidgetAndAddToContainer(m_gtkContainer, m_gtkWindow);
    return m_gtkContainer;
}

GtkWidget* RenderThemeGtk::gtkButton() const
{
    if (m_gtkButton)
        return m_gtkButton;
    m_gtkButton = gtk_button_new();
    setupWidgetAndAddToContainer(m_gtkButton, gtkContainer());
    return m_gtkButton;
}

GtkWidget* RenderThemeGtk::gtkEntry() const
{
    if (m_gtkEntry)
        return m_gtkEntry;
    m_gtkEntry = gtk_entry_new();
    setupWidgetAndAddToContainer(m_gtkEntry, gtkContainer());
    return m_gtkEntry;
}

GtkWidget* RenderThemeGtk::gtkTreeView() const
{
    if (m_gtkTreeView)
        return m_gtkTreeView;
    m_gtkTreeView = gtk_tree_view_new();
    setupWidgetAndAddToContainer(m_gtkTreeView, gtkContainer());
    return m_gtkTreeView;
}

GtkWidget* RenderThemeGtk::gtkVScale() const
{
    if (m_gtkVScale)
        return m_gtkVScale;
    m_gtkVScale = gtk_vscale_new(0);
    setupWidgetAndAddToContainer(m_gtkVScale, gtkContainer());
    return m_gtkVScale;
}

GtkWidget* RenderThemeGtk::gtkHScale() const
{
    if (m_gtkHScale)
        return m_gtkHScale;
    m_gtkHScale = gtk_hscale_new(0);
    setupWidgetAndAddToContainer(m_gtkHScale, gtkContainer());
    return m_gtkHScale;
}

GtkWidget* RenderThemeGtk::gtkScrollbar()
{
    return moz_gtk_get_scrollbar_widget();
}

} // namespace WebCore

#endif // GTK_API_VERSION_2
