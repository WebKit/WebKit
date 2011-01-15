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
#include "PaintInfo.h"
#include "RenderObject.h"
#include "TextDirection.h"
#include "UserAgentStyleSheets.h"
#include <cmath>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#if ENABLE(PROGRESS_TAG)
#include "RenderProgress.h"
#endif

namespace WebCore {

// This is the default value defined by GTK+, where it was defined as MIN_ARROW_SIZE in gtkarrow.c.
static const int minArrowSize = 15;

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

GtkStyleContext* RenderThemeGtk::gtkScrollbarStyle()
{
    return getStyleContext(GTK_TYPE_SCROLLBAR);
}

// This is not a static method, because we want to avoid having GTK+ headers in RenderThemeGtk.h.
extern GtkTextDirection gtkTextDirection(TextDirection);

void RenderThemeGtk::platformInit()
{
}

RenderThemeGtk::~RenderThemeGtk()
{
}

#if ENABLE(VIDEO)
void RenderThemeGtk::initMediaColors()
{
    GdkRGBA color;
    GtkStyleContext* containerContext = getStyleContext(GTK_TYPE_CONTAINER);

    gtk_style_context_get_background_color(containerContext, GTK_STATE_FLAG_NORMAL, &color);
    m_panelColor = color;
    gtk_style_context_get_background_color(containerContext, GTK_STATE_FLAG_ACTIVE, &color);
    m_sliderColor = color;
    gtk_style_context_get_background_color(containerContext, GTK_STATE_FLAG_SELECTED, &color);
    m_sliderThumbColor = color;
}
#endif

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
    bool checkInteriorFocus = false;
    ControlPart part = renderObject->style()->appearance();
    switch (part) {
    case CheckboxPart:
    case RadioPart:
        context = getStyleContext(part == CheckboxPart ? GTK_TYPE_CHECK_BUTTON : GTK_TYPE_RADIO_BUTTON);

        gint indicatorSpacing;
        gtk_style_context_get_style(context, "indicator-spacing", &indicatorSpacing, NULL);
        rect.inflate(indicatorSpacing);

        return;
    case SliderVerticalPart:
    case SliderHorizontalPart:
        context = getStyleContext(part == SliderThumbHorizontalPart ?  GTK_TYPE_HSCALE : GTK_TYPE_VSCALE);
        break;
    case ButtonPart:
    case MenulistButtonPart:
    case MenulistPart:
        context = getStyleContext(GTK_TYPE_BUTTON);
        checkInteriorFocus = true;
        break;
    case TextFieldPart:
    case TextAreaPart:
        context = getStyleContext(GTK_TYPE_ENTRY);
        checkInteriorFocus = true;
        break;
    default:
        return;
    }

    ASSERT(context);
    if (checkInteriorFocus) {
        gboolean interiorFocus;
        gtk_style_context_get_style(context, "interior-focus", &interiorFocus, NULL);
        if (interiorFocus)
            return;
    }
    adjustRectForFocus(context, rect);
}

static void setToggleSize(GtkStyleContext* context, RenderStyle* style)
{
    // The width and height are both specified, so we shouldn't change them.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    // Other ports hard-code this to 13 which is also the default value defined by GTK+.
    // GTK+ users tend to demand the native look.
    // It could be made a configuration option values other than 13 actually break site compatibility.
    gint indicatorSize;
    gtk_style_context_get_style(context, "indicator-size", &indicatorSize, NULL);

    if (style->width().isIntrinsicOrAuto())
        style->setWidth(Length(indicatorSize, Fixed));

    if (style->height().isAuto())
        style->setHeight(Length(indicatorSize, Fixed));
}

