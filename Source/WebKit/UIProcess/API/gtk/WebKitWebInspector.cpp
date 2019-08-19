/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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
#include "WebKitWebInspector.h"

#include "WebInspectorProxy.h"
#include "WebInspectorProxyClient.h"
#include "WebKitWebInspectorPrivate.h"
#include <glib/gi18n-lib.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

using namespace WebKit;

/**
 * SECTION: WebKitWebInspector
 * @Short_description: Access to the WebKit inspector
 * @Title: WebKitWebInspector
 *
 * The WebKit Inspector is a graphical tool to inspect and change the
 * content of a #WebKitWebView. It also includes an interactive
 * JavaScript debugger. Using this class one can get a #GtkWidget
 * which can be embedded into an application to show the inspector.
 *
 * The inspector is available when the #WebKitSettings of the
 * #WebKitWebView has set the #WebKitSettings:enable-developer-extras
 * to true, otherwise no inspector is available.
 *
 * <informalexample><programlisting>
 * /<!-- -->* Enable the developer extras *<!-- -->/
 * WebKitSettings *setting = webkit_web_view_get_settings (WEBKIT_WEB_VIEW(my_webview));
 * g_object_set (G_OBJECT(settings), "enable-developer-extras", TRUE, NULL);
 *
 * /<!-- -->* Load some data or reload to be able to inspect the page*<!-- -->/
 * webkit_web_view_load_uri (WEBKIT_WEB_VIEW(my_webview), "http://www.gnome.org");
 *
 * /<!-- -->* Show the inspector *<!-- -->/
 * WebKitWebInspector *inspector = webkit_web_view_get_inspector (WEBKIT_WEB_VIEW(my_webview));
 * webkit_web_inspector_show (WEBKIT_WEB_INSPECTOR(inspector));
 * </programlisting></informalexample>
 *
 */

enum {
    OPEN_WINDOW,
    BRING_TO_FRONT,
    CLOSED,
    ATTACH,
    DETACH,

    LAST_SIGNAL
};

enum {
    PROP_0,

    PROP_INSPECTED_URI,
    PROP_ATTACHED_HEIGHT,
    PROP_CAN_ATTACH
};

struct _WebKitWebInspectorPrivate {
    ~_WebKitWebInspectorPrivate()
    {
        webInspector->setClient(nullptr);
    }

    RefPtr<WebInspectorProxy> webInspector;
    CString inspectedURI;
    unsigned attachedHeight;
    bool canAttach;
};

WEBKIT_DEFINE_TYPE(WebKitWebInspector, webkit_web_inspector, G_TYPE_OBJECT)

static guint signals[LAST_SIGNAL] = { 0, };

