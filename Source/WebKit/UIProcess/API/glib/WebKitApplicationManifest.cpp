/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street,
 * Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitApplicationManifestPrivate.h"

#if ENABLE(2022_GLIB_API)
#if PLATFORM(GTK)
#include "GtkUtilities.h"
#elif PLATFORM(WPE)
#include "WebKitColorPrivate.h"
#endif
#include "WebKitImagePrivate.h"
#include "WebKitWebViewPrivate.h"
#include <WebCore/FloatSize.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ShareableBitmap.h>
#include <optional>
#include <wtf/JSONValues.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GWeakPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

/**
 * WebKitApplicationManifest:
 *
 * Represents the application manifest associated with a web page.
 *
 * An [class@ApplicationManifest] contains the processed manifest metadata for a
 * page, including its name, launch URI, display mode, and icon descriptors.
 * Icon data is not loaded as part of manifest processing.
 *
 * Since: 2.56
 */

/**
 * WebKitApplicationManifestIcon:
 *
 * Represents an icon declared in an [class@ApplicationManifest].
 *
 * The icon URI and metadata are resolved according to the web application
 * manifest specification. The icon implements [iface@Gio.Icon] and
 * [iface@Gio.LoadableIcon], so its resource is loaded on demand using the page
 * network session.
 *
 * Since: 2.56
 */

/**
 * WebKitApplicationManifestShortcut:
 *
 * Represents a shortcut declared in an [class@ApplicationManifest].
 *
 * A shortcut contains a name, a resolved URI, and optional icon descriptors.
 * Shortcut icon data is loaded on demand in the same way as other
 * [class@ApplicationManifestIcon] resources.
 *
 * Since: 2.56
 */

enum {
    PROP_0,
    PROP_NAME,
    PROP_SHORT_NAME,
    PROP_DESCRIPTION,
    PROP_START_URI,
    PROP_SCOPE,
    PROP_MANIFEST_URI,
    PROP_ID,
    PROP_DISPLAY_MODE,
    N_PROPERTIES,
};

#if PLATFORM(GTK)
using ApplicationManifestColor = GdkRGBA;
#elif PLATFORM(WPE)
using ApplicationManifestColor = WebKitColor;
#endif

static std::optional<ApplicationManifestColor> applicationManifestColor(const WebCore::Color& webCoreColor)
{
    if (!webCoreColor.isValid())
        return std::nullopt;

    ApplicationManifestColor color;
#if PLATFORM(GTK)
    color = WebKit::colorToGdkRGBA(webCoreColor);
#elif PLATFORM(WPE)
    webkitColorFillFromWebCoreColor(webCoreColor, &color);
#endif
    return color;
}

struct _WebKitApplicationManifestPrivate {
    String rawJSON;
    GRefPtr<GVariant> fullManifest;
    CString name;
    CString shortName;
    CString description;
    CString startURI;
    CString scope;
    CString manifestURI;
    CString id;
    CString displayMode;
    std::optional<ApplicationManifestColor> backgroundColor;
    std::optional<ApplicationManifestColor> backgroundColorDark;
    std::optional<ApplicationManifestColor> themeColor;
    std::optional<ApplicationManifestColor> themeColorDark;
    GRefPtr<GPtrArray> icons;
    GRefPtr<GPtrArray> shortcuts;
};

struct _WebKitApplicationManifestIconPrivate {
    CString uri;
    GRefPtr<GPtrArray> sizes;
    CString mimeType;
    GWeakPtr<WebKitWebView> webView;
    bool isMaskable { false };
    bool isMonochrome { false };
};

struct _WebKitApplicationManifestShortcutPrivate {
    CString name;
    CString uri;
    GRefPtr<GPtrArray> icons;
};

static void webkitApplicationManifestIconGIconInterfaceInit(GIconIface*);
static void webkitApplicationManifestIconGLoadableIconInterfaceInit(GLoadableIconIface*);

WEBKIT_DEFINE_FINAL_TYPE(WebKitApplicationManifest, webkit_application_manifest, G_TYPE_OBJECT, GObject)
WEBKIT_DEFINE_FINAL_TYPE_WITH_CODE(
    WebKitApplicationManifestIcon, webkit_application_manifest_icon, G_TYPE_OBJECT, GObject,
    G_IMPLEMENT_INTERFACE(G_TYPE_ICON, webkitApplicationManifestIconGIconInterfaceInit)
    G_IMPLEMENT_INTERFACE(G_TYPE_LOADABLE_ICON, webkitApplicationManifestIconGLoadableIconInterfaceInit))
