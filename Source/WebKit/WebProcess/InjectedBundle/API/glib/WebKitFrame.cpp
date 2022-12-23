/*
 * Copyright (C) 2013 Igalia S.L.
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
#include "WebKitFrame.h"

#include "WebKitFramePrivate.h"
#include "WebKitScriptWorldPrivate.h"
#include "WebKitWebFormManagerPrivate.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <WebCore/Frame.h>
#include <WebCore/JSNode.h>
#include <WebCore/ScriptController.h>
#include <jsc/JSCContextPrivate.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

#if !ENABLE(2022_GLIB_API)
#include "WebKitDOMNodePrivate.h"
#endif

using namespace WebKit;
using namespace WebCore;

/**
 * WebKitFrame:
 *
 * A web page frame.
 *
 * Each `WebKitWebPage` has at least one main frame, and can have any number
 * of subframes.
 *
 * Since: 2.26
 */

struct _WebKitFramePrivate {
    RefPtr<WebFrame> webFrame;

    CString uri;
};

WEBKIT_DEFINE_TYPE(WebKitFrame, webkit_frame, G_TYPE_OBJECT)

static void webkit_frame_class_init(WebKitFrameClass*)
{
}

WebKitFrame* webkitFrameCreate(WebFrame* webFrame)
{
    WebKitFrame* frame = WEBKIT_FRAME(g_object_new(WEBKIT_TYPE_FRAME, NULL));
    frame->priv->webFrame = webFrame;
    return frame;
}

WebFrame* webkitFrameGetWebFrame(WebKitFrame* frame)
{
    return frame->priv->webFrame.get();
}

GRefPtr<JSCValue> webkitFrameGetJSCValueForElementInWorld(WebKitFrame* frame, Element& element, WebKitScriptWorld* world)
{
    Vector<RefPtr<Element>> elements = { RefPtr<Element>(&element) };
    auto values = webkitFrameGetJSCValuesForElementsInWorld(frame, elements, world);
    return values.takeLast();
}

Vector<GRefPtr<JSCValue>> webkitFrameGetJSCValuesForElementsInWorld(WebKitFrame* frame, const Vector<RefPtr<Element>>& elements, WebKitScriptWorld* world)
{
    auto* wkWorld = webkitScriptWorldGetInjectedBundleScriptWorld(world);
    auto jsContext = jscContextGetOrCreate(frame->priv->webFrame->jsContextForWorld(wkWorld));
    JSDOMWindow* globalObject = frame->priv->webFrame->coreFrame()->script().globalObject(wkWorld->coreWorld());
    return elements.map([frame, &jsContext, globalObject](auto& element) -> GRefPtr<JSCValue> {
        JSValueRef jsValue = nullptr;
        {
            JSC::JSLockHolder lock(globalObject);
            jsValue = toRef(globalObject, toJS(globalObject, globalObject, element.get()));
        }
        return jsValue ? jscContextGetOrCreateValue(jsContext.get(), jsValue) : nullptr;
    });
}

/**
 * webkit_frame_get_id:
 * @frame: a #WebKitFrame
 *
 * Gets the process-unique identifier of this #WebKitFrame. No other
 * frame in the same web process will have the same ID; however, frames
 * in other web processes may.
 *
 * Returns: the identifier of @frame
 *
 * Since: 2.26
 */
guint64 webkit_frame_get_id(WebKitFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_FRAME(frame), 0);

    return frame->priv->webFrame->frameID().object().toUInt64();
}

/**
 * webkit_frame_is_main_frame:
 * @frame: a #WebKitFrame
 *
 * Gets whether @frame is the main frame of a #WebKitWebPage
 *
 * Returns: %TRUE if @frame is a main frame or %FALSE otherwise
 *
 * Since: 2.2
 */
gboolean webkit_frame_is_main_frame(WebKitFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_FRAME(frame), FALSE);

    return frame->priv->webFrame->isMainFrame();
}

/**
 * webkit_frame_get_uri:
 * @frame: a #WebKitFrame
 *
 * Gets the current active URI of @frame.
 *
 * Returns: the current active URI of @frame or %NULL if nothing has been
 *    loaded yet.
 *
 * Since: 2.2
 */
const gchar* webkit_frame_get_uri(WebKitFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_FRAME(frame), 0);

    if (frame->priv->uri.isNull())
        frame->priv->uri = frame->priv->webFrame->url().string().utf8();

    return frame->priv->uri.data();
}

#if PLATFORM(GTK) && !USE(GTK4)
/**
 * webkit_frame_get_javascript_global_context: (skip)
 * @frame: a #WebKitFrame
 *
 * Gets the global JavaScript execution context. Use this function to bridge
 * between the WebKit and JavaScriptCore APIs.
 *
 * Returns: (transfer none): the global JavaScript context of @frame
 *
 * Since: 2.2
 *
 * Deprecated: 2.22: Use webkit_frame_get_js_context() instead.
 */
JSGlobalContextRef webkit_frame_get_javascript_global_context(WebKitFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_FRAME(frame), 0);

    return frame->priv->webFrame->jsContext();
}