static void webkitWebInspectorGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitWebInspector* inspector = WEBKIT_WEB_INSPECTOR(object);

    switch (propId) {
    case PROP_INSPECTED_URI:
        g_value_set_string(value, webkit_web_inspector_get_inspected_uri(inspector));
        break;
    case PROP_ATTACHED_HEIGHT:
        g_value_set_uint(value, webkit_web_inspector_get_attached_height(inspector));
        break;
    case PROP_CAN_ATTACH:
        g_value_set_boolean(value, webkit_web_inspector_get_can_attach(inspector));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_web_inspector_class_init(WebKitWebInspectorClass* findClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(findClass);
    gObjectClass->get_property = webkitWebInspectorGetProperty;

    /**
     * WebKitWebInspector:inspected-uri:
     *
     * The URI that is currently being inspected.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_INSPECTED_URI,
                                    g_param_spec_string("inspected-uri",
                                                        _("Inspected URI"),
                                                        _("The URI that is currently being inspected"),
                                                        0,
                                                        WEBKIT_PARAM_READABLE));
    /**
     * WebKitWebInspector:attached-height:
     *
     * The height that the inspector view should have when it is attached.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ATTACHED_HEIGHT,
                                    g_param_spec_uint("attached-height",
                                                      _("Attached Height"),
                                                      _("The height that the inspector view should have when it is attached"),
                                                      0, G_MAXUINT, 0,
                                                      WEBKIT_PARAM_READABLE));

    /**
     * WebKitWebInspector:can-attach:
     *
     * Whether the @inspector can be attached to the same window that contains
     * the inspected view.
     *
     * Since: 2.8
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_CAN_ATTACH,
        g_param_spec_boolean(
            "can-attach",
            _("Can Attach"),
            _("Whether the inspector can be attached to the same window that contains the inspected view"),
            FALSE,
            WEBKIT_PARAM_READABLE));

    /**
     * WebKitWebInspector::open-window:
     * @inspector: the #WebKitWebInspector on which the signal is emitted
     *
     * Emitted when the inspector is requested to open in a separate window.
     * If this signal is not handled, a #GtkWindow with the inspector will be
     * created and shown, so you only need to handle this signal if you want
     * to use your own window.
     * This signal is emitted after #WebKitWebInspector::detach to show
     * the inspector in a separate window after being detached.
     *
     * To prevent the inspector from being shown you can connect to this
     * signal and simply return %TRUE
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to propagate the event further.
     */
    signals[OPEN_WINDOW] = g_signal_new(
        "open-window",
        G_TYPE_FROM_CLASS(gObjectClass),
        G_SIGNAL_RUN_LAST,
        0,
        g_signal_accumulator_true_handled, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 0);

    /**
     * WebKitWebInspector::bring-to-front:
     * @inspector: the #WebKitWebInspector on which the signal is emitted
     *
     * Emitted when the inspector should be shown.
     *
     * If the inspector is not attached the inspector window should be shown
     * on top of any other windows.
     * If the inspector is attached the inspector view should be made visible.
     * For example, if the inspector view is attached using a tab in a browser
     * window, the browser window should be raised and the tab containing the
     * inspector view should be the active one.
     * In both cases, if this signal is not handled, the default implementation
     * calls gtk_window_present() on the current toplevel #GtkWindow of the
     * inspector view.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to propagate the event further.
     */
    signals[BRING_TO_FRONT] = g_signal_new(
        "bring-to-front",
        G_TYPE_FROM_CLASS(gObjectClass),
        G_SIGNAL_RUN_LAST,
        0,
        g_signal_accumulator_true_handled, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 0);

    /**
     * WebKitWebInspector::closed:
     * @inspector: the #WebKitWebInspector on which the signal is emitted
     *
     * Emitted when the inspector page is closed. If you are using your own
     * inspector window, you should connect to this signal and destroy your
     * window.
     */
    signals[CLOSED] =
        g_signal_new("closed",
                     G_TYPE_FROM_CLASS(gObjectClass),
                     G_SIGNAL_RUN_LAST,
                     0, 0, 0,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    /**
     * WebKitWebInspector::attach:
     * @inspector: the #WebKitWebInspector on which the signal is emitted
     *
     * Emitted when the inspector is requested to be attached to the window
     * where the inspected web view is.
     * If this signal is not handled the inspector view will be automatically
     * attached to the inspected view, so you only need to handle this signal
     * if you want to attach the inspector view yourself (for example, to add
     * the inspector view to a browser tab).
     *
     * To prevent the inspector view from being attached you can connect to this
     * signal and simply return %TRUE.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to propagate the event further.
     */
    signals[ATTACH] = g_signal_new(
        "attach",
        G_TYPE_FROM_CLASS(gObjectClass),
        G_SIGNAL_RUN_LAST,
        0,
        g_signal_accumulator_true_handled, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 0);

    /**
     * WebKitWebInspector::detach:
     * @inspector: the #WebKitWebInspector on which the signal is emitted
     *
     * Emitted when the inspector is requested to be detached from the window
     * it is currently attached to. The inspector is detached when the inspector page
     * is about to be closed, and this signal is emitted right before
     * #WebKitWebInspector::closed, or when the user clicks on the detach button
     * in the inspector view to show the inspector in a separate window. In this case
     * the signal #WebKitWebInspector::open-window is emitted after this one.
     *
     * To prevent the inspector view from being detached you can connect to this
     * signal and simply return %TRUE.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to propagate the event further.
     */
    signals[DETACH] = g_signal_new(
        "detach",
        G_TYPE_FROM_CLASS(gObjectClass),
        G_SIGNAL_RUN_LAST,
        0,
        g_signal_accumulator_true_handled, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 0);
}

class WebKitInspectorClient final : public WebInspectorProxyClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WebKitInspectorClient(WebKitWebInspector* inspector)
        : m_inspector(inspector)
    {
    }

private:
    bool openWindow(WebInspectorProxy&) override
    {
        gboolean returnValue;
        g_signal_emit(m_inspector, signals[OPEN_WINDOW], 0, &returnValue);
        return returnValue;
    }

    void didClose(WebInspectorProxy&) override
    {
        g_signal_emit(m_inspector, signals[CLOSED], 0);
    }

    bool bringToFront(WebInspectorProxy&) override
    {
        gboolean returnValue;
        g_signal_emit(m_inspector, signals[BRING_TO_FRONT], 0, &returnValue);
        return returnValue;
    }

    void inspectedURLChanged(WebInspectorProxy&, const String& url) override
    {
        CString uri = url.utf8();
        if (uri == m_inspector->priv->inspectedURI)
            return;
        m_inspector->priv->inspectedURI = uri;
        g_object_notify(G_OBJECT(m_inspector), "inspected-uri");
    }

    bool attach(WebInspectorProxy&) override
    {
        gboolean returnValue;
        g_signal_emit(m_inspector, signals[ATTACH], 0, &returnValue);
        return returnValue;
    }

    bool detach(WebInspectorProxy&) override
    {
        gboolean returnValue;
        g_signal_emit(m_inspector, signals[DETACH], 0, &returnValue);
        return returnValue;
    }

    void didChangeAttachedHeight(WebInspectorProxy&, unsigned height) override
    {
        if (m_inspector->priv->attachedHeight == height)
            return;
        m_inspector->priv->attachedHeight = height;
        g_object_notify(G_OBJECT(m_inspector), "attached-height");
    }

    void didChangeAttachedWidth(WebInspectorProxy&, unsigned width) override
    {
    }

    void didChangeAttachAvailability(WebInspectorProxy&, bool available) override
    {
        if (m_inspector->priv->canAttach == available)
            return;
        m_inspector->priv->canAttach = available;
        g_object_notify(G_OBJECT(m_inspector), "can-attach");
    }

    WebKitWebInspector* m_inspector;
};

WebKitWebInspector* webkitWebInspectorCreate(WebInspectorProxy* webInspector)
{
    WebKitWebInspector* inspector = WEBKIT_WEB_INSPECTOR(g_object_new(WEBKIT_TYPE_WEB_INSPECTOR, NULL));
    inspector->priv->webInspector = webInspector;
    webInspector->setClient(makeUnique<WebKitInspectorClient>(inspector));
    return inspector;
}

/**
 * webkit_web_inspector_get_web_view:
 * @inspector: a #WebKitWebInspector
 *
 * Get the #WebKitWebViewBase used to display the inspector.
 * This might be %NULL if the inspector hasn't been loaded yet,
 * or it has been closed.
 *
 * Returns: (transfer none): the #WebKitWebViewBase used to display the inspector or %NULL
 */
WebKitWebViewBase* webkit_web_inspector_get_web_view(WebKitWebInspector* inspector)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_INSPECTOR(inspector), 0);

    return WEBKIT_WEB_VIEW_BASE(inspector->priv->webInspector->inspectorView());
}