WEBKIT_DEFINE_FINAL_TYPE(WebKitApplicationManifestShortcut, webkit_application_manifest_shortcut, G_TYPE_OBJECT, GObject)

static std::array<GParamSpec*, N_PROPERTIES> sObjProperties;

static void webkitApplicationManifestGetProperty(GObject* object, guint propID, GValue* value, GParamSpec* paramSpec)
{
    auto* manifest = WEBKIT_APPLICATION_MANIFEST(object);

    switch (propID) {
    case PROP_NAME:
        g_value_set_string(value, webkit_application_manifest_get_name(manifest));
        break;
    case PROP_SHORT_NAME:
        g_value_set_string(value, webkit_application_manifest_get_short_name(manifest));
        break;
    case PROP_DESCRIPTION:
        g_value_set_string(value, webkit_application_manifest_get_description(manifest));
        break;
    case PROP_START_URI:
        g_value_set_string(value, webkit_application_manifest_get_start_uri(manifest));
        break;
    case PROP_SCOPE:
        g_value_set_string(value, webkit_application_manifest_get_scope(manifest));
        break;
    case PROP_MANIFEST_URI:
        g_value_set_string(value, webkit_application_manifest_get_manifest_uri(manifest));
        break;
    case PROP_ID:
        g_value_set_string(value, webkit_application_manifest_get_id(manifest));
        break;
    case PROP_DISPLAY_MODE:
        g_value_set_string(value, webkit_application_manifest_get_display_mode(manifest));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, paramSpec);
    }
}

static void webkit_application_manifest_class_init(WebKitApplicationManifestClass* manifestClass)
{
    auto* objectClass = G_OBJECT_CLASS(manifestClass);
    objectClass->get_property = webkitApplicationManifestGetProperty;

    /**
     * WebKitApplicationManifest:name: (getter get_name):
     *
     * The application name declared in the manifest.
     *
     * Since: 2.56
     */
    sObjProperties[PROP_NAME] = g_param_spec_string("name", nullptr, nullptr, nullptr, WEBKIT_PARAM_READABLE);
    /**
     * WebKitApplicationManifest:short-name: (getter get_short_name):
     *
     * The short application name declared in the manifest.
     *
     * Since: 2.56
     */
    sObjProperties[PROP_SHORT_NAME] = g_param_spec_string("short-name", nullptr, nullptr, nullptr, WEBKIT_PARAM_READABLE);
    /**
     * WebKitApplicationManifest:description: (getter get_description):
     *
     * The application description declared in the manifest.
     *
     * Since: 2.56
     */
    sObjProperties[PROP_DESCRIPTION] = g_param_spec_string("description", nullptr, nullptr, nullptr, WEBKIT_PARAM_READABLE);
    /**
     * WebKitApplicationManifest:start-uri: (getter get_start_uri):
     *
     * The resolved start URI of the application.
     *
     * Since: 2.56
     */
    sObjProperties[PROP_START_URI] = g_param_spec_string("start-uri", nullptr, nullptr, nullptr, WEBKIT_PARAM_READABLE);
    /**
     * WebKitApplicationManifest:scope: (getter get_scope):
     *
     * The resolved navigation scope of the application.
     *
     * Since: 2.56
     */
    sObjProperties[PROP_SCOPE] = g_param_spec_string("scope", nullptr, nullptr, nullptr, WEBKIT_PARAM_READABLE);
    /**
     * WebKitApplicationManifest:manifest-uri: (getter get_manifest_uri):
     *
     * The URI from which the manifest was loaded.
     *
     * Since: 2.56
     */
    sObjProperties[PROP_MANIFEST_URI] = g_param_spec_string("manifest-uri", nullptr, nullptr, nullptr, WEBKIT_PARAM_READABLE);
    /**
     * WebKitApplicationManifest:id: (getter get_id):
     *
     * The resolved application identifier declared in the manifest.
     *
     * Since: 2.56
     */
    sObjProperties[PROP_ID] = g_param_spec_string("id", nullptr, nullptr, nullptr, WEBKIT_PARAM_READABLE);
    /**
     * WebKitApplicationManifest:display-mode: (getter get_display_mode):
     *
     * The requested display mode: "browser", "minimal-ui", "standalone", or
     * "fullscreen".
     *
     * Since: 2.56
     */
    sObjProperties[PROP_DISPLAY_MODE] = g_param_spec_string("display-mode", nullptr, nullptr, nullptr, WEBKIT_PARAM_READABLE);

    g_object_class_install_properties(objectClass, N_PROPERTIES, sObjProperties.data());
}

