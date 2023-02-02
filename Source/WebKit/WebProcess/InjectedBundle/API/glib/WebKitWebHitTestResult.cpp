/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "WebKitWebHitTestResult.h"

#include "InjectedBundleHitTestResult.h"
#include "InjectedBundleScriptWorld.h"
#include "WebKitScriptWorldPrivate.h"
#include "WebKitWebHitTestResultPrivate.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <WebCore/Frame.h>
#include <WebCore/JSNode.h>
#include <WebCore/Node.h>
#include <WebCore/ScriptController.h>
#include <glib/gi18n-lib.h>
#include <jsc/JSCContextPrivate.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

#if !ENABLE(2022_GLIB_API)
#include "WebKitDOMNodePrivate.h"
#endif

using namespace WebKit;
using namespace WebCore;

/**
 * WebKitWebHitTestResult:
 * @See_also: #WebKitHitTestResult, #WebKitWebPage
 *
 * Result of a Hit Test (Web Process Extensions).
 *
 * WebKitWebHitTestResult extends #WebKitHitTestResult to provide information
 * about the #WebKitDOMNode in the coordinates of the Hit Test.
 *
 * Since: 2.8
 */

#if !ENABLE(2022_GLIB_API)
enum {
    PROP_0,

    PROP_NODE
};
#endif

struct _WebKitWebHitTestResultPrivate {
    WeakPtr<Node, WeakPtrImplWithEventTargetData> node;

#if ENABLE(2022_GLIB_API)
    GRefPtr<WebKitHitTestResult> hitTestResult;
#endif // ENABLE(2022_GLIB_API)
};

#if ENABLE(2022_GLIB_API)
struct _WebKitWebHitTestResultClass {
    GObjectClass parent;
};

WEBKIT_DEFINE_FINAL_TYPE(WebKitWebHitTestResult, webkit_web_hit_test_result, G_TYPE_OBJECT)
#else
WEBKIT_DEFINE_TYPE(WebKitWebHitTestResult, webkit_web_hit_test_result, WEBKIT_TYPE_HIT_TEST_RESULT)
#endif