/**
 * webkit_web_inspector_get_inspected_uri:
 * @inspector: a #WebKitWebInspector
 *
 * Get the URI that is currently being inspected. This can be %NULL if
 * nothing has been loaded yet in the inspected view, if the inspector
 * has been closed or when inspected view was loaded from a HTML string
 * instead of a URI.
 *
 * Returns: the URI that is currently being inspected or %NULL
 */
const char* webkit_web_inspector_get_inspected_uri(WebKitWebInspector* inspector)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_INSPECTOR(inspector), 0);

    return inspector->priv->inspectedURI.data();
}

/**
 * webkit_web_inspector_get_can_attach:
 * @inspector: a #WebKitWebInspector
 *
 * Whether the @inspector can be attached to the same window that contains
 * the inspected view.
 *
 * Returns: %TRUE if there is enough room for the inspector view inside the
 *     window that contains the inspected view, or %FALSE otherwise.
 *
 * Since: 2.8
 */
gboolean webkit_web_inspector_get_can_attach(WebKitWebInspector* inspector)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_INSPECTOR(inspector), FALSE);

    return inspector->priv->canAttach;
}

/**
 * webkit_web_inspector_is_attached:
 * @inspector: a #WebKitWebInspector
 *
 * Whether the @inspector view is currently attached to the same window that contains
 * the inspected view.
 *
 * Returns: %TRUE if @inspector is currently attached or %FALSE otherwise
 */