static void webkit_application_manifest_icon_class_init(WebKitApplicationManifestIconClass* iconClass)
{
}

static void webkit_application_manifest_shortcut_class_init(WebKitApplicationManifestShortcutClass* shortcutClass)
{
}

static GRefPtr<GVariant> variantFromJSONValue(JSON::Value& value)
{
    using enum JSON::Value::Type;
    switch (value.type()) {
    case Null:
        return g_variant_new_maybe(G_VARIANT_TYPE_VARIANT, nullptr);
    case Boolean:
        return g_variant_new_boolean(*value.asBoolean());
    case Double:
        return g_variant_new_double(*value.asDouble());
    case Integer:
        return g_variant_new_double(*value.asInteger());
    case String:
        return g_variant_new_string(value.asString().utf8().data());
    case Object: {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
        auto object = value.asObject();
        for (const auto& key : object->keys()) {
            auto child = object->getValue(key);
            auto childVariant = variantFromJSONValue(*child);
            g_variant_builder_add(&builder, "{sv}", key.utf8().data(), childVariant.get());
        }
        return g_variant_builder_end(&builder);
    }
    case Array: {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("av"));
        auto array = value.asArray();
        for (size_t i = 0; i < array->length(); ++i) {
            auto childVariant = variantFromJSONValue(array->get(i));
            g_variant_builder_add(&builder, "v", childVariant.get());
        }
        return g_variant_builder_end(&builder);
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
}

#if ENABLE(APPLICATION_MANIFEST)
static const char* displayMode(WebCore::ApplicationManifest::Display display)
{
    using enum WebCore::ApplicationManifest::Display;
    switch (display) {
    case Browser:
        return "browser";
    case MinimalUI:
        return "minimal-ui";
    case Standalone:
        return "standalone";
    case Fullscreen:
        return "fullscreen";
    }
    return nullptr;
}

static GRefPtr<WebKitApplicationManifestIcon> webkitApplicationManifestIconCreate(WebKitWebView* webView, const WebCore::ApplicationManifest::Icon& icon)
{
    GRefPtr<WebKitApplicationManifestIcon> result = adoptGRef(WEBKIT_APPLICATION_MANIFEST_ICON(g_object_new(WEBKIT_TYPE_APPLICATION_MANIFEST_ICON, nullptr)));
    result->priv->uri = icon.src.string().utf8();
    result->priv->mimeType = icon.type.utf8();
    result->priv->webView = GWeakPtr { webView };
    result->priv->isMaskable = icon.purposes.contains(WebCore::ApplicationManifest::Icon::Purpose::Maskable);
    result->priv->isMonochrome = icon.purposes.contains(WebCore::ApplicationManifest::Icon::Purpose::Monochrome);

    result->priv->sizes = adoptGRef(g_ptr_array_new_with_free_func(g_free));
    for (const auto& size : icon.sizes)
        g_ptr_array_add(result->priv->sizes.get(), g_strdup(size.utf8().data()));
    g_ptr_array_add(result->priv->sizes.get(), nullptr);

    return result;
}

static void webkitApplicationManifestObjectDestroy(gpointer data)
{
    if (data)
        g_object_unref(data);
}

static GRefPtr<WebKitApplicationManifestShortcut> webkitApplicationManifestShortcutCreate(WebKitWebView* webView, const WebCore::ApplicationManifest::Shortcut& shortcut)
{
    GRefPtr<WebKitApplicationManifestShortcut> result = adoptGRef(WEBKIT_APPLICATION_MANIFEST_SHORTCUT(g_object_new(WEBKIT_TYPE_APPLICATION_MANIFEST_SHORTCUT, nullptr)));
    result->priv->name = shortcut.name.utf8();
    result->priv->uri = shortcut.url.string().utf8();
    result->priv->icons = adoptGRef(g_ptr_array_new_with_free_func(webkitApplicationManifestObjectDestroy));
    for (const auto& icon : shortcut.icons)
        g_ptr_array_add(result->priv->icons.get(), webkitApplicationManifestIconCreate(webView, icon).leakRef());
    g_ptr_array_add(result->priv->icons.get(), nullptr);
    return result;
}
#endif

static GRefPtr<WebKitImage> webkitApplicationManifestIconCreateImage(Ref<WebCore::ShareableBitmap>&& bitmap)
{
    if (bitmap->bytesPerRow() > std::numeric_limits<guint>::max())
        return nullptr;

    bitmap->ref();
    GRefPtr<GBytes> imageBytes = adoptGRef(g_bytes_new_with_free_func(bitmap->span().data(), bitmap->span().size(), [](void* data) {
        static_cast<WebCore::ShareableBitmap*>(data)->deref();
    }, bitmap.ptr()));
    return adoptGRef(webkitImageNew(bitmap->size().width(), bitmap->size().height(), bitmap->bytesPerRow(), WTF::move(imageBytes)));
}

static guint webkitApplicationManifestIconHash(GIcon* icon)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST_ICON(icon), 0);
    return g_str_hash(WEBKIT_APPLICATION_MANIFEST_ICON(icon)->priv->uri.data());
}

