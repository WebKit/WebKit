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

#ifndef GTK_API_VERSION_2

#include "CSSValueKeywords.h"
#include "GraphicsContext.h"
#include "GtkVersioning.h"
#include "HTMLNames.h"
#include "MediaControlElements.h"
#include "Page.h"
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

typedef HashMap<GType, GRefPtr<GtkStyleContext> > StyleContextMap;
static StyleContextMap& styleContextMap();

static void gtkStyleChangedCallback(GObject*, GParamSpec*)
{
    StyleContextMap::const_iterator end = styleContextMap().end();
    for (StyleContextMap::const_iterator iter = styleContextMap().begin(); iter != end; ++iter)
        gtk_style_context_invalidate(iter->second.get());

    Page::scheduleForcedStyleRecalcForAllPages();
}

static StyleContextMap& styleContextMap()
{
    DEFINE_STATIC_LOCAL(StyleContextMap, map, ());

    static bool initialized = false;
    if (!initialized) {
        GtkSettings* settings = gtk_settings_get_default();
        g_signal_connect(settings, "notify::gtk-theme-name", G_CALLBACK(gtkStyleChangedCallback), 0);
        g_signal_connect(settings, "notify::gtk-color-scheme", G_CALLBACK(gtkStyleChangedCallback), 0);
        initialized = true;
    }
    return map;
}

static GtkStyleContext* getStyleContext(GType widgetType)
{
    std::pair<StyleContextMap::iterator, bool> result = styleContextMap().add(widgetType, 0);
    if (!result.second)
        return result.first->second.get();

    GtkWidgetPath* path = gtk_widget_path_new();
    gtk_widget_path_append_type(path, widgetType);

    GRefPtr<GtkStyleContext> context = adoptGRef(gtk_style_context_new());
    gtk_style_context_set_path(context.get(), path);
    gtk_widget_path_free(path);

    result.first->second = context;
    return context.get();
}

// This is not a static method, because we want to avoid having GTK+ headers in RenderThemeGtk.h.
extern GtkTextDirection gtkTextDirection(TextDirection);

void RenderThemeGtk::initMediaColors()
{
    GtkStyle* style = gtk_widget_get_style(GTK_WIDGET(gtkContainer()));
    m_panelColor = style->bg[GTK_STATE_NORMAL];
    m_sliderColor = style->bg[GTK_STATE_ACTIVE];
    m_sliderThumbColor = style->bg[GTK_STATE_SELECTED];
}

static void adjustRectForFocus(GtkStyleContext* context, IntRect& rect)
{
    gint focusWidth, focusPad;
    gtk_style_context_get_style(context,
                                "focus-line-width", &focusWidth,
                                "focus-padding", &focusPad, NULL);
    rect.inflate(focusWidth + focusPad);
}

void RenderThemeGtk::adjustRepaintRect(const RenderObject* renderObject, IntRect& rect)
{
    GtkStyleContext* context = 0;
    ControlPart part = renderObject->style()->appearance();
    switch (part) {
    case SliderVerticalPart:
    case SliderHorizontalPart:
        context = getStyleContext(part == SliderThumbHorizontalPart ?  GTK_TYPE_HSCALE : GTK_TYPE_VSCALE);
        break;
    case ButtonPart:
        context = getStyleContext(GTK_TYPE_BUTTON);

        gboolean interiorFocus;
        gtk_style_context_get_style(context, "interior-focus", &interiorFocus, NULL);
        if (interiorFocus)
            return;

        break;
    default:
        return;
    }

    ASSERT(context);
    adjustRectForFocus(context, rect);
}

