/*
 * Copyright (C) 2018-2019 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebKitUserContentFilterStore.h"

#include "APIContentRuleList.h"
#include "APIContentRuleListStore.h"
#include "WebKitError.h"
#include "WebKitInitialize.h"
#include "WebKitUserContent.h"
#include "WebKitUserContentPrivate.h"
#include <WebCore/ContentExtensionError.h>
#include <glib/gi18n-lib.h>
#include <wtf/CompletionHandler.h>
#include <wtf/FileSystem.h>
#include <wtf/RefPtr.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>

using namespace WebKit;

/**
 * SECTION: WebKitUserContentFilterStore
 * @Short_description: Handles storage of user content filters on disk.
 * @Title: WebKitUserContentFilterStore
 *
 * The WebKitUserContentFilterStore provides the means to import and save
 * [JSON rule sets](https://webkit.org/blog/3476/content-blockers-first-look/),
 * which can be loaded later in an efficient manner. Once filters are stored,
 * the #WebKitUserContentFilter objects which represent them can be added to
 * a #WebKitUserContentManager with webkit_user_content_manager_add_filter().
 *
 * JSON rule sets are imported using webkit_user_content_filter_store_save() and stored
 * on disk in an implementation defined format. The contents of a filter store must be
 * managed using the #WebKitUserContentFilterStore: a list of all the stored filters
 * can be obtained with webkit_user_content_filter_store_fetch_identifiers(),
 * webkit_user_content_filter_store_load() can be used to retrieve a previously saved
 * filter, and removed from the store with webkit_user_content_filter_store_remove().
 *
 * Since: 2.24
 */

enum {
    PROP_0,

    PROP_PATH,
};

static inline GError* toGError(WebKitUserContentFilterError code, const std::error_code error)
{
    ASSERT(error);
    return g_error_new_literal(WEBKIT_USER_CONTENT_FILTER_ERROR, code, error.message().c_str());
}

struct _WebKitUserContentFilterStorePrivate {
    GUniquePtr<char> storagePath;
    RefPtr<API::ContentRuleListStore> store;
};

WEBKIT_DEFINE_TYPE(WebKitUserContentFilterStore, webkit_user_content_filter_store, G_TYPE_OBJECT)

static void webkitUserContentFilterStoreGetProperty(GObject* object, guint propID, GValue* value, GParamSpec* paramSpec)
{
    WebKitUserContentFilterStore* store = WEBKIT_USER_CONTENT_FILTER_STORE(object);

    switch (propID) {
    case PROP_PATH:
        g_value_set_string(value, webkit_user_content_filter_store_get_path(store));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, paramSpec);
    }
}

static void webkitUserContentFilterStoreSetProperty(GObject* object, guint propID, const GValue* value, GParamSpec* paramSpec)
{
    WebKitUserContentFilterStore* store = WEBKIT_USER_CONTENT_FILTER_STORE(object);

    switch (propID) {
    case PROP_PATH:
        store->priv->storagePath.reset(g_value_dup_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, paramSpec);
    }
}

static void webkitUserContentFilterStoreConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_user_content_filter_store_parent_class)->constructed(object);

    WebKitUserContentFilterStore* store = WEBKIT_USER_CONTENT_FILTER_STORE(object);
    store->priv->store = adoptRef(new API::ContentRuleListStore(FileSystem::stringFromFileSystemRepresentation(store->priv->storagePath.get()), false));
}