static void paintToggle(const RenderThemeGtk* theme, GType widgetType, RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    GtkStyleContext* context = getStyleContext(widgetType);
    gtk_style_context_save(context);

    gtk_style_context_set_direction(context, static_cast<GtkTextDirection>(gtkTextDirection(renderObject->style()->direction())));
    gtk_style_context_add_class(context, widgetType == GTK_TYPE_CHECK_BUTTON ? GTK_STYLE_CLASS_CHECK : GTK_STYLE_CLASS_RADIO);

    guint flags = 0;
    if (!theme->isEnabled(renderObject) || theme->isReadOnlyControl(renderObject))
        flags |= GTK_STATE_FLAG_INSENSITIVE;
    else if (theme->isHovered(renderObject))
        flags |= GTK_STATE_FLAG_PRELIGHT;
    if (theme->isIndeterminate(renderObject))
        flags |= GTK_STATE_FLAG_INCONSISTENT;
    else if (theme->isChecked(renderObject))
        flags |= GTK_STATE_FLAG_ACTIVE;
    if (theme->isPressed(renderObject))
        flags |= GTK_STATE_FLAG_SELECTED;
    gtk_style_context_set_state(context, static_cast<GtkStateFlags>(flags));

    if (widgetType == GTK_TYPE_CHECK_BUTTON)
        gtk_render_check(context, paintInfo.context->platformContext(), rect.x(), rect.y(), rect.width(), rect.height());
    else
        gtk_render_option(context, paintInfo.context->platformContext(), rect.x(), rect.y(), rect.width(), rect.height());

    if (theme->isFocused(renderObject)) {
        IntRect indicatorRect(rect);
        gint indicatorSpacing;
        gtk_style_context_get_style(context, "indicator-spacing", &indicatorSpacing, NULL);
        indicatorRect.inflate(indicatorSpacing);
        gtk_render_focus(context, paintInfo.context->platformContext(), indicatorRect.x(), indicatorRect.y(),
                         indicatorRect.width(), indicatorRect.height());
    }

    gtk_style_context_restore(context);
}

void RenderThemeGtk::setCheckboxSize(RenderStyle* style) const
{
    setToggleSize(getStyleContext(GTK_TYPE_CHECK_BUTTON), style);
}

bool RenderThemeGtk::paintCheckbox(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    paintToggle(this, GTK_TYPE_CHECK_BUTTON, renderObject, paintInfo, rect);
    return false;
}

void RenderThemeGtk::setRadioSize(RenderStyle* style) const
{
    setToggleSize(getStyleContext(GTK_TYPE_RADIO_BUTTON), style);
}

bool RenderThemeGtk::paintRadio(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    paintToggle(this, GTK_TYPE_RADIO_BUTTON, renderObject, paintInfo, rect);
    return false;
}