static gboolean webkitApplicationManifestIconEqual(GIcon* icon1, GIcon* icon2)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST_ICON(icon1), FALSE);

    if (!WEBKIT_IS_APPLICATION_MANIFEST_ICON(icon2))
        return FALSE;
    return !g_strcmp0(WEBKIT_APPLICATION_MANIFEST_ICON(icon1)->priv->uri.data(), WEBKIT_APPLICATION_MANIFEST_ICON(icon2)->priv->uri.data());
}

static void webkitApplicationManifestIconLoadAsync(GLoadableIcon* icon, int size, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    GRefPtr<GTask> task = adoptGRef(g_task_new(icon, cancellable, callback, userData));
    if (g_task_return_error_if_cancelled(task.get()))
        return;

    auto* manifestIcon = WEBKIT_APPLICATION_MANIFEST_ICON(icon);
    GRefPtr<WebKitWebView> webView = manifestIcon->priv->webView ? manifestIcon->priv->webView.get() : nullptr;
    if (!webView) {
        g_task_return_new_error(task.get(), WEBKIT_APPLICATION_MANIFEST_ERROR, WEBKIT_APPLICATION_MANIFEST_ERROR_WEB_VIEW_CLOSED, "The web view that loaded the manifest no longer exists");
        return;
    }

    constexpr size_t maximumBytesFromNetwork = 10 * 1024 * 1024;
    std::optional<WebCore::FloatSize> sizeConstraint;
    if (size > 0)
        sizeConstraint = WebCore::FloatSize { static_cast<float>(size), static_cast<float>(size) };
    WebCore::ResourceRequest request { URL { String::fromUTF8(manifestIcon->priv->uri.data()) } };
    webkitWebViewGetPage(webView.get()).loadAndDecodeImage(WTF::move(request), sizeConstraint, maximumBytesFromNetwork, [task = WTF::move(task), webView = WTF::move(webView)](Expected<Ref<WebCore::ShareableBitmap>, WebCore::ResourceError>&& result) mutable {
        if (g_task_return_error_if_cancelled(task.get()))
            return;

        if (!result) {
            auto message = result.error().localizedDescription().utf8();
            g_task_return_new_error(task.get(), WEBKIT_APPLICATION_MANIFEST_ERROR, WEBKIT_APPLICATION_MANIFEST_ERROR_ICON_LOAD_FAILED, "%s", message.data());
            return;
        }

        GRefPtr<WebKitImage> image = webkitApplicationManifestIconCreateImage(WTF::move(result.value()));
        if (!image) {
            g_task_return_new_error(task.get(), WEBKIT_APPLICATION_MANIFEST_ERROR, WEBKIT_APPLICATION_MANIFEST_ERROR_ICON_LOAD_FAILED, "Failed to create image from manifest icon");
            return;
        }

        GUniqueOutPtr<GError> error;
        GRefPtr<GInputStream> stream = adoptGRef(g_loadable_icon_load(G_LOADABLE_ICON(image.get()), 0, nullptr, nullptr, &error.outPtr()));
        if (error)
            g_task_return_error(task.get(), error.release());
        else
            g_task_return_pointer(task.get(), stream.leakRef(), g_object_unref);
    });
}

static GInputStream* webkitApplicationManifestIconLoadFinish(GLoadableIcon* icon, GAsyncResult* result, char** type, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST_ICON(icon), nullptr);

    if (type)
        *type = g_strdup("image/png");
    return G_INPUT_STREAM(g_task_propagate_pointer(G_TASK(result), error));
}

