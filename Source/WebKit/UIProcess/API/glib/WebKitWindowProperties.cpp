/*
 * Copyright (C) 2011 Igalia S.L.
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
#include "WebKitWindowProperties.h"

#include "APIDictionary.h"
#include "APINumber.h"
#include "APIURLRequest.h"
#include "WebKitWindowPropertiesPrivate.h"
#include <WebCore/IntRect.h>
#include <WebCore/WindowFeatures.h>
#include <glib/gi18n-lib.h>
#include <wtf/glib/WTFGType.h>

using namespace WebCore;

/**
 * SECTION: WebKitWindowProperties
 * @short_description: Window properties of a #WebKitWebView
 * @title: WebKitWindowProperties
 * @see_also: #WebKitWebView::ready-to-show
 *
 * The content of a #WebKitWebView can request to change certain
 * properties of the window containing the view. This can include the x, y position
 * of the window, the width and height but also if a toolbar,
 * scrollbar, statusbar, locationbar should be visible to the user,
 * and the request to show the #WebKitWebView fullscreen.
 *
 * The #WebKitWebView::ready-to-show signal handler is the proper place
 * to apply the initial window properties. Then you can monitor the
 * #WebKitWindowProperties by connecting to ::notify signal.
 *
 * <informalexample><programlisting>
 * static void ready_to_show_cb (WebKitWebView *web_view, gpointer user_data)
 * {
 *     GtkWidget *window;
 *     WebKitWindowProperties *window_properties;
 *     gboolean visible;
 *
 *     /<!-- -->* Create the window to contain the WebKitWebView *<!-- -->/
 *     window = browser_window_new ();
 *     gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (web_view));
 *     gtk_widget_show (GTK_WIDGET (web_view));
 *
 *     /<!-- -->* Get the WebKitWindowProperties of the web view and monitor it *<!-- -->/
 *     window_properties = webkit_web_view_get_window_properties (web_view);
 *     g_signal_connect (window_properties, "notify::geometry",
 *                       G_CALLBACK (window_geometry_changed), window);
 *     g_signal_connect (window_properties, "notify::toolbar-visible",
 *                       G_CALLBACK (window_toolbar_visibility_changed), window);
 *     g_signal_connect (window_properties, "notify::menubar-visible",
 *                       G_CALLBACK (window_menubar_visibility_changed), window);
 *     ....
 *
 *     /<!-- -->* Apply the window properties before showing the window *<!-- -->/
 *     visible = webkit_window_properties_get_toolbar_visible (window_properties);
 *     browser_window_set_toolbar_visible (BROWSER_WINDOW (window), visible);
 *     visible = webkit_window_properties_get_menubar_visible (window_properties);
 *     browser_window_set_menubar_visible (BROWSER_WINDOW (window), visible);
 *     ....
 *
 *     if (webkit_window_properties_get_fullscreen (window_properties)) {
 *         gtk_window_fullscreen (GTK_WINDOW (window));
 *     } else {
 *         GdkRectangle geometry;
 *
 *         gtk_window_set_resizable (GTK_WINDOW (window),
 *                                   webkit_window_properties_get_resizable (window_properties));
 *         webkit_window_properties_get_geometry (window_properties, &geometry);
 *         gtk_window_move (GTK_WINDOW (window), geometry.x, geometry.y);
 *         gtk_window_resize (GTK_WINDOW (window), geometry.width, geometry.height);
 *     }
 *
 *     gtk_widget_show (window);
 * }
 * </programlisting></informalexample>
 */

enum {
    PROP_0,
#if PLATFORM(GTK)
    PROP_GEOMETRY,
#endif
    PROP_TOOLBAR_VISIBLE,
    PROP_STATUSBAR_VISIBLE,
    PROP_SCROLLBARS_VISIBLE,
    PROP_MENUBAR_VISIBLE,
    PROP_LOCATIONBAR_VISIBLE,
    PROP_RESIZABLE,
    PROP_FULLSCREEN,
    N_PROPERTIES,
};

static GParamSpec* sObjProperties[N_PROPERTIES] = { nullptr, };