gboolean webkit_web_inspector_is_attached(WebKitWebInspector* inspector)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_INSPECTOR(inspector), FALSE);

    return inspector->priv->webInspector->isAttached();
}

/**
 * webkit_web_inspector_attach:
 * @inspector: a #WebKitWebInspector
 *
 * Request @inspector to be attached. The signal #WebKitWebInspector::attach
 * will be emitted. If the inspector is already attached it does nothing.
 */
void webkit_web_inspector_attach(WebKitWebInspector* inspector)
{
    g_return_if_fail(WEBKIT_IS_WEB_INSPECTOR(inspector));

    if (inspector->priv->webInspector->isAttached())
        return;
    inspector->priv->webInspector->attach();
}

/**
 * webkit_web_inspector_detach:
 * @inspector: a #WebKitWebInspector
 *
 * Request @inspector to be detached. The signal #WebKitWebInspector::detach
 * will be emitted. If the inspector is already detached it does nothing.
 */
void webkit_web_inspector_detach(WebKitWebInspector* inspector)
{
    g_return_if_fail(WEBKIT_IS_WEB_INSPECTOR(inspector));

    if (!inspector->priv->webInspector->isAttached())
        return;
    inspector->priv->webInspector->detach();
}

/**
 * webkit_web_inspector_show:
 * @inspector: a #WebKitWebInspector
 *
 * Request @inspector to be shown.
 */
void webkit_web_inspector_show(WebKitWebInspector* inspector)
{
    g_return_if_fail(WEBKIT_IS_WEB_INSPECTOR(inspector));

    inspector->priv->webInspector->show();
}

/**
 * webkit_web_inspector_close:
 * @inspector: a #WebKitWebInspector
 *
 * Request @inspector to be closed.
 */
void webkit_web_inspector_close(WebKitWebInspector* inspector)
{
    g_return_if_fail(WEBKIT_IS_WEB_INSPECTOR(inspector));

    inspector->priv->webInspector->close();
}

/**
 * webkit_web_inspector_get_attached_height:
 * @inspector: a #WebKitWebInspector
 *
 * Get the height that the inspector view should have when
 * it's attached. If the inspector view is not attached this
 * returns 0.
 *
 * Returns: the height of the inspector view when attached
 */
guint webkit_web_inspector_get_attached_height(WebKitWebInspector* inspector)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_INSPECTOR(inspector), 0);

    if (!inspector->priv->webInspector->isAttached())
        return 0;
    return inspector->priv->attachedHeight;
}