GtkStateType RenderThemeGtk::getGtkStateType(RenderObject* object)
{
    if (!isEnabled(object) || isReadOnlyControl(object))
        return GTK_STATE_INSENSITIVE;
    if (isPressed(object))
        return GTK_STATE_ACTIVE;
    if (isHovered(object))
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

static void setToggleSize(const RenderThemeGtk* theme, RenderStyle* style, ControlPart appearance)
{
    // The width and height are both specified, so we shouldn't change them.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    // FIXME: This is probably not correct use of indicatorSize and indicatorSpacing.
    gint indicatorSize, indicatorSpacing;
    theme->getIndicatorMetrics(appearance, indicatorSize, indicatorSpacing);

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

bool RenderThemeGtk::paintButton(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    GtkStyleContext* context = getStyleContext(GTK_TYPE_BUTTON);
    gtk_style_context_save(context);

    gtk_style_context_set_direction(context, static_cast<GtkTextDirection>(gtkTextDirection(renderObject->style()->direction())));
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_BUTTON);

    IntRect buttonRect(rect);

    if (isDefault(renderObject)) {
        GtkBorder* borderPtr = 0;
        GtkBorder border = { 1, 1, 1, 1 };

        gtk_style_context_get_style(context, "default-border", &borderPtr, NULL);
        if (borderPtr) {
            border = *borderPtr;
            gtk_border_free(borderPtr);
        }

        buttonRect.move(border.left, border.top);
        buttonRect.setWidth(buttonRect.width() - (border.left + border.right));
        buttonRect.setHeight(buttonRect.height() - (border.top + border.bottom));

        gtk_style_context_add_class(context, GTK_STYLE_CLASS_DEFAULT);
    }

    guint flags = 0;
    if (!isEnabled(renderObject) || isReadOnlyControl(renderObject))
        flags |= GTK_STATE_FLAG_INSENSITIVE;
    else if (isHovered(renderObject))
        flags |= GTK_STATE_FLAG_PRELIGHT;
    if (isPressed(renderObject))
        flags |= GTK_STATE_FLAG_ACTIVE;
    gtk_style_context_set_state(context, static_cast<GtkStateFlags>(flags));

    gtk_render_background(context, paintInfo.context->platformContext(), buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height());
    gtk_render_frame(context, paintInfo.context->platformContext(), buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height());

    if (isFocused(renderObject)) {
        gint focusWidth, focusPad;
        gboolean displaceFocus, interiorFocus;
        gtk_style_context_get_style(context,
                                    "focus-line-width", &focusWidth,
                                    "focus-padding", &focusPad,
                                    "interior-focus", &interiorFocus,
                                    "displace-focus", &displaceFocus,
                                    NULL);

        if (interiorFocus) {
            GtkBorder borderWidth;
            gtk_style_context_get_border(context, static_cast<GtkStateFlags>(flags), &borderWidth);

            buttonRect = IntRect(buttonRect.x() + borderWidth.left + focusPad, buttonRect.y() + borderWidth.top + focusPad,
                                 buttonRect.width() - (2 * focusPad + borderWidth.left + borderWidth.right),
                                 buttonRect.height() - (2 * focusPad + borderWidth.top + borderWidth.bottom));
        } else
            buttonRect.inflate(focusWidth + focusPad);

        if (displaceFocus && isPressed(renderObject)) {
            gint childDisplacementX;
            gint childDisplacementY;
            gtk_style_context_get_style(context,
                                        "child-displacement-x", &childDisplacementX,
                                        "child-displacement-y", &childDisplacementY,
                                        NULL);
            buttonRect.move(childDisplacementX, childDisplacementY);
        }

        gtk_render_focus(context, paintInfo.context->platformContext(), buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height());
    }

    gtk_style_context_restore(context);

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

bool RenderThemeGtk::paintSliderTrack(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    ControlPart part = renderObject->style()->appearance();
    ASSERT(part == SliderHorizontalPart || part == SliderVerticalPart);

    GtkStyleContext* context = getStyleContext(part == SliderThumbHorizontalPart ? GTK_TYPE_HSCALE : GTK_TYPE_VSCALE);
    gtk_style_context_save(context);

    gtk_style_context_set_direction(context, gtkTextDirection(renderObject->style()->direction()));
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_SCALE);
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_TROUGH);

    if (!isEnabled(renderObject) || isReadOnlyControl(renderObject))
        gtk_style_context_set_state(context, GTK_STATE_FLAG_INSENSITIVE);

    gtk_render_background(context, paintInfo.context->platformContext(),
                          rect.x(), rect.y(), rect.width(), rect.height());
    gtk_render_frame(context, paintInfo.context->platformContext(),
                     rect.x(), rect.y(), rect.width(), rect.height());

    if (isFocused(renderObject)) {
        gint focusWidth, focusPad;
        gtk_style_context_get_style(context,
                                    "focus-line-width", &focusWidth,
                                    "focus-padding", &focusPad, NULL);
        IntRect focusRect(rect);
        focusRect.inflate(focusWidth + focusPad);
        gtk_render_focus(context, paintInfo.context->platformContext(),
                         focusRect.x(), focusRect.y(), focusRect.width(), focusRect.height());
    }

    gtk_style_context_restore(context);
    return false;
}