struct WebKitApplicationManifestIconLoadData {
    GMainLoop* mainLoop { nullptr };
    GRefPtr<GInputStream> stream;
    GUniqueOutPtr<GError> error;
    bool completed { false };
};

static GInputStream* webkitApplicationManifestIconLoad(GLoadableIcon* icon, int size, char** type, GCancellable* cancellable, GError** error)
{
    GRefPtr<GMainLoop> mainLoop = adoptGRef(g_main_loop_new(nullptr, FALSE));
    WebKitApplicationManifestIconLoadData data;
    data.mainLoop = mainLoop.get();
    webkitApplicationManifestIconLoadAsync(icon, size, cancellable, [](GObject* source, GAsyncResult* result, gpointer userData) {
        auto* data = static_cast<WebKitApplicationManifestIconLoadData*>(userData);
        data->stream = adoptGRef(webkitApplicationManifestIconLoadFinish(G_LOADABLE_ICON(source), result, nullptr, &data->error.outPtr()));
        data->completed = true;
        g_main_loop_quit(data->mainLoop);
    }, &data);

    if (!data.completed)
        g_main_loop_run(mainLoop.get());

    if (data.error) {
        g_propagate_error(error, data.error.release());
        return nullptr;
    }
    if (type)
        *type = g_strdup("image/png");
    return data.stream.leakRef();
}

static void webkitApplicationManifestIconGIconInterfaceInit(GIconIface* iface)
{
    iface->hash = webkitApplicationManifestIconHash;
    iface->equal = webkitApplicationManifestIconEqual;
}

static void webkitApplicationManifestIconGLoadableIconInterfaceInit(GLoadableIconIface* iface)
{
    iface->load = webkitApplicationManifestIconLoad;
    iface->load_async = webkitApplicationManifestIconLoadAsync;
    iface->load_finish = webkitApplicationManifestIconLoadFinish;
}

#if ENABLE(APPLICATION_MANIFEST)
WebKitApplicationManifest* webkitApplicationManifestCreate(WebKitWebView* webView, const WebCore::ApplicationManifest& coreManifest)
{
    GRefPtr<WebKitApplicationManifest> manifest = adoptGRef(WEBKIT_APPLICATION_MANIFEST(g_object_new(WEBKIT_TYPE_APPLICATION_MANIFEST, nullptr)));
    auto* priv = manifest->priv;
    priv->rawJSON = coreManifest.rawJSON;
    priv->name = coreManifest.name.utf8();
    priv->shortName = coreManifest.shortName.utf8();
    priv->description = coreManifest.description.utf8();
    priv->startURI = coreManifest.startURL.string().utf8();
    priv->scope = coreManifest.scope.string().utf8();
    priv->manifestURI = coreManifest.manifestURL.string().utf8();
    priv->id = coreManifest.id.string().utf8();
    priv->displayMode = displayMode(coreManifest.display);
    priv->backgroundColor = applicationManifestColor(coreManifest.backgroundColor);
    priv->backgroundColorDark = applicationManifestColor(coreManifest.backgroundColorDark);
    priv->themeColor = applicationManifestColor(coreManifest.themeColor);
    priv->themeColorDark = applicationManifestColor(coreManifest.themeColorDark);
    priv->icons = adoptGRef(g_ptr_array_new_with_free_func(webkitApplicationManifestObjectDestroy));
    for (const auto& icon : coreManifest.icons)
        g_ptr_array_add(priv->icons.get(), webkitApplicationManifestIconCreate(webView, icon).leakRef());
    g_ptr_array_add(priv->icons.get(), nullptr);
    priv->shortcuts = adoptGRef(g_ptr_array_new_with_free_func(webkitApplicationManifestObjectDestroy));
    for (const auto& shortcut : coreManifest.shortcuts)
        g_ptr_array_add(priv->shortcuts.get(), webkitApplicationManifestShortcutCreate(webView, shortcut).leakRef());
    g_ptr_array_add(priv->shortcuts.get(), nullptr);
    return manifest.leakRef();
}
#endif

/**
 * webkit_application_manifest_error_quark:
 *
 * Gets the quark for the domain of application manifest errors.
 *
 * Returns: application manifest error domain.
 *
 * Since: 2.56
 */
GQuark webkit_application_manifest_error_quark(void)
{
    return g_quark_from_static_string("WebKitApplicationManifestError");
}

