/*
 * Copyright (C) 2023 Igalia S.L.
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
#include "WebKitFeature.h"

#include "APIFeature.h"
#include "WebKitFeaturePrivate.h"
#include <wtf/Algorithms.h>
#include <wtf/RefPtr.h>

static inline WebKitFeatureStatus toFeatureStatus(API::FeatureStatus status)
{
    switch (status) {
    case API::FeatureStatus::Embedder:
        return WEBKIT_FEATURE_STATUS_EMBEDDER;
    case API::FeatureStatus::Unstable:
        return WEBKIT_FEATURE_STATUS_UNSTABLE;
    case API::FeatureStatus::Internal:
        return WEBKIT_FEATURE_STATUS_INTERNAL;
    case API::FeatureStatus::Developer:
        return WEBKIT_FEATURE_STATUS_DEVELOPER;
    case API::FeatureStatus::Testable:
        return WEBKIT_FEATURE_STATUS_TESTABLE;
    case API::FeatureStatus::Preview:
        return WEBKIT_FEATURE_STATUS_PREVIEW;
    case API::FeatureStatus::Stable:
        return WEBKIT_FEATURE_STATUS_STABLE;
    case API::FeatureStatus::Mature:
        return WEBKIT_FEATURE_STATUS_MATURE;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static inline const char* toFeatureCategory(API::FeatureCategory category)
{
    switch (category) {
    case API::FeatureCategory::None:
        return "Other";
    case API::FeatureCategory::Animation:
        return "Animation";
    case API::FeatureCategory::CSS:
        return "CSS";
    case API::FeatureCategory::DOM:
        return "DOM";
    case API::FeatureCategory::HTML:
        return "HTML";
    case API::FeatureCategory::Javascript:
        return "JavaScript";
    case API::FeatureCategory::Media:
        return "Media";
    case API::FeatureCategory::Networking:
        return "Network";
    case API::FeatureCategory::Privacy:
        return "Privacy";
    case API::FeatureCategory::Security:
        return "Security";
    }
    RELEASE_ASSERT_NOT_REACHED();
}

/**
 * WebKitFeature: (ref-func webkit_feature_ref) (unref-func webkit_feature_unref)
 *
 * Describes a web engine feature that may be toggled at runtime.
 *
 * The WebKit web engine includes a set of features which may be toggled
 * programmatically, each one represented by a #WebKitFeature that provides
 * information about it:
 *
 * - A unique “identifier”: [method@Feature.get_identifier].
 * - A “default value”, which indicates whether the option is enabled
 *   automatically: [method@Feature.get_default_value].
 * - Its “status”, which determines whether it should be considered
 *   user-settable and its development stage (see [enum@FeatureStatus]
 *   for details): [method@Feature.get_status].
 * - A category, which may be used to group features together:
 *   [method@Feature.get_category].
 * - An optional short “name” which can be presented to an user:
 *   [method@Feature.get_name].
 * - An optional longer “detailed” description:
 *   [method@Feature.get_details].
 *
 * The lists of available features can be obtained with
 * [func@Settings.get_all_features], [func@Settings.get_experimental_features],
 * and [func@Settings.get_development_features]). As a rule of thumb,
 * applications which may want to allow users (i.e. web developers) to test
 * WebKit features should use the list of experimental features. Additionally,
 * applications might want to expose development features *when targeting
 * technically inclined users* for early testing of in-development features
 * (i.e. in “technology preview” or “canary” builds).
 *
 * Applications **must not** expose the list of all features to end users
 * because they often lack descriptions and control parts of the web engine
 * which are either intended to be used during development of WebKit itself,
 * or in specific scenarios to tweak how WebKit integrates with the
 * application.
 *
 * Since: 2.42
 */
struct _WebKitFeature {
    _WebKitFeature(RefPtr<API::Feature>&& feature)
        : feature(WTFMove(feature))
    {
    }

    RefPtr<API::Feature> feature;
    CString identifier { toIdentifier(feature->key()) };
    CString name { feature->name().utf8() };
    CString details { feature->details().utf8() };
    int referenceCount { 1 };

    static CString toIdentifier(const String& key)
    {
        if (key.endsWith("Enabled"_s))
            return StringView(key).left(key.length() - (sizeof("Enabled") - 1)).utf8();
        return key.utf8();
    }
};

G_DEFINE_BOXED_TYPE(WebKitFeature, webkit_feature, webkit_feature_ref, webkit_feature_unref)