static void webkit_user_content_filter_store_class_init(WebKitUserContentFilterStoreClass* storeClass)
{
    webkitInitialize();

    GObjectClass* gObjectClass = G_OBJECT_CLASS(storeClass);

    gObjectClass->get_property = webkitUserContentFilterStoreGetProperty;
    gObjectClass->set_property = webkitUserContentFilterStoreSetProperty;
    gObjectClass->constructed = webkitUserContentFilterStoreConstructed;

    /**
     * WebKitUserContentFilterStore:path:
     *
     * The directory used for filter storage. This path is used as the base
     * directory where user content filters are stored on disk.
     *
     * Since: 2.24
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_PATH,
        g_param_spec_string(
            "path",
            _("Storage directory path"),
            _("The directory where user content filters are stored"),
            nullptr,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
}

/**
 * webkit_user_content_filter_store_new:
 * @storage_path: path where data for filters will be stored on disk
 *
 * Create a new #WebKitUserContentFilterStore to manipulate filters stored at @storage_path.
 * The path must point to a local filesystem, and will be created if needed.
 *
 * Returns: (transfer full): a newly created #WebKitUserContentFilterStore
 *
 * Since: 2.24
 */
WebKitUserContentFilterStore* webkit_user_content_filter_store_new(const gchar* storagePath)
{
    g_return_val_if_fail(storagePath, nullptr);
    return WEBKIT_USER_CONTENT_FILTER_STORE(g_object_new(WEBKIT_TYPE_USER_CONTENT_FILTER_STORE, "path", storagePath, nullptr));
}

/**
 * webkit_user_content_filter_store_get_path:
 * @store: a #WebKitUserContentFilterStore
 *
 * Returns: (transfer none): The storage path for user content filters.
 *
 * Since: 2.24
 */
const char* webkit_user_content_filter_store_get_path(WebKitUserContentFilterStore* store)
{
    g_return_val_if_fail(WEBKIT_IS_USER_CONTENT_FILTER_STORE(store), nullptr);
    return store->priv->storagePath.get();
}

static void webkitUserContentFilterStoreSaveBytes(GRefPtr<GTask>&& task, String&& identifier, GRefPtr<GBytes>&& source)
{
    size_t sourceSize;
    const char* sourceData = static_cast<const char*>(g_bytes_get_data(source.get(), &sourceSize));
    if (!sourceSize) {
        g_task_return_error(task.get(), g_error_new_literal(WEBKIT_USER_CONTENT_FILTER_ERROR, WEBKIT_USER_CONTENT_FILTER_ERROR_INVALID_SOURCE, "Source JSON rule set cannot be empty"));
        return;
    }

    auto* store = WEBKIT_USER_CONTENT_FILTER_STORE(g_task_get_source_object(task.get()));
    store->priv->store->compileContentRuleList(identifier, String::fromUTF8(sourceData, sourceSize), [task = WTFMove(task)](RefPtr<API::ContentRuleList> contentRuleList, std::error_code error) {
        if (g_task_return_error_if_cancelled(task.get()))
            return;

        if (error) {
            ASSERT(error.category() == WebCore::ContentExtensions::contentExtensionErrorCategory());
            g_task_return_error(task.get(), toGError(WEBKIT_USER_CONTENT_FILTER_ERROR_INVALID_SOURCE, error));
        } else
            g_task_return_pointer(task.get(), webkitUserContentFilterCreate(WTFMove(contentRuleList)), reinterpret_cast<GDestroyNotify>(webkit_user_content_filter_unref));
    });
}

/**
 * webkit_user_content_filter_store_save:
 * @store: a #WebKitUserContentFilterStore
 * @identifier: a string used to identify the saved filter
 * @source: #GBytes containing the rule set in JSON format
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when saving is completed
 * @user_data: (closure): the data to pass to the callback function
 *
 * Asynchronously save a content filter from a source rule set in the
 * [WebKit content extesions JSON format](https://webkit.org/blog/3476/content-blockers-first-look/).
 *
 * The @identifier can be used afterwards to refer to the filter when using
 * webkit_user_content_filter_store_remove() and webkit_user_content_filter_store_load().
 * When the @identifier has been used in the past, the new filter source will replace
 * the one saved beforehand for the same identifier.
 *
 * When the operation is finished, @callback will be invoked, which then can use
 * webkit_user_content_filter_store_save_finish() to obtain the resulting filter.
 *
 * Since: 2.24
 */
void webkit_user_content_filter_store_save(WebKitUserContentFilterStore* store, const gchar* identifier, GBytes* source, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_USER_CONTENT_FILTER_STORE(store));
    g_return_if_fail(identifier);
    g_return_if_fail(source);
    g_return_if_fail(callback);

    GRefPtr<GTask> task = adoptGRef(g_task_new(store, cancellable, callback, userData));
    webkitUserContentFilterStoreSaveBytes(WTFMove(task), String::fromUTF8(identifier), GRefPtr<GBytes>(source));
}