#if !ENABLE(2022_GLIB_API)
static void webkitWebHitTestResultGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitWebHitTestResult* webHitTestResult = WEBKIT_WEB_HIT_TEST_RESULT(object);

    switch (propId) {
    case PROP_NODE:
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        g_value_set_object(value, webkit_web_hit_test_result_get_node(webHitTestResult));
        G_GNUC_END_IGNORE_DEPRECATIONS;
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitWebHitTestResultSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    WebKitWebHitTestResult* webHitTestResult = WEBKIT_WEB_HIT_TEST_RESULT(object);

    switch (propId) {
    case PROP_NODE:
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        if (gpointer node = g_value_get_object(value))
            webHitTestResult->priv->node = core(WEBKIT_DOM_NODE(node));
        G_GNUC_END_IGNORE_DEPRECATIONS;
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}
#endif

static void webkit_web_hit_test_result_class_init(WebKitWebHitTestResultClass* klass)
{
#if !ENABLE(2022_GLIB_API)
    GObjectClass* gObjectClass = G_OBJECT_CLASS(klass);

    gObjectClass->get_property = webkitWebHitTestResultGetProperty;
    gObjectClass->set_property = webkitWebHitTestResultSetProperty;

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    /**
     * WebKitWebHitTestResult:node:
     *
     * The #WebKitDOMNode
     *
     * Deprecated: 2.40
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_NODE,
        g_param_spec_object(
            "node",
            nullptr, nullptr,
            WEBKIT_DOM_TYPE_NODE,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
    G_GNUC_END_IGNORE_DEPRECATIONS;
#endif
}

WebKitWebHitTestResult* webkitWebHitTestResultCreate(const HitTestResult& hitTestResult)
{
    unsigned context = WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT;
    String absoluteLinkURL = hitTestResult.absoluteLinkURL().string();
    if (!absoluteLinkURL.isEmpty())
        context |= WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK;
    String absoluteImageURL = hitTestResult.absoluteImageURL().string();
    if (!absoluteImageURL.isEmpty())
        context |= WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE;
    String absoluteMediaURL = hitTestResult.absoluteMediaURL().string();
    if (!absoluteMediaURL.isEmpty())
        context |= WEBKIT_HIT_TEST_RESULT_CONTEXT_MEDIA;
    if (hitTestResult.isContentEditable())
        context |= WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE;
    if (hitTestResult.scrollbar())
        context |= WEBKIT_HIT_TEST_RESULT_CONTEXT_SCROLLBAR;
    if (hitTestResult.isSelected())
        context |= WEBKIT_HIT_TEST_RESULT_CONTEXT_SELECTION;

    String linkTitle = hitTestResult.titleDisplayString();
    String linkLabel = hitTestResult.textContent();

#if ENABLE(2022_GLIB_API)
    GRefPtr<WebKitHitTestResult> webkitHitTestResult = adoptGRef(WEBKIT_HIT_TEST_RESULT(g_object_new(WEBKIT_TYPE_HIT_TEST_RESULT,
#else
    auto* result = WEBKIT_WEB_HIT_TEST_RESULT(g_object_new(WEBKIT_TYPE_WEB_HIT_TEST_RESULT,
#endif
        "context", context,
        "link-uri", context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK ? absoluteLinkURL.utf8().data() : nullptr,
        "image-uri", context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE ? absoluteImageURL.utf8().data() : nullptr,
        "media-uri", context & WEBKIT_HIT_TEST_RESULT_CONTEXT_MEDIA ? absoluteMediaURL.utf8().data() : nullptr,
        "link-title", !linkTitle.isEmpty() ? linkTitle.utf8().data() : nullptr,
        "link-label", !linkLabel.isEmpty() ? linkLabel.utf8().data() : nullptr,
#if ENABLE(2022_GLIB_API)
        nullptr)));
#else
        "node", kit(hitTestResult.innerNonSharedNode()),
        nullptr));
#endif

#if ENABLE(2022_GLIB_API)
    auto* result = WEBKIT_WEB_HIT_TEST_RESULT(g_object_new(WEBKIT_TYPE_WEB_HIT_TEST_RESULT, nullptr));
    result->priv->hitTestResult = WTFMove(webkitHitTestResult);
    result->priv->node = hitTestResult.innerNonSharedNode();
#endif
    return result;
}

#if ENABLE(2022_GLIB_API)

/**
 * webkit_web_hit_test_result_get_context:
 * @web_hit_test_result: a #WebKitWebHitTestResult
 *
 * Gets the the context flags for the hit test result.
 *
 * Returns: a bitmask of #WebKitHitTestResultContext flags
 */
guint webkit_web_hit_test_result_get_context(WebKitWebHitTestResult* webHitTestResult)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_HIT_TEST_RESULT(webHitTestResult), 0);
    return webkit_hit_test_result_get_context(webHitTestResult->priv->hitTestResult.get());
}

/**
 * webkit_web_hit_test_result_context_is_link:
 * @web_hit_test_result: a #WebWebKitHitTestResult
 *
 * Check whether there is a link element at the hit test position.
 *
 * Checks whether %WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK flag is present in
 * the context flags.
 *
 * Returns: %TRUE if the hit test covers a link element or %FALSE otherwise.
 */
gboolean webkit_web_hit_test_result_context_is_link(WebKitWebHitTestResult* webHitTestResult)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_HIT_TEST_RESULT(webHitTestResult), FALSE);
    return webkit_hit_test_result_context_is_link(webHitTestResult->priv->hitTestResult.get());
}

/**
 * webkit_web_hit_test_result_context_is_image:
 * @web_hit_test_result: a #WebWebKitHitTestResult
 *
 * Check whether there is an image element at the hit test position.
 *
 * Checks whether %WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE flag is present in
 * the context flags.
 *
 * Returns: %TRUE if the hit test covers an image element or %FALSE otherwise.
 */
gboolean webkit_web_hit_test_result_context_is_image(WebKitWebHitTestResult* webHitTestResult)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_HIT_TEST_RESULT(webHitTestResult), FALSE);
    return webkit_hit_test_result_context_is_image(webHitTestResult->priv->hitTestResult.get());
}

