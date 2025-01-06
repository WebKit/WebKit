/*
 * Copyright (C) 2018, 2019, 2021, 2024 Igalia S.L
 * Copyright (C) 2018, 2019 Zodiac Inflight Innovations
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WPEQtView.h"

#include "WPEQtViewLoadRequest.h"
#include "WPEQtViewLoadRequestPrivate.h"
#include "WPEQtViewPrivate.h"
#include "WPEViewQtQuick.h"

#include <QQmlEngine>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>

#include <wtf/glib/GUniquePtr.h>

/*!
  \qmltype WPEView
  \inqmlmodule org.wpewebkit.qtwpe
  \brief A component for displaying web content.

  WPEView is a component for displaying web content which is implemented using native
  APIs on the platforms where this is available, thus it does not necessarily require
  including a full web browser stack as part of the application.

  WPEView provides an API compatible with Qt's QtWebView component. However
  WPEView is limited to Linux platforms supporting EGL KHR extensions. WPEView
  was successfully tested with the EGLFS and Wayland-EGL QPAs.
*/
WPEQtView::WPEQtView(QQuickItem* parent)
    : QQuickItem(parent)
    , d_ptr(new WPEQtViewPrivate)
{
    connect(this, &QQuickItem::windowChanged, this, &WPEQtView::configureWindow);
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    setAcceptTouchEvents(true);
}

WPEQtView::~WPEQtView()
{
    Q_D(WPEQtView);

    if (d->m_webView)
        g_signal_handlers_disconnect_by_data(d->m_webView.get(), this);
}

bool WPEQtView::event(QEvent* ev)
{
    return QQuickItem::event(ev);
}

void WPEQtView::geometryChange(const QRectF& newGeometry, const QRectF&)
{
    Q_D(WPEQtView);

    auto newSize = newGeometry.size().toSize();
    if (newSize.width() <= 0 || newSize.height() <= 0) {
        qWarning() << "WPEQtView::geometryChange(), ignoring invalid resize to new size " << newSize << " keeping old size " << d->m_size;
        return;
    }

    d->m_size = newSize;
    if (!d->m_webView)
        return;

    auto* wpeView = webkit_web_view_get_wpe_view(d->m_webView.get());
    if (auto* wpeToplevel = wpe_view_get_toplevel(wpeView))
        wpe_toplevel_resize(wpeToplevel, d->m_size.width(), d->m_size.height());

}

void WPEQtView::configureWindow()
{
    auto* win = window();
    if (!win)
        return;

    win->setSurfaceType(QWindow::OpenGLSurface);

    if (win->isSceneGraphInitialized())
        createWebView();
    else
        connect(win, &QQuickWindow::sceneGraphInitialized, this, &WPEQtView::createWebView);
}

void WPEQtView::createWebView()
{
    Q_D(WPEQtView);
    RELEASE_ASSERT(!d->m_webView);

    GUniqueOutPtr<GError> error;
    auto* wpeDisplay = wpe_display_qtquick_new();
    if (!wpe_display_connect(wpeDisplay, &error.outPtr())) {
        g_warning("Failed to initialize WPE display: %s", error->message);
        return;
    }

    auto settings = adoptGRef(webkit_settings_new_with_settings("enable-developer-extras", TRUE, nullptr));
    d->m_webView = adoptGRef(WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "display", wpeDisplay,
        "settings", settings.get(), nullptr)));

    auto* wpeView = webkit_web_view_get_wpe_view(d->m_webView.get());
    if (auto* wpeToplevel = wpe_view_get_toplevel(wpeView))
        wpe_toplevel_resize(wpeToplevel, d->m_size.width(), d->m_size.height());

    wpe_view_map(wpeView); // FIXME: unmap when appropriate and implement can_be_mapped if needed.

    if (!wpe_view_qtquick_initialize_rendering(WPE_VIEW_QTQUICK(wpeView), this, &error.outPtr())) {
        g_warning("Failed to create Web view: %s", error->message);
        d->m_webView = nullptr;
        return;
    }

    g_signal_connect_swapped(d->m_webView.get(), "notify::uri", G_CALLBACK(notifyUrlChangedCallback), this);
    g_signal_connect_swapped(d->m_webView.get(), "notify::title", G_CALLBACK(notifyTitleChangedCallback), this);
    g_signal_connect_swapped(d->m_webView.get(), "notify::estimated-load-progress", G_CALLBACK(notifyLoadProgressCallback), this);
    g_signal_connect(d->m_webView.get(), "load-changed", G_CALLBACK(notifyLoadChangedCallback), this);
    g_signal_connect(d->m_webView.get(), "load-failed", G_CALLBACK(notifyLoadFailedCallback), this);

    if (!d->m_url.isEmpty())
        webkit_web_view_load_uri(d->m_webView.get(), d->m_url.toString().toUtf8().constData());
    else if (!d->m_html.isEmpty())
        webkit_web_view_load_html(d->m_webView.get(), d->m_html.toUtf8().constData(), d->m_baseUrl.toString().toUtf8().constData());

    Q_EMIT webViewCreated();
}