/**
 * webkit_user_content_filter_store_save_finish:
 * @store: a #WebKitUserContentFilterStore
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finishes an asynchronous filter save previously started with
 * webkit_user_content_filter_store_save().
 *
 * Returns: (transfer full): a #WebKitUserContentFilter, or %NULL if saving failed
 *
 * Since: 2.24
 */
WebKitUserContentFilter* webkit_user_content_filter_store_save_finish(WebKitUserContentFilterStore* store, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_USER_CONTENT_FILTER_STORE(store), nullptr);
    g_return_val_if_fail(result, nullptr);
    return static_cast<WebKitUserContentFilter*>(g_task_propagate_pointer(G_TASK(result), error));
}

struct SaveTaskData {
    String identifier;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(SaveTaskData)

/**
 * webkit_user_content_filter_store_save_from_file:
 * @store: a #WebKitUserContentFilterStore
 * @identifier: a string used to identify the saved filter
 * @file: a #GFile containing the rule set in JSON format
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when saving is completed
 * @user_data: (closure): the data to pass to the callback function
 *
 * Asynchronously save a content filter from the contents of a file, which must be
 * native to the platform, as checked by g_file_is_native(). See
 * webkit_user_content_filter_store_save() for more details.
 *
 * When the operation is finished, @callback will be invoked, which then can use
 * webkit_user_content_filter_store_save_finish() to obtain the resulting filter.
 *
 * Since: 2.24
 */
void webkit_user_content_filter_store_save_from_file(WebKitUserContentFilterStore* store, const gchar* identifier, GFile* file, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_USER_CONTENT_FILTER_STORE(store));
    g_return_if_fail(identifier);
    g_return_if_fail(G_IS_FILE(file));
    g_return_if_fail(callback);

    GRefPtr<GTask> task = adoptGRef(g_task_new(store, cancellable, callback, userData));

    // Try mapping the file in memory first, and fall-back to reading the contents if that fails.
    if (g_file_is_native(file)) {
        GUniquePtr<char> filePath(g_file_get_path(file));
        GRefPtr<GMappedFile> mappedFile = adoptGRef(g_mapped_file_new(filePath.get(), FALSE, nullptr));
        if (mappedFile) {
            GRefPtr<GBytes> source = adoptGRef(g_mapped_file_get_bytes(mappedFile.get()));
            webkitUserContentFilterStoreSaveBytes(WTFMove(task), String::fromUTF8(identifier), WTFMove(source));
            return;
        }
    }

    // Pass the identifier as task data to be used in the completion callback once the contents have been loaded.
    SaveTaskData* data = createSaveTaskData();
    data->identifier = String::fromUTF8(identifier);
    g_task_set_task_data(task.get(), data, reinterpret_cast<GDestroyNotify>(destroySaveTaskData));

    g_file_load_contents_async(file, cancellable, [](GObject* sourceObject, GAsyncResult* result, void* userData) {
        GRefPtr<GTask> task = adoptGRef(G_TASK(userData));
        if (g_task_return_error_if_cancelled(task.get()))
            return;

        char* sourceData;
        size_t sourceSize;
        GUniqueOutPtr<GError> error;
        if (g_file_load_contents_finish(G_FILE(sourceObject), result, &sourceData, &sourceSize, nullptr, &error.outPtr())) {
            SaveTaskData* data = static_cast<SaveTaskData*>(g_task_get_task_data(task.get()));
            webkitUserContentFilterStoreSaveBytes(WTFMove(task), WTFMove(data->identifier), GRefPtr<GBytes>(g_bytes_new_take(sourceData, sourceSize)));
        } else
            g_task_return_error(task.get(), error.release());
    }, task.leakRef());
}