/**
 * webkit_web_hit_test_result_context_is_media:
 * @web_hit_test_result: a #WebWebKitHitTestResult
 *
 * Check whether there is a media element at the hit test position.
 *
 * Checks whether %WEBKIT_HIT_TEST_RESULT_CONTEXT_MEDIA flag is present in
 * the context flags.
 *
 * Returns: %TRUE if the hit test covers a media element or %FALSE otherwise.
 */
gboolean webkit_web_hit_test_result_context_is_media(WebKitWebHitTestResult* webHitTestResult)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_HIT_TEST_RESULT(webHitTestResult), FALSE);
    return webkit_hit_test_result_context_is_media(webHitTestResult->priv->hitTestResult.get());
}

/**
 * webkit_web_hit_test_result_context_is_editable:
 * @web_hit_test_result: a #WebWebKitHitTestResult
 *
 * Check whether there is an editable element at the hit test position.
 *
 * Checks whether %WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE flag is present in
 * the context flags.
 *
 * Returns: %TRUE if the hit test covers an editable element or %FALSE otherwise.
 */
gboolean webkit_web_hit_test_result_context_is_editable(WebKitWebHitTestResult* webHitTestResult)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_HIT_TEST_RESULT(webHitTestResult), FALSE);
    return webkit_hit_test_result_context_is_editable(webHitTestResult->priv->hitTestResult.get());
}

/**
 * webkit_web_hit_test_result_context_is_selection:
 * @web_hit_test_result: a #WebWebKitHitTestResult
 *
 * Check whether there is a selected element at the hit test position.
 *
 * Checks whether %WEBKIT_HIT_TEST_RESULT_CONTEXT_SELECTION flag is present in
 * the context flags.
 *
 * Returns: %TRUE if the hit test covers a selected element or %FALSE otherwise.
 */
gboolean webkit_web_hit_test_result_context_is_selection(WebKitWebHitTestResult* webHitTestResult)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_HIT_TEST_RESULT(webHitTestResult), FALSE);
    return webkit_hit_test_result_context_is_selection(webHitTestResult->priv->hitTestResult.get());
}

/**
 * webkit_web_hit_test_result_get_link_uri:
 * @web_hit_test_result: a #WebWebKitHitTestResult
 *
 * Obtains the URI associated with the link element at the hit test position.
 *
 * Returns: the URI of the link element, or %NULL if the hit test does not cover a link element.
 */
const gchar* webkit_web_hit_test_result_get_link_uri(WebKitWebHitTestResult* webHitTestResult)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_HIT_TEST_RESULT(webHitTestResult), nullptr);
    return webkit_hit_test_result_get_link_uri(webHitTestResult->priv->hitTestResult.get());
}

/**
 * webkit_web_hit_test_result_get_link_title:
 * @web_hit_test_result: a #WebWebKitHitTestResult
 *
 * Obtains the title associated with the link element at the hit test position.
 *
 * Returns: the title of the link element, or %NULL if the hit test does not cover a link element
 *    or the link element does not have a title.
 */
const gchar* webkit_web_hit_test_result_get_link_title(WebKitWebHitTestResult* webHitTestResult)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_HIT_TEST_RESULT(webHitTestResult), nullptr);
    return webkit_hit_test_result_get_link_title(webHitTestResult->priv->hitTestResult.get());
}

/**
 * webkit_web_hit_test_result_get_link_label:
 * @web_hit_test_result: a #WebWebKitHitTestResult
 *
 * Obtains the label associated with the link element at the hit test position.
 *
 * Returns: the label of the link element, or %NULL if the hit test does not cover a link element
 *    or the link element does not have a label.
 */
const gchar* webkit_web_hit_test_result_get_link_label(WebKitWebHitTestResult* webHitTestResult)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_HIT_TEST_RESULT(webHitTestResult), nullptr);
    return webkit_hit_test_result_get_link_label(webHitTestResult->priv->hitTestResult.get());
}

/**
 * webkit_web_hit_test_result_get_image_uri:
 * @web_hit_test_result: a #WebWebKitHitTestResult
 *
 * Obtains the URI associated with the image element at the hit test position.
 *
 * Returns: the URI of the image element, or %NULL if the hit test does not cover an image element.
 */