struct _WebKitWindowPropertiesPrivate {
#if PLATFORM(GTK)
    GdkRectangle geometry;
#endif

    bool toolbarVisible : 1;
    bool statusbarVisible : 1;
    bool scrollbarsVisible : 1;
    bool menubarVisible : 1;
    bool locationbarVisible : 1;

    bool resizable : 1;
    bool fullscreen : 1;
};

WEBKIT_DEFINE_TYPE(WebKitWindowProperties, webkit_window_properties, G_TYPE_OBJECT)

static void webkitWindowPropertiesGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitWindowProperties* windowProperties = WEBKIT_WINDOW_PROPERTIES(object);

    switch (propId) {
#if PLATFORM(GTK)
    case PROP_GEOMETRY:
        g_value_set_boxed(value, &windowProperties->priv->geometry);
        break;
#endif
    case PROP_TOOLBAR_VISIBLE:
        g_value_set_boolean(value, webkit_window_properties_get_toolbar_visible(windowProperties));
        break;
    case PROP_STATUSBAR_VISIBLE:
        g_value_set_boolean(value, webkit_window_properties_get_statusbar_visible(windowProperties));
        break;
    case PROP_SCROLLBARS_VISIBLE:
        g_value_set_boolean(value, webkit_window_properties_get_scrollbars_visible(windowProperties));
        break;
    case PROP_MENUBAR_VISIBLE:
        g_value_set_boolean(value, webkit_window_properties_get_menubar_visible(windowProperties));
        break;
    case PROP_LOCATIONBAR_VISIBLE:
        g_value_set_boolean(value, webkit_window_properties_get_locationbar_visible(windowProperties));
        break;
    case PROP_RESIZABLE:
        g_value_set_boolean(value, webkit_window_properties_get_resizable(windowProperties));
        break;
    case PROP_FULLSCREEN:
        g_value_set_boolean(value, webkit_window_properties_get_fullscreen(windowProperties));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitWindowPropertiesSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    WebKitWindowProperties* windowProperties = WEBKIT_WINDOW_PROPERTIES(object);

    switch (propId) {
#if PLATFORM(GTK)
    case PROP_GEOMETRY:
        if (GdkRectangle* geometry = static_cast<GdkRectangle*>(g_value_get_boxed(value)))
            windowProperties->priv->geometry = *geometry;
        break;
#endif
    case PROP_TOOLBAR_VISIBLE:
        windowProperties->priv->toolbarVisible = g_value_get_boolean(value);
        break;
    case PROP_STATUSBAR_VISIBLE:
        windowProperties->priv->statusbarVisible = g_value_get_boolean(value);
        break;
    case PROP_SCROLLBARS_VISIBLE:
        windowProperties->priv->scrollbarsVisible = g_value_get_boolean(value);
        break;
    case PROP_MENUBAR_VISIBLE:
        windowProperties->priv->menubarVisible = g_value_get_boolean(value);
        break;
    case PROP_LOCATIONBAR_VISIBLE:
        windowProperties->priv->locationbarVisible = g_value_get_boolean(value);
        break;
    case PROP_RESIZABLE:
        windowProperties->priv->resizable = g_value_get_boolean(value);
        break;
    case PROP_FULLSCREEN:
        windowProperties->priv->fullscreen = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_window_properties_class_init(WebKitWindowPropertiesClass* requestClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(requestClass);
    objectClass->get_property = webkitWindowPropertiesGetProperty;
    objectClass->set_property = webkitWindowPropertiesSetProperty;

    constexpr GParamFlags paramFlags = static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

#if PLATFORM(GTK)
    /**
     * WebKitWebWindowProperties:geometry:
     *
     * The size and position of the window on the screen.
     */
    sObjProperties[PROP_GEOMETRY] =
        g_param_spec_boxed(
            "geometry",
            _("Geometry"),
            _("The size and position of the window on the screen."),
            GDK_TYPE_RECTANGLE,
            paramFlags);
#endif

    /**
     * WebKitWebWindowProperties:toolbar-visible:
     *
     * Whether the toolbar should be visible for the window.
     */
    sObjProperties[PROP_TOOLBAR_VISIBLE] =
        g_param_spec_boolean(
            "toolbar-visible",
            _("Toolbar Visible"),
            _("Whether the toolbar should be visible for the window."),
            TRUE,
            paramFlags);

    /**
     * WebKitWebWindowProperties:statusbar-visible:
     *
     * Whether the statusbar should be visible for the window.
     */
    sObjProperties[PROP_STATUSBAR_VISIBLE] =
        g_param_spec_boolean(
            "statusbar-visible",
            _("Statusbar Visible"),
            _("Whether the statusbar should be visible for the window."),
            TRUE,
            paramFlags);

    /**
     * WebKitWebWindowProperties:scrollbars-visible:
     *
     * Whether the scrollbars should be visible for the window.
     */
    sObjProperties[PROP_SCROLLBARS_VISIBLE] =
        g_param_spec_boolean(
            "scrollbars-visible",
            _("Scrollbars Visible"),
            _("Whether the scrollbars should be visible for the window."),
            TRUE,
            paramFlags);

    /**
     * WebKitWebWindowProperties:menubar-visible:
     *
     * Whether the menubar should be visible for the window.
     */
    sObjProperties[PROP_MENUBAR_VISIBLE] =
        g_param_spec_boolean(
            "menubar-visible",
            _("Menubar Visible"),
            _("Whether the menubar should be visible for the window."),
            TRUE,
            paramFlags);

    /**
     * WebKitWebWindowProperties:locationbar-visible:
     *
     * Whether the locationbar should be visible for the window.
     */
    sObjProperties[PROP_LOCATIONBAR_VISIBLE] =
        g_param_spec_boolean(
            "locationbar-visible",
            _("Locationbar Visible"),
            _("Whether the locationbar should be visible for the window."),
            TRUE,
            paramFlags);
    /**
     * WebKitWebWindowProperties:resizable:
     *
     * Whether the window can be resized.
     */
    sObjProperties[PROP_RESIZABLE] =
        g_param_spec_boolean(
            "resizable",
            _("Resizable"),
            _("Whether the window can be resized."),
            TRUE,
            paramFlags);

    /**
     * WebKitWebWindowProperties:fullscreen:
     *
     * Whether window will be displayed fullscreen.
     */
    sObjProperties[PROP_FULLSCREEN] =
        g_param_spec_boolean(
            "fullscreen",
            _("Fullscreen"),
            _("Whether window will be displayed fullscreen."),
            FALSE,
            paramFlags);

    g_object_class_install_properties(objectClass, N_PROPERTIES, sObjProperties);
}

WebKitWindowProperties* webkitWindowPropertiesCreate()
{
    return WEBKIT_WINDOW_PROPERTIES(g_object_new(WEBKIT_TYPE_WINDOW_PROPERTIES, NULL));
}

#if PLATFORM(GTK)
void webkitWindowPropertiesSetGeometry(WebKitWindowProperties* windowProperties, GdkRectangle* geometry)
{
    if (windowProperties->priv->geometry.x == geometry->x
        && windowProperties->priv->geometry.y == geometry->y
        && windowProperties->priv->geometry.width == geometry->width
        && windowProperties->priv->geometry.height == geometry->height)
        return;
    windowProperties->priv->geometry = *geometry;
    g_object_notify_by_pspec(G_OBJECT(windowProperties), sObjProperties[PROP_GEOMETRY]);
}
#endif

void webkitWindowPropertiesSetToolbarVisible(WebKitWindowProperties* windowProperties, bool toolbarsVisible)
{
    if (windowProperties->priv->toolbarVisible == toolbarsVisible)
        return;
    windowProperties->priv->toolbarVisible = toolbarsVisible;
    g_object_notify_by_pspec(G_OBJECT(windowProperties), sObjProperties[PROP_TOOLBAR_VISIBLE]);
}

void webkitWindowPropertiesSetMenubarVisible(WebKitWindowProperties* windowProperties, bool menuBarVisible)
{
    if (windowProperties->priv->menubarVisible == menuBarVisible)
        return;
    windowProperties->priv->menubarVisible = menuBarVisible;
    g_object_notify_by_pspec(G_OBJECT(windowProperties), sObjProperties[PROP_MENUBAR_VISIBLE]);
}

void webkitWindowPropertiesSetStatusbarVisible(WebKitWindowProperties* windowProperties, bool statusBarVisible)
{
    if (windowProperties->priv->statusbarVisible == statusBarVisible)
        return;
    windowProperties->priv->statusbarVisible = statusBarVisible;
    g_object_notify_by_pspec(G_OBJECT(windowProperties), sObjProperties[PROP_STATUSBAR_VISIBLE]);
}

void webkitWindowPropertiesSetLocationbarVisible(WebKitWindowProperties* windowProperties, bool locationBarVisible)
{
    if (windowProperties->priv->locationbarVisible == locationBarVisible)
        return;
    windowProperties->priv->locationbarVisible = locationBarVisible;
    g_object_notify_by_pspec(G_OBJECT(windowProperties), sObjProperties[PROP_LOCATIONBAR_VISIBLE]);
}

void webkitWindowPropertiesSetScrollbarsVisible(WebKitWindowProperties* windowProperties, bool scrollBarsVisible)
{
    if (windowProperties->priv->scrollbarsVisible == scrollBarsVisible)
        return;
    windowProperties->priv->scrollbarsVisible = scrollBarsVisible;
    g_object_notify_by_pspec(G_OBJECT(windowProperties), sObjProperties[PROP_SCROLLBARS_VISIBLE]);
}

void webkitWindowPropertiesSetResizable(WebKitWindowProperties* windowProperties, bool resizable)
{
    if (windowProperties->priv->resizable == resizable)
        return;
    windowProperties->priv->resizable = resizable;
    g_object_notify_by_pspec(G_OBJECT(windowProperties), sObjProperties[PROP_RESIZABLE]);
}

void webkitWindowPropertiesSetFullscreen(WebKitWindowProperties* windowProperties, bool fullscreen)
{
    if (windowProperties->priv->fullscreen == fullscreen)
        return;
    windowProperties->priv->fullscreen = fullscreen;
    g_object_notify_by_pspec(G_OBJECT(windowProperties), sObjProperties[PROP_FULLSCREEN]);
}

void webkitWindowPropertiesUpdateFromWebWindowFeatures(WebKitWindowProperties* windowProperties, const WindowFeatures& windowFeatures)
{
#if PLATFORM(GTK)
    GdkRectangle geometry = windowProperties->priv->geometry;
    if (windowFeatures.x)
        geometry.x = *windowFeatures.x;
    if (windowFeatures.y)
        geometry.y = *windowFeatures.y;
    if (windowFeatures.width)
        geometry.width = *windowFeatures.width;
    if (windowFeatures.height)
        geometry.height = *windowFeatures.height;
    webkitWindowPropertiesSetGeometry(windowProperties, &geometry);
#endif

    webkitWindowPropertiesSetMenubarVisible(windowProperties, windowFeatures.menuBarVisible);
    webkitWindowPropertiesSetStatusbarVisible(windowProperties, windowFeatures.statusBarVisible);
    webkitWindowPropertiesSetToolbarVisible(windowProperties, windowFeatures.toolBarVisible);
    webkitWindowPropertiesSetLocationbarVisible(windowProperties, windowFeatures.locationBarVisible);
    webkitWindowPropertiesSetScrollbarsVisible(windowProperties, windowFeatures.scrollbarsVisible);
    webkitWindowPropertiesSetResizable(windowProperties, windowFeatures.resizable);
    webkitWindowPropertiesSetFullscreen(windowProperties, windowFeatures.fullscreen);
}

#if PLATFORM(GTK)
/**
 * webkit_window_properties_get_geometry:
 * @window_properties: a #WebKitWindowProperties
 * @geometry: (out): return location for the window geometry
 *
 * Get the geometry the window should have on the screen when shown.
 */
void webkit_window_properties_get_geometry(WebKitWindowProperties* windowProperties, GdkRectangle* geometry)
{
    g_return_if_fail(WEBKIT_IS_WINDOW_PROPERTIES(windowProperties));
    g_return_if_fail(geometry);

    *geometry = windowProperties->priv->geometry;
}
#endif

/**
 * webkit_window_properties_get_toolbar_visible:
 * @window_properties: a #WebKitWindowProperties
 *
 * Get whether the window should have the toolbar visible or not.
 *
 * Returns: %TRUE if toolbar should be visible or %FALSE otherwise.
 */
gboolean webkit_window_properties_get_toolbar_visible(WebKitWindowProperties* windowProperties)
{
    g_return_val_if_fail(WEBKIT_IS_WINDOW_PROPERTIES(windowProperties), TRUE);

    return windowProperties->priv->toolbarVisible;
}

/**
 * webkit_window_properties_get_statusbar_visible:
 * @window_properties: a #WebKitWindowProperties
 *
 * Get whether the window should have the statusbar visible or not.
 *
 * Returns: %TRUE if statusbar should be visible or %FALSE otherwise.
 */
gboolean webkit_window_properties_get_statusbar_visible(WebKitWindowProperties* windowProperties)
{
    g_return_val_if_fail(WEBKIT_IS_WINDOW_PROPERTIES(windowProperties), TRUE);

    return windowProperties->priv->statusbarVisible;
}

/**
 * webkit_window_properties_get_scrollbars_visible:
 * @window_properties: a #WebKitWindowProperties
 *
 * Get whether the window should have the scrollbars visible or not.
 *
 * Returns: %TRUE if scrollbars should be visible or %FALSE otherwise.
 */
gboolean webkit_window_properties_get_scrollbars_visible(WebKitWindowProperties* windowProperties)
{
    g_return_val_if_fail(WEBKIT_IS_WINDOW_PROPERTIES(windowProperties), TRUE);

    return windowProperties->priv->scrollbarsVisible;
}

/**
 * webkit_window_properties_get_menubar_visible:
 * @window_properties: a #WebKitWindowProperties
 *
 * Get whether the window should have the menubar visible or not.
 *
 * Returns: %TRUE if menubar should be visible or %FALSE otherwise.
 */
gboolean webkit_window_properties_get_menubar_visible(WebKitWindowProperties* windowProperties)
{
    g_return_val_if_fail(WEBKIT_IS_WINDOW_PROPERTIES(windowProperties), TRUE);

    return windowProperties->priv->menubarVisible;
}

/**
 * webkit_window_properties_get_locationbar_visible:
 * @window_properties: a #WebKitWindowProperties
 *
 * Get whether the window should have the locationbar visible or not.
 *
 * Returns: %TRUE if locationbar should be visible or %FALSE otherwise.
 */
gboolean webkit_window_properties_get_locationbar_visible(WebKitWindowProperties* windowProperties)
{
    g_return_val_if_fail(WEBKIT_IS_WINDOW_PROPERTIES(windowProperties), TRUE);

    return windowProperties->priv->locationbarVisible;
}

/**
 * webkit_window_properties_get_resizable:
 * @window_properties: a #WebKitWindowProperties
 *
 * Get whether the window should be resizable by the user or not.
 *
 * Returns: %TRUE if the window should be resizable or %FALSE otherwise.
 */
gboolean webkit_window_properties_get_resizable(WebKitWindowProperties* windowProperties)
{
    g_return_val_if_fail(WEBKIT_IS_WINDOW_PROPERTIES(windowProperties), TRUE);

    return windowProperties->priv->resizable;
}

/**
 * webkit_window_properties_get_fullscreen:
 * @window_properties: a #WebKitWindowProperties
 *
 * Get whether the window should be shown in fullscreen state or not.
 *
 * Returns: %TRUE if the window should be fullscreen or %FALSE otherwise.
 */
gboolean webkit_window_properties_get_fullscreen(WebKitWindowProperties* windowProperties)
{
    g_return_val_if_fail(WEBKIT_IS_WINDOW_PROPERTIES(windowProperties), FALSE);

    return windowProperties->priv->fullscreen;
}