/**
 * webkit_application_manifest_get_full_manifest:
 * @manifest: an [class@ApplicationManifest]
 *
 * Get all the unprocessed values in @manifest.
 *
 * The returned #GVariant is an immutable dictionary with type `a{sv}`. JSON
 * objects are represented by `a{sv}`, arrays by `av`, strings by `s`,
 * booleans by `b`, numbers by `d`, and null by `mv`.
 * A #GVariantDict can be initialized from the returned value for convenient
 * lookups.
 *
 * Returns: (transfer none): the full manifest as a #GVariant dictionary.
 *
 * Since: 2.56
 */
GVariant* webkit_application_manifest_get_full_manifest(WebKitApplicationManifest* manifest)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST(manifest), nullptr);

    auto* priv = manifest->priv;
    if (!priv->fullManifest) {
        auto fullManifest = JSON::Value::parseJSON(priv->rawJSON);
        if (!fullManifest || fullManifest->type() != JSON::Value::Type::Object)
            fullManifest = JSON::Object::create();
        priv->fullManifest = variantFromJSONValue(*fullManifest);
    }
    return priv->fullManifest.get();
}

/**
 * webkit_application_manifest_get_name: (get-property name):
 * @manifest: an [class@ApplicationManifest]
 *
 * Get the application name.
 *
 * Returns: (nullable): the manifest name.
 *
 * Since: 2.56
 */
const gchar* webkit_application_manifest_get_name(WebKitApplicationManifest* manifest)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST(manifest), nullptr);
    return manifest->priv->name.data();
}

/**
 * webkit_application_manifest_get_short_name: (get-property short-name):
 * @manifest: an ApplicationManifest
 *
 * Get the short application name.
 *
 * Returns: (nullable): the manifest short name.
 *
 * Since: 2.56
 */
const gchar* webkit_application_manifest_get_short_name(WebKitApplicationManifest* manifest)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST(manifest), nullptr);
    return manifest->priv->shortName.data();
}

/**
 * webkit_application_manifest_get_description: (get-property description):
 * @manifest: an ApplicationManifest
 *
 * Get the application description.
 *
 * Returns: (nullable): the manifest description.
 *
 * Since: 2.56
 */
const gchar* webkit_application_manifest_get_description(WebKitApplicationManifest* manifest)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST(manifest), nullptr);
    return manifest->priv->description.data();
}

/**
 * webkit_application_manifest_get_start_uri: (get-property start-uri):
 * @manifest: an ApplicationManifest
 *
 * Get the resolved start URI of the application.
 *
 * Returns: (nullable): the application start URI.
 *
 * Since: 2.56
 */
const gchar* webkit_application_manifest_get_start_uri(WebKitApplicationManifest* manifest)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST(manifest), nullptr);
    return manifest->priv->startURI.data();
}

/**
 * webkit_application_manifest_get_scope: (get-property scope):
 * @manifest: an ApplicationManifest
 *
 * Get the resolved navigation scope of the application.
 *
 * Returns: (nullable): the application scope.
 *
 * Since: 2.56
 */
const gchar* webkit_application_manifest_get_scope(WebKitApplicationManifest* manifest)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST(manifest), nullptr);
    return manifest->priv->scope.data();
}

/**
 * webkit_application_manifest_get_manifest_uri: (get-property manifest-uri):
 * @manifest: an ApplicationManifest
 *
 * Get the URI from which the manifest was loaded.
 *
 * Returns: (nullable): the manifest URI.
 *
 * Since: 2.56
 */
const gchar* webkit_application_manifest_get_manifest_uri(WebKitApplicationManifest* manifest)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST(manifest), nullptr);
    return manifest->priv->manifestURI.data();
}

/**
 * webkit_application_manifest_get_id: (get-property id):
 * @manifest: an ApplicationManifest
 *
 * Get the resolved application identifier.
 *
 * Returns: (nullable): the application identifier.
 *
 * Since: 2.56
 */
const gchar* webkit_application_manifest_get_id(WebKitApplicationManifest* manifest)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST(manifest), nullptr);
    return manifest->priv->id.data();
}

/**
 * webkit_application_manifest_get_display_mode: (get-property display-mode):
 * @manifest: an ApplicationManifest
 *
 * Get the requested display mode.
 *
 * Returns: (nullable): "browser", "minimal-ui", "standalone", or
 *     "fullscreen".
 *
 * Since: 2.56
 */
const gchar* webkit_application_manifest_get_display_mode(WebKitApplicationManifest* manifest)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST(manifest), nullptr);
    return manifest->priv->displayMode.data();
}