void WPEQtView::notifyUrlChangedCallback(WPEQtView* view)
{
    Q_EMIT view->urlChanged();
}

void WPEQtView::notifyTitleChangedCallback(WPEQtView* view)
{
    Q_EMIT view->titleChanged();
}

void WPEQtView::notifyLoadProgressCallback(WPEQtView* view)
{
    Q_EMIT view->loadProgressChanged();
}

void WPEQtView::notifyLoadChangedCallback(WebKitWebView*, WebKitLoadEvent event, WPEQtView* view)
{
    bool statusSet = false;
    WPEQtView::LoadStatus loadStatus;
    switch (event) {
    case WEBKIT_LOAD_STARTED:
        loadStatus = WPEQtView::LoadStatus::LoadStartedStatus;
        statusSet = true;
        break;
    case WEBKIT_LOAD_FINISHED:
        loadStatus = WPEQtView::LoadStatus::LoadSucceededStatus;
        statusSet = !view->errorOccured();
        view->setErrorOccured(false);
        break;
    default:
        break;
    }

    if (statusSet) {
        auto loadRequest = std::make_unique<WPEQtViewLoadRequest>(view->url(), loadStatus, "");
        Q_EMIT view->loadingChanged(loadRequest.get());
    }
}

void WPEQtView::notifyLoadFailedCallback(WebKitWebView*, WebKitLoadEvent, const gchar* failingURI, GError* error, WPEQtView* view)
{
    view->setErrorOccured(true);

    WPEQtView::LoadStatus loadStatus;
    if (g_error_matches(error, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED))
        loadStatus = WPEQtView::LoadStatus::LoadStoppedStatus;
    else
        loadStatus = WPEQtView::LoadStatus::LoadFailedStatus;

    auto loadRequest = std::make_unique<WPEQtViewLoadRequest>(QUrl(QString(failingURI)), loadStatus, error->message);
    Q_EMIT view->loadingChanged(loadRequest.get());
}

void WPEQtView::didUpdateScene()
{
    Q_D(WPEQtView);
    auto* wpeView = webkit_web_view_get_wpe_view(d->m_webView.get());
    wpe_view_qtquick_did_update_scene(WPE_VIEW_QTQUICK(wpeView));
}

QSGNode* WPEQtView::updatePaintNode(QSGNode* node, UpdatePaintNodeData*)
{
    Q_D(WPEQtView);
    if (!d->m_webView)
        return node;

    auto* textureNode = static_cast<QSGSimpleTextureNode*>(node);
    if (!textureNode)
        textureNode = new QSGSimpleTextureNode();

    auto* wpeView = webkit_web_view_get_wpe_view(d->m_webView.get());

    GUniqueOutPtr<GError> error;
    auto texture = wpe_view_qtquick_render_buffer_to_texture(WPE_VIEW_QTQUICK(wpeView), d->m_size, &error.outPtr());
    if (!texture) {
        g_warning("Failed to update WPEQtView paint node in scene graph: %s", error->message);
        return node;
    }

    textureNode->setTexture(texture);
    textureNode->setRect(boundingRect());
    triggerDidUpdateScene();
    return textureNode;
}