const gchar* webkit_web_hit_test_result_get_image_uri(WebKitWebHitTestResult *webHitTestResult)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_HIT_TEST_RESULT(webHitTestResult), nullptr);
    return webkit_hit_test_result_get_image_uri(webHitTestResult->priv->hitTestResult.get());
}

/**
 * webkit_web_hit_test_result_get_media_uri:
 * @web_hit_test_result: a #WebWebKitHitTestResult
 *
 * Obtains the URI associated with the media element at the hit test position.
 *
 * Returns: the URI of the media element, or %NULL if the hit test does not cover a media element.
 */
const gchar* webkit_web_hit_test_result_get_media_uri(WebKitWebHitTestResult* webHitTestResult)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_HIT_TEST_RESULT(webHitTestResult), nullptr);
    return webkit_hit_test_result_get_media_uri(webHitTestResult->priv->hitTestResult.get());
}

/**
 * webkit_web_hit_test_result_context_is_scrollbar:
 * @web_hit_test_result: a #WebWebKitHitTestResult
 *
 * Check whether there is a scrollbar at the hit test position.
 *
 * Checks whether %WEBKIT_HIT_TEST_RESULT_CONTEXT_SCROLLBAR flag is present in
 * the context flags.
 *
 * Returns: %TRUE if the hit test covers a scrollbar or %FALSE otherwise.
 */
gboolean webkit_web_hit_test_result_context_is_scrollbar(WebKitWebHitTestResult* webHitTestResult)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_HIT_TEST_RESULT(webHitTestResult), FALSE);
    return webkit_hit_test_result_context_is_scrollbar(webHitTestResult->priv->hitTestResult.get());
}

#else
/**
 * webkit_web_hit_test_result_get_node:
 * @hit_test_result: a #WebKitWebHitTestResult
 *
 * Get the #WebKitDOMNode in the coordinates of the Hit Test.
 *
 * Returns: (transfer none): a #WebKitDOMNode
 *
 * Since: 2.8
 *
 * Deprecated: 2.40: Use webkit_web_hit_test_result_get_js_node() instead
 */
WebKitDOMNode* webkit_web_hit_test_result_get_node(WebKitWebHitTestResult* webHitTestResult)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_HIT_TEST_RESULT(webHitTestResult), nullptr);

    return kit(webHitTestResult->priv->node.get());
}
#endif // ENABLE(2022_GLIB_API)

/**
 * webkit_web_hit_test_result_get_js_node:
 * @web_hit_test_result: a #WebKitWebHitTestResult
 * @world: (nullable): a #WebKitScriptWorld, or %NULL to use the default
 *
 * Get the #JSCValue for the DOM node in @world at the coordinates of the Hit Test.
 *
 * Returns: (transfer full) (nullable): a #JSCValue for the DOM node, or %NULL
 *
 * Since: 2.40
 */
JSCValue* webkit_web_hit_test_result_get_js_node(WebKitWebHitTestResult* webHitTestResult, WebKitScriptWorld* world)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_HIT_TEST_RESULT(webHitTestResult), nullptr);
    g_return_val_if_fail(!world || WEBKIT_IS_SCRIPT_WORLD(world), nullptr);

    if (!webHitTestResult->priv->node)
        return nullptr;

    auto* frame = webHitTestResult->priv->node->document().frame();
    if (!frame)
        return nullptr;

    if (!world)
        world = webkit_script_world_get_default();

    auto* wkWorld = webkitScriptWorldGetInjectedBundleScriptWorld(world);
    JSDOMWindow* globalObject = frame->script().globalObject(wkWorld->coreWorld());
    auto jsContext = jscContextGetOrCreate(toGlobalRef(globalObject));
    JSValueRef jsValue = nullptr;
    {
        JSC::JSLockHolder lock(globalObject);
        jsValue = toRef(globalObject, toJS(globalObject, globalObject, webHitTestResult->priv->node.get()));
    }

    return jsValue ? jscContextGetOrCreateValue(jsContext.get(), jsValue).leakRef() : nullptr;
}