/**
 * webkit_frame_get_javascript_context_for_script_world: (skip)
 * @frame: a #WebKitFrame
 * @world: a #WebKitScriptWorld
 *
 * Gets the JavaScript execution context of @frame for the given #WebKitScriptWorld.
 *
 * Returns: (transfer none): the JavaScript context of @frame for @world
 *
 * Since: 2.2
 *
 * Deprecated: 2.22: Use webkit_frame_get_js_context_for_script_world() instead.
 */
JSGlobalContextRef webkit_frame_get_javascript_context_for_script_world(WebKitFrame* frame, WebKitScriptWorld* world)
{
    g_return_val_if_fail(WEBKIT_IS_FRAME(frame), 0);
    g_return_val_if_fail(WEBKIT_IS_SCRIPT_WORLD(world), 0);

    return frame->priv->webFrame->jsContextForWorld(webkitScriptWorldGetInjectedBundleScriptWorld(world));
}
#endif

/**
 * webkit_frame_get_js_context:
 * @frame: a #WebKitFrame
 *
 * Get the JavaScript execution context of @frame. Use this function to bridge
 * between the WebKit and JavaScriptCore APIs.
 *
 * Returns: (transfer full): the #JSCContext for the JavaScript execution context of @frame.
 *
 * Since: 2.22
 */
JSCContext* webkit_frame_get_js_context(WebKitFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_FRAME(frame), nullptr);

    return jscContextGetOrCreate(frame->priv->webFrame->jsContext()).leakRef();
}

/**
 * webkit_frame_get_js_context_for_script_world:
 * @frame: a #WebKitFrame
 * @world: a #WebKitScriptWorld
 *
 * Get the JavaScript execution context of @frame for the given #WebKitScriptWorld.
 *
 * Returns: (transfer full): the #JSCContext for the JavaScript execution context of @frame for @world.
 *
 * Since: 2.22
 */
JSCContext* webkit_frame_get_js_context_for_script_world(WebKitFrame* frame, WebKitScriptWorld* world)
{
    g_return_val_if_fail(WEBKIT_IS_FRAME(frame), nullptr);
    g_return_val_if_fail(WEBKIT_IS_SCRIPT_WORLD(world), nullptr);

    return jscContextGetOrCreate(frame->priv->webFrame->jsContextForWorld(webkitScriptWorldGetInjectedBundleScriptWorld(world))).leakRef();
}

#if !ENABLE(2022_GLIB_API)
/**
 * webkit_frame_get_js_value_for_dom_object:
 * @frame: a #WebKitFrame
 * @dom_object: a #WebKitDOMObject
 *
 * Get a #JSCValue referencing the given DOM object. The value is created in the JavaScript execution
 * context of @frame.
 *
 * Returns: (transfer full): the #JSCValue referencing @dom_object.
 *
 * Since: 2.22
 *
 * Deprecated: 2.40
 */
JSCValue* webkit_frame_get_js_value_for_dom_object(WebKitFrame* frame, WebKitDOMObject* domObject)
{
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    return webkit_frame_get_js_value_for_dom_object_in_script_world(frame, domObject, webkit_script_world_get_default());
    G_GNUC_END_IGNORE_DEPRECATIONS;
}

/**
 * webkit_frame_get_js_value_for_dom_object_in_script_world:
 * @frame: a #WebKitFrame
 * @dom_object: a #WebKitDOMObject
 * @world: a #WebKitScriptWorld
 *
 * Get a #JSCValue referencing the given DOM object. The value is created in the JavaScript execution
 * context of @frame for the given #WebKitScriptWorld.
 *
 * Returns: (transfer full): the #JSCValue referencing @dom_object
 *
 * Since: 2.22
 *
 * Deprecated: 2.40
 */
JSCValue* webkit_frame_get_js_value_for_dom_object_in_script_world(WebKitFrame* frame, WebKitDOMObject* domObject, WebKitScriptWorld* world)
{
    g_return_val_if_fail(WEBKIT_IS_FRAME(frame), nullptr);
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    g_return_val_if_fail(WEBKIT_DOM_IS_OBJECT(domObject), nullptr);
    G_GNUC_END_IGNORE_DEPRECATIONS;
    g_return_val_if_fail(WEBKIT_IS_SCRIPT_WORLD(world), nullptr);

    auto* wkWorld = webkitScriptWorldGetInjectedBundleScriptWorld(world);
    auto jsContext = jscContextGetOrCreate(frame->priv->webFrame->jsContextForWorld(wkWorld));
    JSDOMWindow* globalObject = frame->priv->webFrame->coreFrame()->script().globalObject(wkWorld->coreWorld());
    JSValueRef jsValue = nullptr;
    {
        JSC::JSLockHolder lock(globalObject);
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        if (WEBKIT_DOM_IS_NODE(domObject))
            jsValue = toRef(globalObject, toJS(globalObject, globalObject, WebKit::core(WEBKIT_DOM_NODE(domObject))));
        G_GNUC_END_IGNORE_DEPRECATIONS;
    }

    return jsValue ? jscContextGetOrCreateValue(jsContext.get(), jsValue).leakRef() : nullptr;
}
#endif