/**
 * webkit_application_manifest_get_background_color:
 * @manifest: an ApplicationManifest
 *
 * Get the background color declared in @manifest.
 *
 * Returns: (nullable) (transfer none): the background color, or %NULL if
 *     @manifest does not define a valid background color.
 *
 * Since: 2.56
 */
#if PLATFORM(GTK)
const GdkRGBA* webkit_application_manifest_get_background_color(WebKitApplicationManifest* manifest)
#elif PLATFORM(WPE)
const WebKitColor* webkit_application_manifest_get_background_color(WebKitApplicationManifest* manifest)
#endif
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST(manifest), nullptr);

    return manifest->priv->backgroundColor ? &*manifest->priv->backgroundColor : nullptr;
}

/**
 * webkit_application_manifest_get_background_color_dark:
 * @manifest: an ApplicationManifest
 *
 * Get the dark background color declared in @manifest.
 *
 * Returns: (nullable) (transfer none): the dark background color, or %NULL if
 *     @manifest does not define a valid dark background color.
 *
 * Since: 2.56
 */
#if PLATFORM(GTK)
const GdkRGBA* webkit_application_manifest_get_background_color_dark(WebKitApplicationManifest* manifest)
#elif PLATFORM(WPE)
const WebKitColor* webkit_application_manifest_get_background_color_dark(WebKitApplicationManifest* manifest)
#endif
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST(manifest), nullptr);

    return manifest->priv->backgroundColorDark ? &*manifest->priv->backgroundColorDark : nullptr;
}

/**
 * webkit_application_manifest_get_theme_color:
 * @manifest: an ApplicationManifest
 *
 * Get the theme color declared in @manifest.
 *
 * Returns: (nullable) (transfer none): the theme color, or %NULL if @manifest
 *     does not define a valid theme color.
 *
 * Since: 2.56
 */
#if PLATFORM(GTK)
const GdkRGBA* webkit_application_manifest_get_theme_color(WebKitApplicationManifest* manifest)
#elif PLATFORM(WPE)
const WebKitColor* webkit_application_manifest_get_theme_color(WebKitApplicationManifest* manifest)
#endif
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST(manifest), nullptr);

    return manifest->priv->themeColor ? &*manifest->priv->themeColor : nullptr;
}

/**
 * webkit_application_manifest_get_theme_color_dark:
 * @manifest: an ApplicationManifest
 *
 * Get the dark theme color declared in @manifest.
 *
 * Returns: (nullable) (transfer none): the dark theme color, or %NULL if
 *     @manifest does not define a valid dark theme color.
 *
 * Since: 2.56
 */
#if PLATFORM(GTK)
const GdkRGBA* webkit_application_manifest_get_theme_color_dark(WebKitApplicationManifest* manifest)
#elif PLATFORM(WPE)
const WebKitColor* webkit_application_manifest_get_theme_color_dark(WebKitApplicationManifest* manifest)
#endif
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST(manifest), nullptr);

    return manifest->priv->themeColorDark ? &*manifest->priv->themeColorDark : nullptr;
}

/**
 * webkit_application_manifest_get_icons:
 * @manifest: an ApplicationManifest
 *
 * Get the icon descriptors declared in @manifest.
 *
 * Returns: (array zero-terminated=1) (element-type WebKitApplicationManifestIcon) (transfer none):
 *     a %NULL-terminated array of [class@ApplicationManifestIcon]. The array
 *     and its contents are owned by @manifest and must not be modified or freed.
 *
 * Since: 2.56
 */
WebKitApplicationManifestIcon* const* webkit_application_manifest_get_icons(WebKitApplicationManifest* manifest)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST(manifest), nullptr);

    return reinterpret_cast<WebKitApplicationManifestIcon* const*>(manifest->priv->icons->pdata);
}

/**
 * webkit_application_manifest_get_shortcuts:
 * @manifest: an ApplicationManifest
 *
 * Get the shortcuts declared in @manifest.
 *
 * Returns: (array zero-terminated=1) (element-type WebKitApplicationManifestShortcut) (transfer none):
 *     a %NULL-terminated array of [class@ApplicationManifestShortcut]. The
 *     array and its contents are owned by @manifest and must not be modified or
 *     freed.
 *
 * Since: 2.56
 */
WebKitApplicationManifestShortcut* const* webkit_application_manifest_get_shortcuts(WebKitApplicationManifest* manifest)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST(manifest), nullptr);

    return reinterpret_cast<WebKitApplicationManifestShortcut* const*>(manifest->priv->shortcuts->pdata);
}