static WebKitFeature* webkitFeatureCreate(const RefPtr<API::Object>& apiFeature)
{
    ASSERT(apiFeature->type() == API::Object::Type::Feature);
    WebKitFeature* feature = static_cast<WebKitFeature*>(fastMalloc(sizeof(WebKitFeature)));
    new (feature) WebKitFeature(static_pointer_cast<API::Feature>(apiFeature));
    return feature;
}

API::Feature& webkitFeatureGetFeature(WebKitFeature* feature)
{
    ASSERT(feature);
    return *feature->feature;
}

/**
 * webkit_feature_ref:
 * @feature: a #WebKitFeature
 *
 * Atomically acquires a reference on the given @feature.
 *
 * This function is MT-safe and may be called from any thread.
 *
 * Returns: The same @feature with an additional reference.
 *
 * Since: 2.42
 */
WebKitFeature* webkit_feature_ref(WebKitFeature* feature)
{
    g_return_val_if_fail(feature, nullptr);
    g_atomic_int_inc(&feature->referenceCount);
    return feature;
}

/**
 * webkit_feature_unref:
 * @feature: a #WebKitFeature
 *
 * Atomically releases a reference on the given @feature.
 *
 * If the reference was the last, the resources associated to the
 * @feature are freed. This function is MT-safe and may be called from
 * any thread.
 *
 * Since: 2.42
 */
void webkit_feature_unref(WebKitFeature* feature)
{
    g_return_if_fail(feature);
    if (g_atomic_int_dec_and_test(&feature->referenceCount)) {
        feature->~WebKitFeature();
        fastFree(feature);
    }
}

/**
 * webkit_feature_get_identifier:
 * @feature: a #WebKitFeature
 *
 * Gets a string that uniquely identifies the @feature.
 *
 * Returns: (transfer none): The identifier string for the feature.
 *
 * Since: 2.42
 */
const char* webkit_feature_get_identifier(WebKitFeature* feature)
{
    g_return_val_if_fail(feature, nullptr);
    return feature->identifier.data();
}

/**
 * webkit_feature_get_name:
 * @feature: a #WebKitFeature
 *
 * Gets a short name for the @feature.
 *
 * The returned string is suitable to be displayed to end users, but it
 * should not be relied upon being localized.
 *
 * Note that some *features may not* have a short name, and @NULL
 * is returned in this case.
 *
 * Returns: (transfer none) (nullable): Short feature name.
 *
 * Since: 2.42
 */
const char* webkit_feature_get_name(WebKitFeature* feature)
{
    g_return_val_if_fail(feature, nullptr);
    return feature->name.length() ? feature->name.data() : nullptr;
}

/**
 * webkit_feature_get_details:
 * @feature: a #WebKitFeature
 *
 * Gets a description for the @feature.
 *
 * The detailed description should be considered an additional clarification
 * on the purpose of the feature, to be used as complementary aid to be
 * displayed along the feature name returned by [method@Feature.get_name].
 * The returned string is suitable to be displayed to end users, but it
 * should not be relied upon being localized.
 *
 * Note that some *features may not* have a detailed description, and @NULL
 * is returned in this case.
 *
 * Returns: (transfer none) (nullable): Feature description.
 *
 * Since: 2.42
 */
const char* webkit_feature_get_details(WebKitFeature* feature)
{
    g_return_val_if_fail(feature, nullptr);
    return feature->details.length() ? feature->details.data() : nullptr;
}

/**
 * webkit_feature_get_status:
 * @feature: a #WebKitFeature
 *
 * Gets the status of the feature.
 *
 * Returns: Feature status.
 *
 * Since: 2.42
 */
WebKitFeatureStatus webkit_feature_get_status(WebKitFeature* feature)
{
    g_return_val_if_fail(feature, WEBKIT_FEATURE_STATUS_EMBEDDER);
    return toFeatureStatus(feature->feature->status());
}

/**
 * webkit_feature_get_category:
 * @feature: a #WebKitFeature
 *
 * Gets the category of the feature.
 *
 * Applications which include user interface to toggle features may want
 * to use the category to group related features together.
 *
 * Returns: Feature category.
 *
 * Since: 2.42
 */
const char* webkit_feature_get_category(WebKitFeature* feature)
{
    g_return_val_if_fail(feature, "None");
    return toFeatureCategory(feature->feature->category());
}