/**
 * webkit_user_content_filter_store_save_from_file_finish:
 * @store: a #WebKitUserContentFilterStore
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finishes and asynchronous filter save previously started with
 * webkit_user_content_filter_store_save_from_file().
 *
 * Returns: (transfer full): a #WebKitUserContentFilter, or %NULL if saving failed.
 *
 * Since: 2.24
 */
WebKitUserContentFilter* webkit_user_content_filter_store_save_from_file_finish(WebKitUserContentFilterStore* store, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_USER_CONTENT_FILTER_STORE(store), nullptr);
    g_return_val_if_fail(result, nullptr);
    return static_cast<WebKitUserContentFilter*>(g_task_propagate_pointer(G_TASK(result), error));
}

/**
 * webkit_user_content_filter_store_remove:
 * @store: a #WebKitUserContentFilterStore
 * @identifier: a filter identifier
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the removal is completed
 * @user_data: (closure): the data to pass to the callback function
 *
 * Asynchronously remove a content filter given its @identifier.
 *
 * When the operation is finished, @callback will be invoked, which then can use
 * webkit_user_content_filter_store_remove_finish() to check whether the removal was
 * successful.
 *
 * Since: 2.24
 */
void webkit_user_content_filter_store_remove(WebKitUserContentFilterStore* store, const gchar* identifier, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_USER_CONTENT_FILTER_STORE(store));
    g_return_if_fail(identifier);
    g_return_if_fail(callback);

    GRefPtr<GTask> task = adoptGRef(g_task_new(store, cancellable, callback, userData));
    store->priv->store->removeContentRuleList(String::fromUTF8(identifier), [task = WTFMove(task)](std::error_code error) {
        if (g_task_return_error_if_cancelled(task.get()))
            return;

        if (error) {
            ASSERT(static_cast<API::ContentRuleListStore::Error>(error.value()) == API::ContentRuleListStore::Error::RemoveFailed);
            g_task_return_error(task.get(), toGError(WEBKIT_USER_CONTENT_FILTER_ERROR_NOT_FOUND, error));
        } else
            g_task_return_boolean(task.get(), TRUE);
    });
}

/**
 * webkit_user_content_filter_store_remove_finish:
 * @store: a #WebKitUserContentFilterStore
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finishes an asynchronous filter removal previously started with
 * webkit_user_content_filter_store_remove().
 *
 * Returns: whether the removal was successful
 *
 * Since: 2.24
 */
gboolean webkit_user_content_filter_store_remove_finish(WebKitUserContentFilterStore* store, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_USER_CONTENT_FILTER_STORE(store), FALSE);
    g_return_val_if_fail(result, FALSE);
    return g_task_propagate_boolean(G_TASK(result), error);
}

/**
 * webkit_user_content_filter_store_load:
 * @store: a #WebKitUserContentFilterStore
 * @identifier: a filter identifier
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the load is completed
 * @user_data: (closure): the data to pass to the callback function
 *
 * Asynchronously load a content filter given its @identifier. The filter must have been
 * previously stored using webkit_user_content_filter_store_save().
 *
 * When the operation is finished, @callback will be invoked, which then can use
 * webkit_user_content_filter_store_load_finish() to obtain the resulting filter.
 *
 * Since: 2.24
 */