/**
 * webkit_application_manifest_icon_get_uri:
 * @icon: an ApplicationManifestIcon
 *
 * Get the resolved URI of @icon.
 *
 * Returns: (nullable): the icon URI.
 *
 * Since: 2.56
 */
const gchar* webkit_application_manifest_icon_get_uri(WebKitApplicationManifestIcon* icon)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST_ICON(icon), nullptr);
    return icon->priv->uri.data();
}

/**
 * webkit_application_manifest_icon_get_sizes:
 * @icon: an ApplicationManifestIcon
 *
 * Get the size descriptors declared for @icon.
 *
 * Returns: (array zero-terminated=1) (element-type utf8) (transfer none): a
 *     %NULL-terminated array of size descriptors. The array and its contents
 *     are owned by @icon and must not be modified or freed.
 *
 * Since: 2.56
 */
const gchar* const* webkit_application_manifest_icon_get_sizes(WebKitApplicationManifestIcon* icon)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST_ICON(icon), nullptr);
    return reinterpret_cast<const gchar* const*>(icon->priv->sizes->pdata);
}

/**
 * webkit_application_manifest_icon_get_mime_type:
 * @icon: an ApplicationManifestIcon
 *
 * Get the MIME type declared for @icon.
 *
 * Returns: (nullable): the icon MIME type.
 *
 * Since: 2.56
 */
const gchar* webkit_application_manifest_icon_get_mime_type(WebKitApplicationManifestIcon* icon)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST_ICON(icon), nullptr);
    return icon->priv->mimeType.data();
}

/**
 * webkit_application_manifest_icon_get_is_maskable:
 * @icon: an ApplicationManifestIcon
 *
 * Whether @icon declares the "maskable" purpose.
 *
 * Returns: %TRUE if @icon is maskable.
 *
 * Since: 2.56
 */
gboolean webkit_application_manifest_icon_get_is_maskable(WebKitApplicationManifestIcon* icon)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST_ICON(icon), FALSE);
    return icon->priv->isMaskable;
}

/**
 * webkit_application_manifest_icon_get_is_monochrome:
 * @icon: an ApplicationManifestIcon
 *
 * Whether @icon declares the "monochrome" purpose.
 *
 * Returns: %TRUE if @icon is monochrome.
 *
 * Since: 2.56
 */
gboolean webkit_application_manifest_icon_get_is_monochrome(WebKitApplicationManifestIcon* icon)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST_ICON(icon), FALSE);
    return icon->priv->isMonochrome;
}

/**
 * webkit_application_manifest_shortcut_get_name:
 * @shortcut: an ApplicationManifestShortcut
 *
 * Get the name of @shortcut.
 *
 * Returns: (nullable): the shortcut name.
 *
 * Since: 2.56
 */
const gchar* webkit_application_manifest_shortcut_get_name(WebKitApplicationManifestShortcut* shortcut)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST_SHORTCUT(shortcut), nullptr);
    return shortcut->priv->name.data();
}

/**
 * webkit_application_manifest_shortcut_get_uri:
 * @shortcut: an ApplicationManifestShortcut
 *
 * Get the resolved URI of @shortcut.
 *
 * Returns: (nullable): the shortcut URI.
 *
 * Since: 2.56
 */
const gchar* webkit_application_manifest_shortcut_get_uri(WebKitApplicationManifestShortcut* shortcut)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST_SHORTCUT(shortcut), nullptr);
    return shortcut->priv->uri.data();
}

/**
 * webkit_application_manifest_shortcut_get_icons:
 * @shortcut: an ApplicationManifestShortcut
 *
 * Get the icon descriptors declared for @shortcut.
 *
 * Returns: (array zero-terminated=1) (element-type WebKitApplicationManifestIcon) (transfer none):
 *     a %NULL-terminated array of [class@ApplicationManifestIcon]. The array
 *     and its contents are owned by @shortcut and must not be modified or
 *     freed.
 *
 * Since: 2.56
 */
WebKitApplicationManifestIcon* const* webkit_application_manifest_shortcut_get_icons(WebKitApplicationManifestShortcut* shortcut)
{
    g_return_val_if_fail(WEBKIT_IS_APPLICATION_MANIFEST_SHORTCUT(shortcut), nullptr);
    return reinterpret_cast<WebKitApplicationManifestIcon* const*>(shortcut->priv->icons->pdata);
}

#endif // ENABLE(2022_GLIB_API)
