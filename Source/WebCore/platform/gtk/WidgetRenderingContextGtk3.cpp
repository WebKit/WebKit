/*
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
#include "WidgetRenderingContext.h"

#include "GraphicsContext.h"
#include "IntRect.h"

#ifndef GTK_API_VERSION_2

namespace WebCore {

WidgetRenderingContext::WidgetRenderingContext(GraphicsContext* context, const IntRect& targetRect)
    : m_graphicsContext(context)
    , m_targetRect(targetRect)
    , m_paintRect(targetRect)
    , m_hadError(false)
    , m_target(context->platformContext())
{
}

WidgetRenderingContext::~WidgetRenderingContext()
{
}

bool WidgetRenderingContext::paintMozillaWidget(GtkThemeWidgetType type, GtkWidgetState* state, int flags, GtkTextDirection textDirection)
{
    m_hadError = moz_gtk_widget_paint(type, m_target, &m_paintRect, state, flags, textDirection) != MOZ_GTK_SUCCESS;
    return !m_hadError;
}

void WidgetRenderingContext::gtkPaintBox(const IntRect& rect, GtkWidget* widget, GtkStateType stateType, GtkShadowType shadowType, const gchar* detail)
{
    GdkRectangle paintRect = { m_paintRect.x + rect.x(), m_paintRect.y + rect.y(), rect.width(), rect.height() };
    gtk_paint_box(gtk_widget_get_style(widget), m_target, stateType, shadowType, widget,
                  detail, paintRect.x, paintRect.y, paintRect.width, paintRect.height);
}

void WidgetRenderingContext::gtkPaintFocus(const IntRect& rect, GtkWidget* widget, GtkStateType stateType, const gchar* detail)
{
    GdkRectangle paintRect = { m_paintRect.x + rect.x(), m_paintRect.y + rect.y(), rect.width(), rect.height() };
    gtk_paint_focus(gtk_widget_get_style(widget), m_target, stateType, widget,
                    detail, paintRect.x, paintRect.y, paintRect.width, paintRect.height);
}

void WidgetRenderingContext::gtkPaintSlider(const IntRect& rect, GtkWidget* widget, GtkStateType stateType, GtkShadowType shadowType, const gchar* detail, GtkOrientation orientation)
{
    GdkRectangle paintRect = { m_paintRect.x + rect.x(), m_paintRect.y + rect.y(), rect.width(), rect.height() };
    gtk_paint_slider(gtk_widget_get_style(widget), m_target, stateType, shadowType, widget,
                     detail, paintRect.x, paintRect.y, paintRect.width, paintRect.height, orientation);
}

}

#endif // !GTK_API_VERSION_2