QUrl WPEQtView::url() const
{
    Q_D(const WPEQtView);
    if (!d->m_webView)
        return d->m_url;

    const gchar* uri = webkit_web_view_get_uri(d->m_webView.get());
    return uri ? QUrl(QString(uri)) : d->m_url;
}

/*!
  \qmlproperty url WPEView::url

  The URL of currently loaded web page. Changing this will trigger
  loading new content.

  The URL is used as-is. URLs that originate from user input should
  be parsed with QUrl::fromUserInput().

  \note The WPEView does not support loading content through the Qt Resource system.
*/
void WPEQtView::setUrl(const QUrl& url)
{
    Q_D(WPEQtView);
    if (url == d->m_url)
        return;

    d->m_errorOccured = false;
    d->m_url = url;
    if (d->m_webView)
        webkit_web_view_load_uri(d->m_webView.get(), d->m_url.toString().toUtf8().constData());
}

/*!
  \qmlproperty int WPEView::loadProgress
  \readonly

  The current load progress of the web content, represented as
  an integer between 0 and 100.
*/
int WPEQtView::loadProgress() const
{
    Q_D(const WPEQtView);
    if (!d->m_webView)
        return 0;

    return webkit_web_view_get_estimated_load_progress(d->m_webView.get()) * 100;
}

/*!
  \qmlproperty string WPEView::title
  \readonly

  The title of the currently loaded web page.
*/
QString WPEQtView::title() const
{
    Q_D(const WPEQtView);
    if (!d->m_webView)
        return "";

    return webkit_web_view_get_title(d->m_webView.get());
}

/*!
  \qmlproperty bool WPEView::canGoBack
  \readonly

  Holds \c true if it's currently possible to navigate back in the web history.
*/
bool WPEQtView::canGoBack() const
{
    Q_D(const WPEQtView);
    if (!d->m_webView)
        return false;

    return webkit_web_view_can_go_back(d->m_webView.get());
}

/*!
  \qmlproperty bool WPEView::loading
  \readonly

  Holds \c true if the WPEView is currently in the process of loading
  new content, \c false otherwise.

  \sa loadingChanged()
*/

/*!
  \qmlsignal WPEView::loadingChanged(WPEViewLoadRequest loadRequest)

  This signal is emitted when the state of loading the web content changes.
  By handling this signal it's possible, for example, to react to page load
  errors.

  The \a loadRequest parameter holds the \e url and \e status of the request,
  as well as an \e errorString containing an error message for a failed
  request.

  \sa WPEViewLoadRequest
*/
bool WPEQtView::isLoading() const
{
    Q_D(const WPEQtView);
    if (!d->m_webView)
        return false;

    return webkit_web_view_is_loading(d->m_webView.get());
}

/*!
  \qmlproperty bool WPEView::canGoForward
  \readonly

  Holds \c true if it's currently possible to navigate forward in the web history.
*/
bool WPEQtView::canGoForward() const
{
    Q_D(const WPEQtView);
    if (!d->m_webView)
        return false;

    return webkit_web_view_can_go_forward(d->m_webView.get());
}

/*!
  \qmlmethod void WPEView::goBack()

  Navigates back in the web history.
*/
void WPEQtView::goBack()
{
    Q_D(WPEQtView);
    if (d->m_webView)
        webkit_web_view_go_back(d->m_webView.get());
}

/*!
  \qmlmethod void WPEView::goForward()

  Navigates forward in the web history.
*/
void WPEQtView::goForward()
{
    Q_D(WPEQtView);
    if (d->m_webView)
        webkit_web_view_go_forward(d->m_webView.get());
}