bool RenderThemeGtk::paintSliderThumb(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    ControlPart part = renderObject->style()->appearance();
    ASSERT(part == SliderThumbHorizontalPart || part == SliderThumbVerticalPart);

    GtkStyleContext* context = getStyleContext(part == SliderThumbHorizontalPart ? GTK_TYPE_HSCALE : GTK_TYPE_VSCALE);
    gtk_style_context_save(context);

    gtk_style_context_set_direction(context, gtkTextDirection(renderObject->style()->direction()));
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_SCALE);
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_SLIDER);

    gint troughBorder;
    gtk_style_context_get_style(context, "trough-border", &troughBorder, NULL);

    IntRect sliderRect(rect);
    sliderRect.inflate(-troughBorder);

    guint flags = 0;
    if (!isEnabled(renderObject) || isReadOnlyControl(renderObject))
        flags |= GTK_STATE_FLAG_INSENSITIVE;
    else if (isHovered(renderObject))
        flags |= GTK_STATE_FLAG_PRELIGHT;
    if (isPressed(renderObject))
        flags |= GTK_STATE_FLAG_ACTIVE;
    gtk_style_context_set_state(context, static_cast<GtkStateFlags>(flags));

    gtk_render_slider(context, paintInfo.context->platformContext(), sliderRect.x(), sliderRect.y(), sliderRect.width(), sliderRect.height(),
                      part == SliderThumbHorizontalPart ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);

    gtk_style_context_restore(context);

    return false;
}

void RenderThemeGtk::adjustSliderThumbSize(RenderObject* renderObject) const
{
    ControlPart part = renderObject->style()->appearance();
#if ENABLE(VIDEO)
    if (part == MediaSliderThumbPart || part == MediaVolumeSliderThumbPart) {
        adjustMediaSliderThumbSize(renderObject);
        return;
    }
#endif

    gint sliderWidth, sliderLength;
    gtk_style_context_get_style(getStyleContext(part == SliderThumbHorizontalPart ? GTK_TYPE_HSCALE : GTK_TYPE_VSCALE),
                                "slider-width", &sliderWidth,
                                "slider-length", &sliderLength,
                                NULL);
    if (part == SliderThumbHorizontalPart) {
        renderObject->style()->setWidth(Length(sliderLength, Fixed));
        renderObject->style()->setHeight(Length(sliderWidth, Fixed));
        return;
    }
    ASSERT(part == SliderThumbVerticalPart);
    renderObject->style()->setWidth(Length(sliderWidth, Fixed));
    renderObject->style()->setHeight(Length(sliderLength, Fixed));
}

#if ENABLE(PROGRESS_TAG)
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
    GtkStyleContext* context = getStyleContext(widgetType);
    GtkIconSet* iconSet = gtk_style_context_lookup_icon_set(context, iconName);

    gtk_style_context_save(context);

    guint flags = 0;
    if (state == GTK_STATE_PRELIGHT)
        flags |= GTK_STATE_FLAG_PRELIGHT;
    else if (state == GTK_STATE_INSENSITIVE)
        flags |= GTK_STATE_FLAG_INSENSITIVE;

    gtk_style_context_set_state(context, static_cast<GtkStateFlags>(flags));
    gtk_style_context_set_direction(context, static_cast<GtkTextDirection>(direction));
    GdkPixbuf* icon = gtk_icon_set_render_icon_pixbuf(iconSet, context, static_cast<GtkIconSize>(iconSize));

    gtk_style_context_restore(context);

    return adoptGRef(icon);
}