void webkit_user_content_filter_store_load(WebKitUserContentFilterStore* store, const gchar* identifier, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_USER_CONTENT_FILTER_STORE(store));
    g_return_if_fail(identifier);
    g_return_if_fail(callback);

    GRefPtr<GTask> task = adoptGRef(g_task_new(store, cancellable, callback, userData));
    store->priv->store->lookupContentRuleList(String::fromUTF8(identifier), [task = WTFMove(task)](RefPtr<API::ContentRuleList> contentRuleList, std::error_code error) {
        if (g_task_return_error_if_cancelled(task.get()))
            return;

        if (error) {
            ASSERT(static_cast<API::ContentRuleListStore::Error>(error.value()) == API::ContentRuleListStore::Error::LookupFailed
                || static_cast<API::ContentRuleListStore::Error>(error.value()) == API::ContentRuleListStore::Error::VersionMismatch);
            g_task_return_error(task.get(), toGError(WEBKIT_USER_CONTENT_FILTER_ERROR_NOT_FOUND, error));
        } else
            g_task_return_pointer(task.get(), webkitUserContentFilterCreate(WTFMove(contentRuleList)), reinterpret_cast<GDestroyNotify>(webkit_user_content_filter_unref));
    });
}

/**
 * webkit_user_content_filter_store_load_finish:
 * @store: a #WebKitUserContentFilterStore
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finishes an asynchronous filter load previously started with
 * webkit_user_content_filter_store_load().
 *
 * Returns: (transfer full): a #WebKitUserContentFilter, or %NULL if the load failed
 *
 * Since: 2.24
 */
WebKitUserContentFilter* webkit_user_content_filter_store_load_finish(WebKitUserContentFilterStore* store, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_USER_CONTENT_FILTER_STORE(store), nullptr);
    g_return_val_if_fail(result, nullptr);
    return static_cast<WebKitUserContentFilter*>(g_task_propagate_pointer(G_TASK(result), error));
}

/**
 * webkit_user_content_filter_store_fetch_identifiers:
 * @store: a #WebKitUserContentFilterStore
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the removal is completed
 * @user_data: (closure): the data to pass to the callback function
 *
 * Asynchronously retrieve a list of the identifiers for all the stored filters.
 *
 * When the operation is finished, @callback will be invoked, which then can use
 * webkit_user_content_filter_store_fetch_identifiers_finish() to obtain the list of
 * filter identifiers.
 *
 * Since: 2.24
 */
void webkit_user_content_filter_store_fetch_identifiers(WebKitUserContentFilterStore* store, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_USER_CONTENT_FILTER_STORE(store));
    g_return_if_fail(callback);

    GRefPtr<GTask> task = adoptGRef(g_task_new(store, cancellable, callback, userData));
    store->priv->store->getAvailableContentRuleListIdentifiers([task = WTFMove(task)](WTF::Vector<WTF::String> identifiers) {
        if (g_task_return_error_if_cancelled(task.get()))
            return;

        GStrv result = static_cast<GStrv>(g_new0(gchar*, identifiers.size() + 1));
        for (size_t i = 0; i < identifiers.size(); ++i)
            result[i] = g_strdup(identifiers[i].utf8().data());
        g_task_return_pointer(task.get(), result, reinterpret_cast<GDestroyNotify>(g_strfreev));
    });
}

/**
 * webkit_user_content_filter_store_fetch_identifiers_finish:
 * @store: a #WebKitUserContentFilterStore
 * @result: a #GAsyncResult
 *
 * Finishes an asynchronous fetch of the list of identifiers for the stored filters previously
 * started with webkit_user_content_filter_store_fetch_identifiers().
 *
 * Returns: (transfer full) (array zero-terminated=1) (element-type utf8): a %NULL-terminated list of filter identifiers.
 *
 * Since: 2.24
 */
gchar** webkit_user_content_filter_store_fetch_identifiers_finish(WebKitUserContentFilterStore* store, GAsyncResult* result)
{
    g_return_val_if_fail(WEBKIT_IS_USER_CONTENT_FILTER_STORE(store), nullptr);
    g_return_val_if_fail(result, nullptr);
    return static_cast<gchar**>(g_task_propagate_pointer(G_TASK(result), nullptr));
}