/*!
  \qmlmethod void WPEView::reload()

  Reloads the current \l url.
*/
void WPEQtView::reload()
{
    Q_D(WPEQtView);
    if (d->m_webView)
        webkit_web_view_reload(d->m_webView.get());
}

/*!
  \qmlmethod void WPEView::stop()

  Stops loading the current \l url.
*/
void WPEQtView::stop()
{
    Q_D(WPEQtView);
    if (d->m_webView)
        webkit_web_view_stop_loading(d->m_webView.get());
}

/*!
  \qmlmethod void WPEView::loadHtml(string html, url baseUrl)

  Loads the specified \a html content to the web view.

  This method offers a lower-level alternative to the \l url property,
  which references HTML pages via URL.

  External objects such as stylesheets or images referenced in the HTML
  document should be located relative to \a baseUrl. For example, if \a html
  is retrieved from \c http://www.example.com/documents/overview.html, which
  is the base URL, then an image referenced with the relative url, \c diagram.png,
  should be at \c{http://www.example.com/documents/diagram.png}.

  \note The WPEView does not support loading content through the Qt Resource system.

  \sa url
*/
void WPEQtView::loadHtml(const QString& html, const QUrl& baseUrl)
{
    Q_D(WPEQtView);
    d->m_html = html;
    d->m_baseUrl = baseUrl;
    d->m_errorOccured = false;

    if (d->m_webView)
        webkit_web_view_load_html(d->m_webView.get(), html.toUtf8().constData(), baseUrl.toString().toUtf8().constData());
}

struct JavascriptCallbackData {
    JavascriptCallbackData(QJSValue cb, QPointer<WPEQtView> obj)
        : callback(cb)
        , object(obj) { }

    QJSValue callback;
    QPointer<WPEQtView> object;
};

static void jsAsyncReadyCallback(GObject* object, GAsyncResult* result, gpointer userData)
{
    GError* error { nullptr };
    std::unique_ptr<JavascriptCallbackData> data(reinterpret_cast<JavascriptCallbackData*>(userData));
    JSCValue* value = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(object), result, &error);
    if (!value) {
        qWarning("Error running javascript: %s", error->message);
        g_error_free(error);
        return;
    }

    if (data->object.data()) {
        QQmlEngine* engine = qmlEngine(data->object.data());
        if (!engine) {
            qWarning("No JavaScript engine, unable to handle JavaScript callback!");
            g_object_unref(value);
            return;
        }

        QJSValueList args;
        QVariant variant;
        // FIXME: Handle more value types?
        if (jsc_value_is_string(value)) {
            auto* strValue = jsc_value_to_string(value);
            JSCContext* context = jsc_value_get_context(value);
            JSCException* exception = jsc_context_get_exception(context);
            if (exception) {
                qWarning("Error running javascript: %s", jsc_exception_get_message(exception));
                jsc_context_clear_exception(context);
            } else
                variant.setValue(QString::fromUtf8(strValue));
            g_free(strValue);
        }
        args.append(engine->toScriptValue(variant));
        data->callback.call(args);
    }
    g_object_unref(value);
}

/*!
  \qmlmethod void WPEView::runJavaScript(string script, variant callback)

  Runs the specified JavaScript.
  In case a \a callback function is provided, it will be invoked after the \a script
  finished running.

  \badcode
  runJavaScript("document.title", function(result) { console.log(result); });
  \endcode
*/
void WPEQtView::runJavaScript(const QString& script, const QJSValue& callback)
{
    Q_D(WPEQtView);
    std::unique_ptr<JavascriptCallbackData> data = std::make_unique<JavascriptCallbackData>(callback, QPointer<WPEQtView>(this));
    auto utf8Script = script.toUtf8();
    webkit_web_view_evaluate_javascript(d->m_webView.get(), utf8Script.constData(), utf8Script.size(), nullptr, nullptr, nullptr, jsAsyncReadyCallback, data.release());
}