static void renderButton(RenderTheme* theme, GtkStyleContext* context, RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    IntRect buttonRect(rect);

    guint flags = 0;
    if (!theme->isEnabled(renderObject) || theme->isReadOnlyControl(renderObject))
        flags |= GTK_STATE_FLAG_INSENSITIVE;
    else if (theme->isHovered(renderObject))
        flags |= GTK_STATE_FLAG_PRELIGHT;
    if (theme->isPressed(renderObject))
        flags |= GTK_STATE_FLAG_ACTIVE;
    gtk_style_context_set_state(context, static_cast<GtkStateFlags>(flags));

    if (theme->isDefault(renderObject)) {
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

    gtk_render_background(context, paintInfo.context->platformContext(), buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height());
    gtk_render_frame(context, paintInfo.context->platformContext(), buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height());

    if (theme->isFocused(renderObject)) {
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

        if (displaceFocus && theme->isPressed(renderObject)) {
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
}
bool RenderThemeGtk::paintButton(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    GtkStyleContext* context = getStyleContext(GTK_TYPE_BUTTON);
    gtk_style_context_save(context);

    gtk_style_context_set_direction(context, static_cast<GtkTextDirection>(gtkTextDirection(renderObject->style()->direction())));
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_BUTTON);

    renderButton(this, context, renderObject, paintInfo, rect);

    gtk_style_context_restore(context);

    return false;
}

static void getComboBoxMetrics(RenderStyle* style, GtkBorder& border, int& focus, int& separator)
{
    // If this menu list button isn't drawn using the native theme, we
    // don't add any extra padding beyond what WebCore already uses.
    if (style->appearance() == NoControlPart)
        return;

    GtkStyleContext* context = getStyleContext(GTK_TYPE_BUTTON);
    gtk_style_context_save(context);

    gtk_style_context_add_class(context, GTK_STYLE_CLASS_BUTTON);
    gtk_style_context_set_direction(context, static_cast<GtkTextDirection>(gtkTextDirection(style->direction())));

    gtk_style_context_get_border(context, static_cast<GtkStateFlags>(0), &border);

    gboolean interiorFocus;
    gint focusWidth, focusPad;
    gtk_style_context_get_style(context,
                                "interior-focus", &interiorFocus,
                                "focus-line-width", &focusWidth,
                                "focus-padding", &focusPad, NULL);
    focus = interiorFocus ? focusWidth + focusPad : 0;

    gtk_style_context_restore(context);

    context = getStyleContext(GTK_TYPE_SEPARATOR);
    gtk_style_context_save(context);

    GtkTextDirection direction = static_cast<GtkTextDirection>(gtkTextDirection(style->direction()));
    gtk_style_context_set_direction(context, direction);
    gtk_style_context_add_class(context, "separator");

    gboolean wideSeparators;
    gint separatorWidth;
    gtk_style_context_get_style(context,
                                "wide-separators", &wideSeparators,
                                "separator-width", &separatorWidth,
                                NULL);

    // GTK+ always uses border.left, regardless of text direction. See gtkseperator.c.
    if (!wideSeparators)
        separatorWidth = border.left;

    separator = separatorWidth;

    gtk_style_context_restore(context);
}

int RenderThemeGtk::popupInternalPaddingLeft(RenderStyle* style) const
{
    GtkBorder borderWidth = { 0, 0, 0, 0 };
    int focusWidth = 0, separatorWidth = 0;
    getComboBoxMetrics(style, borderWidth, focusWidth, separatorWidth);
    int left = borderWidth.left + focusWidth;
    if (style->direction() == RTL)
        left += separatorWidth + minArrowSize;
    return left;
}

int RenderThemeGtk::popupInternalPaddingRight(RenderStyle* style) const
{
    GtkBorder borderWidth = { 0, 0, 0, 0 };
    int focusWidth = 0, separatorWidth = 0;
    getComboBoxMetrics(style, borderWidth, focusWidth, separatorWidth);
    int right = borderWidth.right + focusWidth;
    if (style->direction() == LTR)
        right += separatorWidth + minArrowSize;
    return right;
}

int RenderThemeGtk::popupInternalPaddingTop(RenderStyle* style) const
{
    GtkBorder borderWidth = { 0, 0, 0, 0 };
    int focusWidth = 0, separatorWidth = 0;
    getComboBoxMetrics(style, borderWidth, focusWidth, separatorWidth);
    return borderWidth.top + focusWidth;
}

int RenderThemeGtk::popupInternalPaddingBottom(RenderStyle* style) const
{
    GtkBorder borderWidth = { 0, 0, 0, 0 };
    int focusWidth = 0, separatorWidth = 0;
    getComboBoxMetrics(style, borderWidth, focusWidth, separatorWidth);
    return borderWidth.bottom + focusWidth;
}

bool RenderThemeGtk::paintMenuList(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    cairo_t* cairoContext = paintInfo.context->platformContext();
    GtkTextDirection direction = static_cast<GtkTextDirection>(gtkTextDirection(renderObject->style()->direction()));

    // Paint the button.
    GtkStyleContext* buttonStyleContext = getStyleContext(GTK_TYPE_BUTTON);
    gtk_style_context_save(buttonStyleContext);
    gtk_style_context_set_direction(buttonStyleContext, direction);
    gtk_style_context_add_class(buttonStyleContext, GTK_STYLE_CLASS_BUTTON);
    renderButton(this, buttonStyleContext, renderObject, paintInfo, rect);

    // Get the inner rectangle.
    gint focusWidth, focusPad;
    GtkBorder* innerBorderPtr = 0;
    GtkBorder innerBorder = { 1, 1, 1, 1 };
    gtk_style_context_get_style(buttonStyleContext,
                                "inner-border", &innerBorderPtr,
                                "focus-line-width", &focusWidth,
                                "focus-padding", &focusPad,
                                NULL);
    if (innerBorderPtr) {
        innerBorder = *innerBorderPtr;
        gtk_border_free(innerBorderPtr);
    }

    GtkBorder borderWidth;
    GtkStateFlags state = gtk_style_context_get_state(buttonStyleContext);
    gtk_style_context_get_border(buttonStyleContext, state, &borderWidth);

    focusWidth += focusPad;
    IntRect innerRect(rect.x() + innerBorder.left + borderWidth.left + focusWidth,
                      rect.y() + innerBorder.top + borderWidth.top + focusWidth,
                      rect.width() - borderWidth.left - borderWidth.right - innerBorder.left - innerBorder.right - (2 * focusWidth),
                      rect.height() - borderWidth.top - borderWidth.bottom - innerBorder.top - innerBorder.bottom - (2 * focusWidth));

    if (isPressed(renderObject)) {
        gint childDisplacementX;
        gint childDisplacementY;
        gtk_style_context_get_style(buttonStyleContext,
                                    "child-displacement-x", &childDisplacementX,
                                    "child-displacement-y", &childDisplacementY,
                                    NULL);
        innerRect.move(childDisplacementX, childDisplacementY);
    }
    innerRect.setWidth(max(1, innerRect.width()));
    innerRect.setHeight(max(1, innerRect.height()));

    gtk_style_context_restore(buttonStyleContext);

    // Paint the arrow.
    GtkStyleContext* arrowStyleContext = getStyleContext(GTK_TYPE_ARROW);
    gtk_style_context_save(arrowStyleContext);

    gtk_style_context_set_direction(arrowStyleContext, direction);
    gtk_style_context_add_class(arrowStyleContext, "arrow");
    gtk_style_context_add_class(arrowStyleContext, GTK_STYLE_CLASS_BUTTON);

    gfloat arrowScaling;
    gtk_style_context_get_style(arrowStyleContext, "arrow-scaling", &arrowScaling, NULL);

    IntSize arrowSize(minArrowSize, innerRect.height());
    IntPoint arrowPosition = innerRect.location();
    if (direction == GTK_TEXT_DIR_LTR)
        arrowPosition.move(innerRect.width() - arrowSize.width(), 0);

    // GTK+ actually fetches the xalign and valign values from the widget, but since we
    // don't have a widget here, we are just using the default xalign and valign values of 0.5.
    gint extent = std::min(arrowSize.width(), arrowSize.height()) * arrowScaling;
    arrowPosition.move(std::floor((arrowSize.width() - extent) / 2), std::floor((arrowSize.height() - extent) / 2));

    gtk_style_context_set_state(arrowStyleContext, state);
    gtk_render_arrow(arrowStyleContext, cairoContext, G_PI, arrowPosition.x(), arrowPosition.y(), extent);

    gtk_style_context_restore(arrowStyleContext);

    // Paint the separator if needed.
    GtkStyleContext* separatorStyleContext = getStyleContext(GTK_TYPE_SEPARATOR);
    gtk_style_context_save(separatorStyleContext);

    gtk_style_context_set_direction(separatorStyleContext, direction);
    gtk_style_context_add_class(separatorStyleContext, "separator");
    gtk_style_context_add_class(separatorStyleContext, GTK_STYLE_CLASS_BUTTON);

    gboolean wideSeparators;
    gint separatorWidth;
    gtk_style_context_get_style(separatorStyleContext,
                                "wide-separators", &wideSeparators,
                                "separator-width", &separatorWidth,
                                NULL);
    if (wideSeparators && !separatorWidth) {
        gtk_style_context_restore(separatorStyleContext);
        return false;
    }

    gtk_style_context_set_state(separatorStyleContext, state);
    IntPoint separatorPosition(arrowPosition.x(), innerRect.y());
    if (wideSeparators) {
        if (direction == GTK_TEXT_DIR_LTR)
            separatorPosition.move(-separatorWidth, 0);
        else
            separatorPosition.move(arrowSize.width(), 0);

        gtk_render_frame(separatorStyleContext, cairoContext,
                         separatorPosition.x(), separatorPosition.y(),
                         separatorWidth, innerRect.height());
    } else {
        GtkBorder padding;
        gtk_style_context_get_padding(separatorStyleContext, state, &padding);
        GtkBorder border;
        gtk_style_context_get_border(separatorStyleContext, state, &border);

        if (direction == GTK_TEXT_DIR_LTR)
            separatorPosition.move(-(padding.left + border.left), 0);
        else
            separatorPosition.move(arrowSize.width(), 0);

        cairo_save(cairoContext);

        // An extra clip prevents the separator bleeding outside of the specified rectangle because of subpixel positioning.
        cairo_rectangle(cairoContext, separatorPosition.x(), separatorPosition.y(), border.left, innerRect.height());
        cairo_clip(cairoContext);
        gtk_render_line(separatorStyleContext, cairoContext,
                        separatorPosition.x(), separatorPosition.y(),
                        separatorPosition.x(), innerRect.bottom());
        cairo_restore(cairoContext);
    }

    gtk_style_context_restore(separatorStyleContext);
    return false;
}

bool RenderThemeGtk::paintTextField(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    GtkStyleContext* context = getStyleContext(GTK_TYPE_ENTRY);
    gtk_style_context_save(context);

    gtk_style_context_set_direction(context, static_cast<GtkTextDirection>(gtkTextDirection(renderObject->style()->direction())));
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_ENTRY);

    guint flags = 0;
    if (!isEnabled(renderObject) || isReadOnlyControl(renderObject))
        flags |= GTK_STATE_FLAG_INSENSITIVE;
    else if (isFocused(renderObject))
        flags |= GTK_STATE_FLAG_FOCUSED;
    gtk_style_context_set_state(context, static_cast<GtkStateFlags>(flags));

    gtk_render_background(context, paintInfo.context->platformContext(), rect.x(), rect.y(), rect.width(), rect.height());
    gtk_render_frame(context, paintInfo.context->platformContext(), rect.x(), rect.y(), rect.width(), rect.height());

    if (isFocused(renderObject) && isEnabled(renderObject)) {
        gboolean interiorFocus;
        gint focusWidth, focusPad;
        gtk_style_context_get_style(context,
                                    "interior-focus", &interiorFocus,
                                    "focus-line-width", &focusWidth,
                                    "focus-padding", &focusPad,
                                    NULL);
        if (!interiorFocus) {
            IntRect focusRect(rect);
            focusRect.inflate(focusWidth + focusPad);
            gtk_render_focus(context, paintInfo.context->platformContext(),
                             focusRect.x(), focusRect.y(), focusRect.width(), focusRect.height());
        }
    }

    gtk_style_context_restore(context);

    return false;
}

bool RenderThemeGtk::paintSliderTrack(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    ControlPart part = renderObject->style()->appearance();
    ASSERT(part == SliderHorizontalPart || part == SliderVerticalPart || part == MediaVolumeSliderPart);

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
    ASSERT(part == SliderThumbHorizontalPart || part == SliderThumbVerticalPart || part == MediaVolumeSliderThumbPart);

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
    if (part == MediaSliderThumbPart) {
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
    ASSERT(part == SliderThumbVerticalPart || part == MediaVolumeSliderThumbPart);
    renderObject->style()->setWidth(Length(sliderWidth, Fixed));
    renderObject->style()->setHeight(Length(sliderLength, Fixed));
}

#if ENABLE(PROGRESS_TAG)
// These values have been copied from RenderThemeChromiumSkia.cpp
static const int progressActivityBlocks = 5;
static const int progressAnimationFrames = 10;
static const double progressAnimationInterval = 0.125;
double RenderThemeGtk::animationRepeatIntervalForProgressBar(RenderProgress*) const
{
    return progressAnimationInterval;
}

double RenderThemeGtk::animationDurationForProgressBar(RenderProgress*) const
{
    return progressAnimationInterval * progressAnimationFrames * 2; // "2" for back and forth;
}

bool RenderThemeGtk::paintProgressBar(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!renderObject->isProgress())
        return true;

    GtkStyleContext* context = getStyleContext(GTK_TYPE_PROGRESS_BAR);
    gtk_style_context_save(context);

    gtk_style_context_add_class(context, GTK_STYLE_CLASS_TROUGH);

    gtk_render_background(context, paintInfo.context->platformContext(), rect.x(), rect.y(), rect.width(), rect.height());
    gtk_render_frame(context, paintInfo.context->platformContext(), rect.x(), rect.y(), rect.width(), rect.height());

    gtk_style_context_restore(context);

    gtk_style_context_save(context);
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_PROGRESSBAR);

    RenderProgress* renderProgress = toRenderProgress(renderObject);

    GtkBorder padding;
    gtk_style_context_get_padding(context, static_cast<GtkStateFlags>(0), &padding);
    IntRect progressRect(rect.x() + padding.left, rect.y() + padding.top,
                         rect.width() - (padding.left + padding.right),
                         rect.height() - (padding.top + padding.bottom));

    if (renderProgress->isDeterminate()) {
        progressRect.setWidth(progressRect.width() * renderProgress->position());
        if (renderObject->style()->direction() == RTL)
            progressRect.setX(rect.x() + rect.width() - progressRect.width() - padding.right);
    } else {
        double animationProgress = renderProgress->animationProgress();

        progressRect.setWidth(max(2, progressRect.width() / progressActivityBlocks));
        int movableWidth = rect.width() - progressRect.width();
        if (animationProgress < 0.5)
            progressRect.setX(progressRect.x() + (animationProgress * 2 * movableWidth));
        else
            progressRect.setX(progressRect.x() + ((1.0 - animationProgress) * 2 * movableWidth));
    }

    if (!progressRect.isEmpty())
        gtk_render_activity(context, paintInfo.context->platformContext(), progressRect.x(), progressRect.y(), progressRect.width(), progressRect.height());

    gtk_style_context_restore(context);

    return false;
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

} // namespace WebCore

#endif // !GTK_API_VERSION_2
