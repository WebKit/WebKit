/*
 * Copyright (C) 2018 Igalia S.L.
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
 */

#include "config.h"
#include "WebKitWebViewDialog.h"

#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

struct _WebKitWebViewDialogPrivate {
#if !GTK_CHECK_VERSION(3, 20, 0)
    GRefPtr<GtkStyleContext> styleContext;
#endif
};

WEBKIT_DEFINE_ABSTRACT_TYPE(WebKitWebViewDialog, webkit_web_view_dialog, GTK_TYPE_EVENT_BOX)

static gboolean webkitWebViewDialogDraw(GtkWidget* widget, cairo_t* cr)
{
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
    cairo_paint(cr);

    if (GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget))) {
        GtkAllocation allocation;
        gtk_widget_get_allocation(child, &allocation);

#if GTK_CHECK_VERSION(3, 20, 0)
        GtkStyleContext* styleContext = gtk_widget_get_style_context(widget);
#else
        GtkStyleContext* styleContext = WEBKIT_WEB_VIEW_DIALOG(widget)->priv->styleContext.get();
#endif
        gtk_render_background(styleContext, cr, allocation.x, allocation.y, allocation.width, allocation.height);
    }

    GTK_WIDGET_CLASS(webkit_web_view_dialog_parent_class)->draw(widget, cr);

    return FALSE;
}

static void webkitWebViewDialogSizeAllocate(GtkWidget* widget, GtkAllocation* allocation)
{
    GTK_WIDGET_CLASS(webkit_web_view_dialog_parent_class)->size_allocate(widget, allocation);

    GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget));
    if (!child)
        return;

    GtkRequisition naturalSize;
    gtk_widget_get_preferred_size(child, 0, &naturalSize);

    GtkAllocation childAllocation;
    gtk_widget_get_allocation(child, &childAllocation);

    childAllocation.x += (allocation->width - naturalSize.width) / 2;
    childAllocation.y += (allocation->height - naturalSize.height) / 2;
    childAllocation.width = naturalSize.width;
    childAllocation.height = naturalSize.height;
    gtk_widget_size_allocate(child, &childAllocation);
}

static void webkitWebViewDialogConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_web_view_dialog_parent_class)->constructed(object);

    gtk_widget_set_app_paintable(GTK_WIDGET(object), TRUE);

#if GTK_CHECK_VERSION(3, 20, 0)
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(object)), GTK_STYLE_CLASS_CSD);
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(object)), GTK_STYLE_CLASS_BACKGROUND);
#else
    WebKitWebViewDialogPrivate* priv = WEBKIT_WEB_VIEW_DIALOG(object)->priv;
    priv->styleContext = adoptGRef(gtk_style_context_new());
    GtkWidgetPath* path = gtk_widget_path_new();
    gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
    gtk_style_context_add_class(priv->styleContext.get(), GTK_STYLE_CLASS_BACKGROUND);
    gtk_style_context_add_class(priv->styleContext.get(), GTK_STYLE_CLASS_TITLEBAR);
    gtk_style_context_add_class(priv->styleContext.get(), GTK_STYLE_CLASS_CSD);
    gtk_style_context_set_path(priv->styleContext.get(), path);
    gtk_widget_path_free(path);
#endif
}

static void webkit_web_view_dialog_class_init(WebKitWebViewDialogClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->constructed = webkitWebViewDialogConstructed;

    GtkWidgetClass* widgetClass = GTK_WIDGET_CLASS(klass);
    widgetClass->draw = webkitWebViewDialogDraw;
    widgetClass->size_allocate = webkitWebViewDialogSizeAllocate;

#if GTK_CHECK_VERSION(3, 20, 0)
    gtk_widget_class_set_css_name(widgetClass, "messagedialog");
#endif
}