void WPEQtView::mousePressEvent(QMouseEvent* event)
{
    Q_D(WPEQtView);
    forceActiveFocus();
    if (!d->m_webView)
        return;
    auto* wpeView = webkit_web_view_get_wpe_view(d->m_webView.get());
    wpe_view_dispatch_mouse_press_event(WPE_VIEW_QTQUICK(wpeView), event);
}

void WPEQtView::mouseMoveEvent(QMouseEvent* event)
{
    Q_D(WPEQtView);
    if (!d->m_webView)
        return;
    auto* wpeView = webkit_web_view_get_wpe_view(d->m_webView.get());
    wpe_view_dispatch_mouse_move_event(WPE_VIEW_QTQUICK(wpeView), event);
}

void WPEQtView::mouseReleaseEvent(QMouseEvent* event)
{
    Q_D(WPEQtView);
    if (!d->m_webView)
        return;
    auto* wpeView = webkit_web_view_get_wpe_view(d->m_webView.get());
    wpe_view_dispatch_mouse_release_event(WPE_VIEW_QTQUICK(wpeView), event);
}

void WPEQtView::hoverEnterEvent(QHoverEvent* event)
{
    Q_D(WPEQtView);
    if (!d->m_webView)
        return;
    auto* wpeView = webkit_web_view_get_wpe_view(d->m_webView.get());
    wpe_view_dispatch_hover_enter_event(WPE_VIEW_QTQUICK(wpeView), event);
}

void WPEQtView::hoverLeaveEvent(QHoverEvent* event)
{
    Q_D(WPEQtView);
    if (!d->m_webView)
        return;
    auto* wpeView = webkit_web_view_get_wpe_view(d->m_webView.get());
    wpe_view_dispatch_hover_leave_event(WPE_VIEW_QTQUICK(wpeView), event);
}

void WPEQtView::hoverMoveEvent(QHoverEvent* event)
{
    Q_D(WPEQtView);
    if (!d->m_webView)
        return;
    auto* wpeView = webkit_web_view_get_wpe_view(d->m_webView.get());
    wpe_view_dispatch_hover_move_event(WPE_VIEW_QTQUICK(wpeView), event);
}

void WPEQtView::wheelEvent(QWheelEvent* event)
{
    Q_D(WPEQtView);
    if (!d->m_webView)
        return;
    auto* wpeView = webkit_web_view_get_wpe_view(d->m_webView.get());
    wpe_view_dispatch_wheel_event(WPE_VIEW_QTQUICK(wpeView), event);
}

void WPEQtView::keyPressEvent(QKeyEvent* event)
{
    Q_D(WPEQtView);
    if (!d->m_webView)
        return;
    auto* wpeView = webkit_web_view_get_wpe_view(d->m_webView.get());
    wpe_view_dispatch_key_press_event(WPE_VIEW_QTQUICK(wpeView), event);
}

void WPEQtView::keyReleaseEvent(QKeyEvent* event)
{
    Q_D(WPEQtView);
    if (!d->m_webView)
        return;
    auto* wpeView = webkit_web_view_get_wpe_view(d->m_webView.get());
    wpe_view_dispatch_key_release_event(WPE_VIEW_QTQUICK(wpeView), event);
}

void WPEQtView::touchEvent(QTouchEvent* event)
{
    Q_D(WPEQtView);
    if (!d->m_webView)
        return;
    auto* wpeView = webkit_web_view_get_wpe_view(d->m_webView.get());
    wpe_view_dispatch_touch_event(WPE_VIEW_QTQUICK(wpeView), event);
}

WebKitWebView* WPEQtView::webView() const
{
    Q_D(const WPEQtView);
    return d->m_webView.get();
}

bool WPEQtView::errorOccured() const
{
    Q_D(const WPEQtView);
    return d->m_errorOccured;
}

void WPEQtView::setErrorOccured(bool errorOccured)
{
    Q_D(WPEQtView);
    d->m_errorOccured = errorOccured;
}

#include "moc_WPEQtView.cpp"
