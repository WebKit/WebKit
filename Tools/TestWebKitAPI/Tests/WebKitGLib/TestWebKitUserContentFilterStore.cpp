/*
 * Copyright (C) 2019 Igalia S.L.
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

#include "TestMain.h"
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

static const char* kInvalidJSONSources[] = {
    // Empty input.
    "",
    // Incorrect JSON input.
    "[",
    "]",
    "[{]",
    "[{ \"trigger: {} }]",
    "[{ 'trigger': { 'url-filter': '.*' }, 'action': { 'type': 'block' } }]",
    // Oddball JSON objects.
    "{}",
    "[]",
    "[{}]",
    // Empty trigger/action.
    "[{ \"trigger\": {} }]",
    "[{ \"action\": {} }]",
    "[{ \"trigger\": {}, \"action\": {} }]",
    "[{ \"trigger\": { \"url-filter\": \".*\" } }]",
    "[{ \"trigger\": { \"url-filter\": \".*\" }, \"action\": {} }]",
    // Empty action type.
    "[{ \"trigger\": { \"url-filter\": \".*\" }, \"action\": { \"type\": \"\" } }]",
    // Empty URL filter.
    "[{ \"trigger\": { \"url-filter\": \"\" }, \"action\": { \"type\": \"block\" } }]",
};

static const char* kSimpleJSONSource =
    "[{\n"
    "  \"trigger\": {\n"
    "    \"url-filter\": \".*\"\n"
    "  },\n"
    "  \"action\": {\n"
    "    \"type\": \"block\"\n"
    "  }\n"
    "}]\n";

namespace WTF {

template <> WebKitUserContentFilter* refGPtr(WebKitUserContentFilter* ptr)
{
    if (ptr)
        webkit_user_content_filter_ref(ptr);
    return ptr;
}

template <> void derefGPtr(WebKitUserContentFilter* ptr)
{
    if (ptr)
        webkit_user_content_filter_unref(ptr);
}

}

class UserContentFilterStoreTest: public Test {
public:
    MAKE_GLIB_TEST_FIXTURE(UserContentFilterStoreTest);

    GError* lastError() const
    {
        return m_error.get();
    }

    void resetStore()
    {
        auto* oldStore = m_filterStore.get();
        m_filterStore = newStore();
        g_assert_true(oldStore != m_filterStore.get());
    }

    char** fetchIdentifiers()
    {
        webkit_user_content_filter_store_fetch_identifiers(m_filterStore.get(), nullptr, [](GObject* sourceObject, GAsyncResult* result, void* userData) {
            auto* test = static_cast<UserContentFilterStoreTest*>(userData);
            test->m_filterIds.reset(webkit_user_content_filter_store_fetch_identifiers_finish(WEBKIT_USER_CONTENT_FILTER_STORE(sourceObject), result));
            g_main_loop_quit(test->m_mainLoop.get());
        }, this);
        g_main_loop_run(m_mainLoop.get());
        g_assert_nonnull(m_filterIds.get());
        return m_filterIds.get();
    }

    WebKitUserContentFilter* saveFilter(const char* filterId, const char* source)
    {
        GRefPtr<GBytes> sourceBytes = adoptGRef(g_bytes_new_static(source, strlen(source)));
        webkit_user_content_filter_store_save(m_filterStore.get(), filterId, sourceBytes.get(), nullptr, [](GObject* sourceObject, GAsyncResult* result, void* userData) {
            auto* test = static_cast<UserContentFilterStoreTest*>(userData);
            test->m_filter = adoptGRef(webkit_user_content_filter_store_save_finish(WEBKIT_USER_CONTENT_FILTER_STORE(sourceObject), result, &test->m_error.outPtr()));
            g_main_loop_quit(test->m_mainLoop.get());
        }, this);
        g_main_loop_run(m_mainLoop.get());
        return m_filter.get();
    }

    WebKitUserContentFilter* saveFilterFromFile(const char* filterId, GFile* file)
    {
        webkit_user_content_filter_store_save_from_file(m_filterStore.get(), filterId, file, nullptr, [](GObject* sourceObject, GAsyncResult* result, void* userData) {
            auto* test = static_cast<UserContentFilterStoreTest*>(userData);
            test->m_filter = adoptGRef(webkit_user_content_filter_store_save_from_file_finish(WEBKIT_USER_CONTENT_FILTER_STORE(sourceObject), result, &test->m_error.outPtr()));
            g_main_loop_quit(test->m_mainLoop.get());
        }, this);
        g_main_loop_run(m_mainLoop.get());
        return m_filter.get();
    }

    WebKitUserContentFilter* loadFilter(const char* filterId)
    {
        webkit_user_content_filter_store_load(m_filterStore.get(), filterId, nullptr, [](GObject* sourceObject, GAsyncResult* result, void* userData) {
            auto* test = static_cast<UserContentFilterStoreTest*>(userData);
            test->m_filter = adoptGRef(webkit_user_content_filter_store_load_finish(WEBKIT_USER_CONTENT_FILTER_STORE(sourceObject), result, &test->m_error.outPtr()));
            g_main_loop_quit(test->m_mainLoop.get());
        }, this);
        g_main_loop_run(m_mainLoop.get());
        return m_filter.get();
    }

    bool removeFilter(const char* filterId)
    {
        webkit_user_content_filter_store_remove(m_filterStore.get(), filterId, nullptr, [](GObject* sourceObject, GAsyncResult* result, void* userData) {
            auto* test = static_cast<UserContentFilterStoreTest*>(userData);
            test->m_completedOk = !!webkit_user_content_filter_store_remove_finish(WEBKIT_USER_CONTENT_FILTER_STORE(sourceObject), result, &test->m_error.outPtr());
            g_main_loop_quit(test->m_mainLoop.get());
        }, this);
        g_main_loop_run(m_mainLoop.get());
        return m_completedOk;
    }

private:
    GRefPtr<WebKitUserContentFilterStore> newStore()
    {
        GUniquePtr<char> storagePath(g_build_filename(dataDirectory(), "content-filters", nullptr));
        auto store = adoptGRef(webkit_user_content_filter_store_new(storagePath.get()));
        g_assert_nonnull(store.get());
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(store.get()));
        g_assert_cmpstr(storagePath.get(), ==, webkit_user_content_filter_store_get_path(store.get()));
        return store;
    }

    GRefPtr<WebKitUserContentFilterStore> m_filterStore { newStore() };
    GRefPtr<GMainLoop> m_mainLoop { adoptGRef(g_main_loop_new(nullptr, FALSE)) };
    GRefPtr<WebKitUserContentFilter> m_filter;
    GUniquePtr<char*> m_filterIds;
    GUniqueOutPtr<GError> m_error;
    bool m_completedOk;
};

static void testEmptyStore(UserContentFilterStoreTest* test, gconstpointer)
{
    g_assert_cmpuint(0, ==, g_strv_length(test->fetchIdentifiers()));
}

static void testSaveInvalidFilter(UserContentFilterStoreTest* test, gconstpointer)
{
    for (unsigned i = 0; i < G_N_ELEMENTS(kInvalidJSONSources); i++) {
        GUniquePtr<char> filterId(g_strdup_printf("Filter-%u", i));
        g_assert_null(test->saveFilter(filterId.get(), kInvalidJSONSources[i]));
        g_assert_error(test->lastError(), WEBKIT_USER_CONTENT_FILTER_ERROR, WEBKIT_USER_CONTENT_FILTER_ERROR_INVALID_SOURCE);
    }
}

static void testSaveLoadFilter(UserContentFilterStoreTest* test, gconstpointer)
{
    static const char* kFilterId = "SimpleFilter";

    // Trying to load a filter before saving must fail.
    g_assert_null(test->loadFilter(kFilterId));
    g_assert_error(test->lastError(), WEBKIT_USER_CONTENT_FILTER_ERROR, WEBKIT_USER_CONTENT_FILTER_ERROR_NOT_FOUND);

    // Then save a filter. Which must succed.
    g_assert_nonnull(test->saveFilter(kFilterId, kSimpleJSONSource));
    g_assert_no_error(test->lastError());

    // Now it should be possible to actually load the filter.
    g_assert_nonnull(test->loadFilter(kFilterId));
    g_assert_no_error(test->lastError());
}

static void testSavedFilterIdentifierMatch(UserContentFilterStoreTest* test, gconstpointer)
{
    static const char* kFilterId = "SimpleFilter";

    auto* filter = test->saveFilter(kFilterId, kSimpleJSONSource);
    g_assert_nonnull(filter);
    g_assert_no_error(test->lastError());
    g_assert_cmpstr(kFilterId, ==, webkit_user_content_filter_get_identifier(filter));
}

static void testRemoveFilter(UserContentFilterStoreTest* test, gconstpointer)
{
    static const char* kFilterId = "SimpleFilter";

    // Save a filter.
    g_assert_nonnull(test->saveFilter(kFilterId, kSimpleJSONSource));
    g_assert_no_error(test->lastError());

    // Now remove it.
    g_assert_true(test->removeFilter(kFilterId));
    g_assert_no_error(test->lastError());

    // Load must fail, and should not be listed.
    g_assert_null(test->loadFilter(kFilterId));
    g_assert_error(test->lastError(), WEBKIT_USER_CONTENT_FILTER_ERROR, WEBKIT_USER_CONTENT_FILTER_ERROR_NOT_FOUND);
    g_assert_cmpuint(0, ==, g_strv_length(test->fetchIdentifiers()));
}

static void testSaveMultipleFilters(UserContentFilterStoreTest* test, gconstpointer)
{
    static const char* kFilterIdSpam = "Filter-Spam";
    static const char* kFilterIdEggs = "Filter-Eggs";

    g_assert_nonnull(test->saveFilter(kFilterIdSpam, kSimpleJSONSource));
    g_assert_no_error(test->lastError());

    g_assert_nonnull(test->saveFilter(kFilterIdEggs, kSimpleJSONSource));
    g_assert_no_error(test->lastError());

    auto filterIds = test->fetchIdentifiers();
    g_assert_true(g_strv_contains(filterIds, kFilterIdSpam));
    g_assert_true(g_strv_contains(filterIds, kFilterIdEggs));
    g_assert_cmpuint(2, ==, g_strv_length(filterIds));

    g_assert_nonnull(test->saveFilter(kFilterIdEggs, kSimpleJSONSource));
    g_assert_no_error(test->lastError());

    filterIds = test->fetchIdentifiers();
    g_assert_true(g_strv_contains(filterIds, kFilterIdSpam));
    g_assert_true(g_strv_contains(filterIds, kFilterIdEggs));
    g_assert_cmpuint(2, ==, g_strv_length(filterIds));
}

static void testSaveFilterFromFile(UserContentFilterStoreTest* test, gconstpointer)
{
    static const char* kFilterId = "SimpleFilter";

    GUniqueOutPtr<GError> error;
    GUniquePtr<char> filePath(g_build_filename(test->dataDirectory(), "SimpleFilter.json", nullptr));
    g_assert_true(g_file_set_contents(filePath.get(), kSimpleJSONSource, strlen(kSimpleJSONSource), &error.outPtr()));
    g_assert_no_error(error.get());

    GRefPtr<GFile> file = adoptGRef(g_file_new_for_path(filePath.get()));
    g_assert_nonnull(test->saveFilterFromFile(kFilterId, file.get()));
    g_assert_no_error(test->lastError());

    g_assert_true(g_strv_contains(test->fetchIdentifiers(), kFilterId));
}

static void testFilterPersistence(UserContentFilterStoreTest* test, gconstpointer)
{
    static const char* kFilterId = "PersistedFilter";

    g_assert_nonnull(test->saveFilter(kFilterId, kSimpleJSONSource));
    g_assert_no_error(test->lastError());

    test->resetStore();

    g_assert_nonnull(test->loadFilter(kFilterId));
    g_assert_no_error(test->lastError());
}

void beforeAll()
{
    UserContentFilterStoreTest::add("WebKitUserContentFilterStore", "empty-store", testEmptyStore);
    UserContentFilterStoreTest::add("WebKitUserContentFilterStore", "invalid-filter-source", testSaveInvalidFilter);
    UserContentFilterStoreTest::add("WebKitUserContentFilterStore", "filter-save-load", testSaveLoadFilter);
    UserContentFilterStoreTest::add("WebKitUserContentFilterStore", "saved-filter-identifier-match", testSavedFilterIdentifierMatch);
    UserContentFilterStoreTest::add("WebKitUserContentFilterStore", "remove-filter", testRemoveFilter);
    UserContentFilterStoreTest::add("WebKitUserContentFilterStore", "save-multiple-filters", testSaveMultipleFilters);
    UserContentFilterStoreTest::add("WebKitUserContentFilterStore", "save-from-file", testSaveFilterFromFile);
    UserContentFilterStoreTest::add("WebKitUserContentFilterStore", "filter-persistence", testFilterPersistence);
}

void afterAll()
{
}
