/*
 * Copyright (C) 2018, 2020 Igalia S.L.
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

#include <WebCore/FloatSize.h>
#include <WebCore/GtkVersioning.h>
#include <WebCore/RefPtrCairo.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

struct _WebKitWebViewDialogPrivate {
    GtkWidget* child;
#if USE(GTK4)
    GRefPtr<GtkCssProvider> cssProvider;
#endif
};

#if USE(GTK4)
WEBKIT_DEFINE_ABSTRACT_TYPE(WebKitWebViewDialog, webkit_web_view_dialog, GTK_TYPE_WIDGET)
#else
WEBKIT_DEFINE_ABSTRACT_TYPE(WebKitWebViewDialog, webkit_web_view_dialog, GTK_TYPE_EVENT_BOX)
#endif

#if USE(GTK4)
static void webkitWebViewDialogSnapshot(GtkWidget* widget, GtkSnapshot* snapshot)
{
    WebCore::FloatSize widgetSize(gtk_widget_get_width(widget), gtk_widget_get_height(widget));
    graphene_rect_t rect = GRAPHENE_RECT_INIT(0, 0, widgetSize.width(), widgetSize.height());
    RefPtr<cairo_t> cr = adoptRef(gtk_snapshot_append_cairo(snapshot, &rect));
    cairo_set_operator(cr.get(), CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba(cr.get(), 0, 0, 0, 0.5);
    cairo_paint(cr.get());

    WebKitWebViewDialogPrivate* priv = WEBKIT_WEB_VIEW_DIALOG(widget)->priv;
    if (priv->child)
        gtk_widget_snapshot_child(widget, priv->child, snapshot);
}
#else
static gboolean webkitWebViewDialogDraw(GtkWidget* widget, cairo_t* cr)
{
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
    cairo_paint(cr);

    if (GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget))) {
        GtkAllocation allocation;
        gtk_widget_get_allocation(child, &allocation);

        GtkStyleContext* styleContext = gtk_widget_get_style_context(widget);
        gtk_render_background(styleContext, cr, allocation.x, allocation.y, allocation.width, allocation.height);
    }

    GTK_WIDGET_CLASS(webkit_web_view_dialog_parent_class)->draw(widget, cr);

    return FALSE;
}
#endif

#if USE(GTK4)
static void webkitWebViewDialogSizeAllocate(GtkWidget* widget, int width, int height, int baseline)
#else
static void webkitWebViewDialogSizeAllocate(GtkWidget* widget, GtkAllocation* allocation)
#endif
{
#if USE(GTK4)
    GTK_WIDGET_CLASS(webkit_web_view_dialog_parent_class)->size_allocate(widget, width, height, baseline);
    GtkAllocation allocationStack = { 0, 0, width, height };
    GtkAllocation* allocation = &allocationStack;
    GtkWidget* child = WEBKIT_WEB_VIEW_DIALOG(widget)->priv->child;
#else
    GTK_WIDGET_CLASS(webkit_web_view_dialog_parent_class)->size_allocate(widget, allocation);
    GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget));
#endif

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

#if USE(GTK4)
    gtk_widget_add_css_class(GTK_WIDGET(object), "dialog");
    gtk_widget_add_css_class(GTK_WIDGET(object), "message");
    gtk_widget_add_css_class(GTK_WIDGET(object), "csd");
    gtk_widget_remove_css_class(GTK_WIDGET(object), "background");

    WebKitWebViewDialogPrivate* priv = WEBKIT_WEB_VIEW_DIALOG(object)->priv;
    priv->cssProvider = adoptGRef(gtk_css_provider_new());
    gtk_css_provider_load_from_data(priv->cssProvider.get(), ".dialog-vbox { border-radius: 7px; }", -1);
#else
    gtk_widget_set_app_paintable(GTK_WIDGET(object), TRUE);

    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(object)), GTK_STYLE_CLASS_CSD);
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(object)), GTK_STYLE_CLASS_BACKGROUND);
#endif
}

static void webkit_web_view_dialog_class_init(WebKitWebViewDialogClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->constructed = webkitWebViewDialogConstructed;

    GtkWidgetClass* widgetClass = GTK_WIDGET_CLASS(klass);
#if USE(GTK4)
    widgetClass->snapshot = webkitWebViewDialogSnapshot;
#else
    widgetClass->draw = webkitWebViewDialogDraw;
#endif
    widgetClass->size_allocate = webkitWebViewDialogSizeAllocate;

#if USE(GTK4)
    gtk_widget_class_set_css_name(widgetClass, "window");
#else
    gtk_widget_class_set_css_name(widgetClass, "messagedialog");
#endif
}

#if USE(GTK4)
void webkitWebViewDialogSetChild(WebKitWebViewDialog* dialog, GtkWidget* child)
{
    WebKitWebViewDialogPrivate* priv = dialog->priv;
    g_clear_pointer(&priv->child, gtk_widget_unparent);
    if (child) {
        gtk_widget_set_parent(child, GTK_WIDGET(dialog));
        gtk_widget_add_css_class(child, "background");
        gtk_style_context_add_provider(gtk_widget_get_style_context(child), GTK_STYLE_PROVIDER(priv->cssProvider.get()), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        priv->child = child;
    }
}

GtkWidget* webkitWebViewDialogGetChild(WebKitWebViewDialog* dialog)
{
    return dialog->priv->child;
}
#endif