/**
 * webkit_feature_get_default_value:
 * @feature: a #WebKitFeature
 *
 * Gets whether the feature is enabled by default.
 *
 * The default value may be used by applications which include user interface
 * to toggle features to restore its settings to their defaults. Note that
 * whether a feature is actually enabled must be checked with
 * [method@Settings.get_feature_enabled].
 *
 * Returns: Whether the feature is enabled by default.
 *
 * Since: 2.42
 */
gboolean webkit_feature_get_default_value(WebKitFeature* feature)
{
    g_return_val_if_fail(feature, FALSE);
    return feature->feature->defaultValue() ? TRUE : FALSE;
}

/**
 * WebKitFeatureList: (ref-func webkit_feature_list_ref) (unref-func webkit_feature_list_unref)
 *
 * Contains a set of toggle-able web engine features.
 *
 * The list supports passing around a set of [struct@Feature] objects and
 * iterating over them:
 *
 * ```c
 * g_autoptr(WebKitFeatureList) list = webkit_settings_get_experimental_features();
 * for (gsize i = 0; i < webkit_feature_list_get_length(list): i++) {
 *     WebKitFeature *feature = webkit_feature_list_get(list, i);
 *     // Do something with "feature".
 * }
 * ```
 *
 * Lists of features can be obtained with
 * [func@Settings.get_experimental_features],
 * [func@Settings.get_development_features], and
 * [func@Settings.get_all_features].
 *
 * Since: 2.42
 */
struct _WebKitFeatureList {
    _WebKitFeatureList(const Vector<RefPtr<API::Object>>& features)
        : items(features.map(webkitFeatureCreate))
    {
    }

    ~_WebKitFeatureList()
    {
        forEach(items, webkit_feature_unref);
    }

    Vector<WebKitFeature*> items;
    int referenceCount { 1 };
};

G_DEFINE_BOXED_TYPE(WebKitFeatureList, webkit_feature_list, webkit_feature_list_ref, webkit_feature_list_unref)

WebKitFeatureList* webkitFeatureListCreate(const Vector<RefPtr<API::Object>>& features)
{
    WebKitFeatureList* featureList = static_cast<WebKitFeatureList*>(fastMalloc(sizeof(WebKitFeatureList)));
    new (featureList) WebKitFeatureList(features);
    return featureList;
}

/**
 * webkit_feature_list_ref:
 * @feature_list: a #WebKitFeatureList
 *
 * Atomically acquires a reference on the given @feature_list.
 *
 * This function is MT-safe and may be called from any thread.
 *
 * Returns: The same @feature_list with an additional reference.
 *
 * Since: 2.42
 */
WebKitFeatureList* webkit_feature_list_ref(WebKitFeatureList* featureList)
{
    g_return_val_if_fail(featureList, nullptr);
    g_atomic_int_inc(&featureList->referenceCount);
    return featureList;
}

/**
 * webkit_feature_list_unref:
 * @feature_list: a #WebKitFeatureList
 *
 * Atomically releases a reference on the given @feature_list.
 *
 * If the reference was the last, the resources associated to the
 * @feature_list are freed. This function is MT-safe and may be called
 * from any thread.
 *
 * Since: 2.42
 */
void webkit_feature_list_unref(WebKitFeatureList* featureList)
{
    g_return_if_fail(featureList);
    if (g_atomic_int_dec_and_test(&featureList->referenceCount)) {
        featureList->~WebKitFeatureList();
        fastFree(featureList);
    }
}

/**
 * webkit_feature_list_get_length:
 * @feature_list: a #WebKitFeatureList
 *
 * Gets the number of elements in the feature list.
 *
 * Returns: number of elements.
 *
 * Since 2.42
 */
gsize webkit_feature_list_get_length(WebKitFeatureList* featureList)
{
    g_return_val_if_fail(featureList, 0);
    return featureList->items.size();
}

/**
 * webkit_feature_list_get:
 * @feature_list: a #WebKitFeatureList
 * @index: index of the feature
 *
 * Gets a feature given its index.
 *
 * Returns: (transfer none): The feature at @index.
 *
 * Since: 2.42
 */
WebKitFeature* webkit_feature_list_get(WebKitFeatureList* featureList, gsize index)
{
    g_return_val_if_fail(featureList, nullptr);
    g_return_val_if_fail(index < featureList->items.size(), nullptr);
    return featureList->items[index];
}