Color RenderThemeGtk::platformActiveSelectionBackgroundColor() const
{
    GdkRGBA gdkRGBAColor;
    gtk_style_context_get_background_color(getStyleContext(GTK_TYPE_ENTRY), GTK_STATE_FLAG_SELECTED, &gdkRGBAColor);
    return gdkRGBAColor;
}

Color RenderThemeGtk::platformInactiveSelectionBackgroundColor() const
{
    GdkRGBA gdkRGBAColor;
    gtk_style_context_get_background_color(getStyleContext(GTK_TYPE_ENTRY), GTK_STATE_FLAG_ACTIVE, &gdkRGBAColor);
    return gdkRGBAColor;
}

Color RenderThemeGtk::platformActiveSelectionForegroundColor() const
{
    GdkRGBA gdkRGBAColor;
    gtk_style_context_get_color(getStyleContext(GTK_TYPE_ENTRY), GTK_STATE_FLAG_SELECTED, &gdkRGBAColor);
    return gdkRGBAColor;
}

Color RenderThemeGtk::platformInactiveSelectionForegroundColor() const
{
    GdkRGBA gdkRGBAColor;
    gtk_style_context_get_color(getStyleContext(GTK_TYPE_ENTRY), GTK_STATE_FLAG_ACTIVE, &gdkRGBAColor);
    return gdkRGBAColor;
}

Color RenderThemeGtk::activeListBoxSelectionBackgroundColor() const
{
    GdkRGBA gdkRGBAColor;
    gtk_style_context_get_background_color(getStyleContext(GTK_TYPE_TREE_VIEW), GTK_STATE_FLAG_SELECTED, &gdkRGBAColor);
    return gdkRGBAColor;
}

Color RenderThemeGtk::inactiveListBoxSelectionBackgroundColor() const
{
    GdkRGBA gdkRGBAColor;
    gtk_style_context_get_background_color(getStyleContext(GTK_TYPE_TREE_VIEW), GTK_STATE_FLAG_ACTIVE, &gdkRGBAColor);
    return gdkRGBAColor;
}

Color RenderThemeGtk::activeListBoxSelectionForegroundColor() const
{
    GdkRGBA gdkRGBAColor;
    gtk_style_context_get_color(getStyleContext(GTK_TYPE_TREE_VIEW), GTK_STATE_FLAG_SELECTED, &gdkRGBAColor);
    return gdkRGBAColor;
}

Color RenderThemeGtk::inactiveListBoxSelectionForegroundColor() const
{
    GdkRGBA gdkRGBAColor;
    gtk_style_context_get_color(getStyleContext(GTK_TYPE_TREE_VIEW), GTK_STATE_FLAG_ACTIVE, &gdkRGBAColor);
    return gdkRGBAColor;
}

Color RenderThemeGtk::systemColor(int cssValueId) const
{
    GdkRGBA gdkRGBAColor;

    switch (cssValueId) {
    case CSSValueButtontext:
        gtk_style_context_get_color(getStyleContext(GTK_TYPE_BUTTON), static_cast<GtkStateFlags>(0), &gdkRGBAColor);
        return gdkRGBAColor;
    case CSSValueCaptiontext:
        gtk_style_context_get_color(getStyleContext(GTK_TYPE_ENTRY), static_cast<GtkStateFlags>(0), &gdkRGBAColor);
        return gdkRGBAColor;
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

GtkStyleContext* RenderThemeGtk::gtkScrollbarStyle()
{
    return getStyleContext(GTK_TYPE_SCROLLBAR);
}

} // namespace WebCore

#endif // !GTK_API_VERSION_2
