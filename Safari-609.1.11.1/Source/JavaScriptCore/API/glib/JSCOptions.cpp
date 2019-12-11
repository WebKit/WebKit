/*
 * Copyright (C) 2019 Igalia S.L.
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
#include "JSCOptions.h"

#include "Options.h"
#include <glib/gi18n-lib.h>
#include <wtf/Vector.h>
#include <wtf/glib/GUniquePtr.h>

/**
 * SECTION: JSCOptions
 * @short_description: JavaScript options
 * @title: JSCOptions
 *
 * JavaScript options allow changing the behavior of the JavaScript engine.
 * They affect the way the engine works, so the options must be set
 * at the very beginning of the program execution, before any other JavaScript
 * API call. Most of the options are only useful for testing and debugging.
 * Only a few of them are documented; you can use the undocumented options at
 * your own risk. (You can find the list of options in the WebKit source code).
 *
 * The API allows to set and get any option using the types defined in #JSCOptionType.
 * You can also iterate all the available options using jsc_options_foreach() and
 * passing a #JSCOptionsFunc callback. If your application uses #GOptionContext to handle
 * command line arguments, you can easily integrate the JSCOptions by adding the
 * #GOptionGroup returned by jsc_options_get_option_group().
 *
 * Since: 2.24
 */

using namespace JSC;

using int32 = int32_t;
using size = size_t;

static bool valueFromGValue(const GValue* gValue, bool& value)
{
    value = g_value_get_boolean(gValue);
    return true;
}

static void valueToGValue(bool value, GValue* gValue)
{
    g_value_set_boolean(gValue, value);
}

static bool valueFromGValue(const GValue* gValue, int32_t& value)
{
    value = g_value_get_int(gValue);
    return true;
}

static void valueToGValue(int32_t value, GValue* gValue)
{
    g_value_set_int(gValue, value);
}

#if CPU(ADDRESS64)
static bool valueFromGValue(const GValue* gValue, unsigned& value)
{
    value = g_value_get_uint(gValue);
    return true;
}

static void valueToGValue(unsigned value, GValue* gValue)
{
    g_value_set_uint(gValue, value);
}
#endif

static bool valueFromGValue(const GValue* gValue, size_t& value)
{
    value = GPOINTER_TO_SIZE(g_value_get_pointer(gValue));
    return true;
}

static void valueToGValue(size_t value, GValue* gValue)
{
    g_value_set_pointer(gValue, GSIZE_TO_POINTER(value));
}

static bool valueFromGValue(const GValue* gValue, const char*& value)
{
    value = g_value_dup_string(gValue);
    return true;
}

static void valueToGValue(const char* value, GValue* gValue)
{
    g_value_set_string(gValue, value);
}

static bool valueFromGValue(const GValue* gValue, double& value)
{
    value = g_value_get_double(gValue);
    return true;
}

static void valueToGValue(double value, GValue* gValue)
{
    g_value_set_double(gValue, value);
}

static bool valueFromGValue(const GValue* gValue, OptionRange& value)
{
    return value.init(g_value_get_string(gValue) ? g_value_get_string(gValue) : "<null>");
}

static void valueToGValue(const OptionRange& value, GValue* gValue)
{
    const char* rangeString = value.rangeString();
    g_value_set_string(gValue, !g_strcmp0(rangeString, "<null>") ? nullptr : rangeString);
}

static bool valueFromGValue(const GValue* gValue, GCLogging::Level& value)
{
    switch (g_value_get_uint(gValue)) {
    case 0:
        value = GCLogging::Level::None;
        return true;
    case 1:
        value = GCLogging::Level::Basic;
        return true;
    case 2:
        value = GCLogging::Level::Verbose;
        return true;
    default:
        break;
    }

    return false;
}

static void valueToGValue(GCLogging::Level value, GValue* gValue)
{
    switch (value) {
    case GCLogging::Level::None:
        g_value_set_uint(gValue, 0);
        break;
    case GCLogging::Level::Basic:
        g_value_set_uint(gValue, 1);
        break;
    case GCLogging::Level::Verbose:
        g_value_set_uint(gValue, 2);
        break;
    }
}

static gboolean jscOptionsSetValue(const char* option, const GValue* value)
{
#define SET_OPTION_VALUE(type_, name_, defaultValue_, availability_, description_) \
    if (!g_strcmp0(#name_, option)) {                                   \
        OptionsStorage::type_ valueToSet;                                  \
        if (!valueFromGValue(value, valueToSet))                        \
            return FALSE;                                               \
        Options::name_() = valueToSet;                                  \
        return TRUE;                                                    \
    }

    Options::initialize();
    FOR_EACH_JSC_OPTION(SET_OPTION_VALUE)
#undef SET_OPTION_VALUE

    return FALSE;
}

static gboolean jscOptionsGetValue(const char* option, GValue* value)
{
#define GET_OPTION_VALUE(type_, name_, defaultValue_, availability_, description_) \
    if (!g_strcmp0(#name_, option)) {                                   \
        OptionsStorage::type_ valueToGet = Options::name_();               \
        valueToGValue(valueToGet, value);                               \
        return TRUE;                                                    \
    }

    Options::initialize();
    FOR_EACH_JSC_OPTION(GET_OPTION_VALUE)
#undef GET_OPTION_VALUE

    return FALSE;
}

/**
 * jsc_options_set_boolean:
 * @option: the option identifier
 * @value: the value to set
 *
 * Set @option as a #gboolean value.
 *
 * Returns: %TRUE if option was correctly set or %FALSE otherwise.
 *
 * Since: 2.24
 */
gboolean jsc_options_set_boolean(const char* option, gboolean value)
{
    g_return_val_if_fail(option, FALSE);

    GValue gValue = G_VALUE_INIT;
    g_value_init(&gValue, G_TYPE_BOOLEAN);
    g_value_set_boolean(&gValue, value);
    return jscOptionsSetValue(option, &gValue);
}

/**
 * jsc_options_get_boolean:
 * @option: the option identifier
 * @value: (out): return location for the option value
 *
 * Get @option as a #gboolean value.
 *
 * Returns: %TRUE if @value has been set or %FALSE if the option doesn't exist
 *
 * Since: 2.24
 */
gboolean jsc_options_get_boolean(const char* option, gboolean* value)
{
    g_return_val_if_fail(option, FALSE);
    g_return_val_if_fail(value, FALSE);

    GValue gValue = G_VALUE_INIT;
    g_value_init(&gValue, G_TYPE_BOOLEAN);
    if (!jscOptionsGetValue(option, &gValue))
        return FALSE;

    *value =  g_value_get_boolean(&gValue);
    return TRUE;
}

/**
 * jsc_options_set_int:
 * @option: the option identifier
 * @value: the value to set
 *
 * Set @option as a #gint value.
 *
 * Returns: %TRUE if option was correctly set or %FALSE otherwise.
 *
 * Since: 2.24
 */
gboolean jsc_options_set_int(const char* option, gint value)
{
    g_return_val_if_fail(option, FALSE);

    GValue gValue = G_VALUE_INIT;
    g_value_init(&gValue, G_TYPE_INT);
    g_value_set_int(&gValue, value);
    return jscOptionsSetValue(option, &gValue);
}

/**
 * jsc_options_get_int:
 * @option: the option identifier
 * @value: (out): return location for the option value
 *
 * Get @option as a #gint value.
 *
 * Returns: %TRUE if @value has been set or %FALSE if the option doesn't exist
 *
 * Since: 2.24
 */
gboolean jsc_options_get_int(const char* option, gint* value)
{
    g_return_val_if_fail(option, FALSE);
    g_return_val_if_fail(value, FALSE);

    GValue gValue = G_VALUE_INIT;
    g_value_init(&gValue, G_TYPE_INT);
    if (!jscOptionsGetValue(option, &gValue))
        return FALSE;

    *value = g_value_get_int(&gValue);
    return TRUE;
}

/**
 * jsc_options_set_uint:
 * @option: the option identifier
 * @value: the value to set
 *
 * Set @option as a #guint value.
 *
 * Returns: %TRUE if option was correctly set or %FALSE otherwise.
 *
 * Since: 2.24
 */
gboolean jsc_options_set_uint(const char* option, guint value)
{
    g_return_val_if_fail(option, FALSE);

    GValue gValue = G_VALUE_INIT;
    g_value_init(&gValue, G_TYPE_UINT);
    g_value_set_uint(&gValue, value);
    return jscOptionsSetValue(option, &gValue);
}

/**
 * jsc_options_get_uint:
 * @option: the option identifier
 * @value: (out): return location for the option value
 *
 * Get @option as a #guint value.
 *
 * Returns: %TRUE if @value has been set or %FALSE if the option doesn't exist
 *
 * Since: 2.24
 */
gboolean jsc_options_get_uint(const char* option, guint* value)
{
    g_return_val_if_fail(option, FALSE);
    g_return_val_if_fail(value, FALSE);

    GValue gValue = G_VALUE_INIT;
    g_value_init(&gValue, G_TYPE_UINT);
    if (!jscOptionsGetValue(option, &gValue))
        return FALSE;

    *value = g_value_get_uint(&gValue);
    return TRUE;
}

/**
 * jsc_options_set_size:
 * @option: the option identifier
 * @value: the value to set
 *
 * Set @option as a #gsize value.
 *
 * Returns: %TRUE if option was correctly set or %FALSE otherwise.
 *
 * Since: 2.24
 */
gboolean jsc_options_set_size(const char* option, gsize value)
{
    g_return_val_if_fail(option, FALSE);

    GValue gValue = G_VALUE_INIT;
    g_value_init(&gValue, G_TYPE_POINTER);
    g_value_set_pointer(&gValue, GSIZE_TO_POINTER(value));
    return jscOptionsSetValue(option, &gValue);
}

/**
 * jsc_options_get_size:
 * @option: the option identifier
 * @value: (out): return location for the option value
 *
 * Get @option as a #gsize value.
 *
 * Returns: %TRUE if @value has been set or %FALSE if the option doesn't exist
 *
 * Since: 2.24
 */
gboolean jsc_options_get_size(const char* option, gsize* value)
{
    g_return_val_if_fail(option, FALSE);
    g_return_val_if_fail(value, FALSE);

    GValue gValue = G_VALUE_INIT;
    g_value_init(&gValue, G_TYPE_POINTER);
    if (!jscOptionsGetValue(option, &gValue))
        return FALSE;

    *value = GPOINTER_TO_SIZE(g_value_get_pointer(&gValue));
    return TRUE;
}

/**
 * jsc_options_set_double:
 * @option: the option identifier
 * @value: the value to set
 *
 * Set @option as a #gdouble value.
 *
 * Returns: %TRUE if option was correctly set or %FALSE otherwise.
 *
 * Since: 2.24
 */
gboolean jsc_options_set_double(const char* option, gdouble value)
{
    g_return_val_if_fail(option, FALSE);

    GValue gValue = G_VALUE_INIT;
    g_value_init(&gValue, G_TYPE_DOUBLE);
    g_value_set_double(&gValue, value);
    return jscOptionsSetValue(option, &gValue);
}

/**
 * jsc_options_get_double:
 * @option: the option identifier
 * @value: (out): return location for the option value
 *
 * Get @option as a #gdouble value.
 *
 * Returns: %TRUE if @value has been set or %FALSE if the option doesn't exist
 *
 * Since: 2.24
 */
gboolean jsc_options_get_double(const char* option, gdouble* value)
{
    g_return_val_if_fail(option, FALSE);
    g_return_val_if_fail(value, FALSE);

    GValue gValue = G_VALUE_INIT;
    g_value_init(&gValue, G_TYPE_DOUBLE);
    if (!jscOptionsGetValue(option, &gValue))
        return FALSE;

    *value = g_value_get_double(&gValue);
    return TRUE;
}

/**
 * jsc_options_set_string:
 * @option: the option identifier
 * @value: the value to set
 *
 * Set @option as a string.
 *
 * Returns: %TRUE if option was correctly set or %FALSE otherwise.
 *
 * Since: 2.24
 */
gboolean jsc_options_set_string(const char* option, const char* value)
{
    g_return_val_if_fail(option, FALSE);

    GValue gValue = G_VALUE_INIT;
    g_value_init(&gValue, G_TYPE_STRING);
    g_value_set_string(&gValue, value);
    bool success = jscOptionsSetValue(option, &gValue);
    g_value_unset(&gValue);
    return success;
}

/**
 * jsc_options_get_string:
 * @option: the option identifier
 * @value: (out): return location for the option value
 *
 * Get @option as a string.
 *
 * Returns: %TRUE if @value has been set or %FALSE if the option doesn't exist
 *
 * Since: 2.24
 */
gboolean jsc_options_get_string(const char* option, char** value)
{
    g_return_val_if_fail(option, FALSE);
    g_return_val_if_fail(value, FALSE);

    GValue gValue = G_VALUE_INIT;
    g_value_init(&gValue, G_TYPE_STRING);
    if (!jscOptionsGetValue(option, &gValue))
        return FALSE;

    *value = g_value_dup_string(&gValue);
    g_value_unset(&gValue);
    return TRUE;
}

/**
 * jsc_options_set_range_string:
 * @option: the option identifier
 * @value: the value to set
 *
 * Set @option as a range string. The string must be in the
 * format <emphasis>[!]&lt;low&gt;[:&lt;high&gt;]</emphasis> where low and high are #guint values.
 * Values between low and high (both included) will be considered in
 * the range, unless <emphasis>!</emphasis> is used to invert the range.
 *
 * Returns: %TRUE if option was correctly set or %FALSE otherwise.
 *
 * Since: 2.24
 */
gboolean jsc_options_set_range_string(const char* option, const char* value)
{
    g_return_val_if_fail(option, FALSE);

    GValue gValue = G_VALUE_INIT;
    g_value_init(&gValue, G_TYPE_STRING);
    g_value_set_string(&gValue, value);
    bool success = jscOptionsSetValue(option, &gValue);
    g_value_unset(&gValue);
    return success;
}

/**
 * jsc_options_get_range_string:
 * @option: the option identifier
 * @value: (out): return location for the option value
 *
 * Get @option as a range string. The string must be in the
 * format <emphasis>[!]&lt;low&gt;[:&lt;high&gt;]</emphasis> where low and high are #guint values.
 * Values between low and high (both included) will be considered in
 * the range, unless <emphasis>!</emphasis> is used to invert the range.
 *
 * Returns: %TRUE if @value has been set or %FALSE if the option doesn't exist
 *
 * Since: 2.24
 */
gboolean jsc_options_get_range_string(const char* option, char** value)
{
    g_return_val_if_fail(option, FALSE);
    g_return_val_if_fail(value, FALSE);

    GValue gValue = G_VALUE_INIT;
    g_value_init(&gValue, G_TYPE_STRING);
    if (!jscOptionsGetValue(option, &gValue))
        return FALSE;

    *value = g_value_dup_string(&gValue);
    g_value_unset(&gValue);
    return TRUE;
}

static JSCOptionType jscOptionsType(bool)
{
    return JSC_OPTION_BOOLEAN;
}

static JSCOptionType jscOptionsType(int)
{
    return JSC_OPTION_INT;
}

#if CPU(ADDRESS64)
static JSCOptionType jscOptionsType(unsigned)
{
    return JSC_OPTION_UINT;
}
#endif

static JSCOptionType jscOptionsType(size_t)
{
    return JSC_OPTION_SIZE;
}

static JSCOptionType jscOptionsType(double)
{
    return JSC_OPTION_DOUBLE;
}

static JSCOptionType jscOptionsType(const char*)
{
    return JSC_OPTION_STRING;
}

static JSCOptionType jscOptionsType(const OptionRange&)
{
    return JSC_OPTION_RANGE_STRING;
}

/**
 * JSCOptionType:
 * @JSC_OPTION_BOOLEAN: A #gboolean option type.
 * @JSC_OPTION_INT: A #gint option type.
 * @JSC_OPTION_UINT: A #guint option type.
 * @JSC_OPTION_SIZE: A #gsize options type.
 * @JSC_OPTION_DOUBLE: A #gdouble options type.
 * @JSC_OPTION_STRING: A string option type.
 * @JSC_OPTION_RANGE_STRING: A range string option type.
 *
 * Enum values for options types.
 *
 * Since: 2.24
 */

/**
 * JSCOptionsFunc:
 * @option: the option name
 * @type: the option #JSCOptionType
 * @description: (nullable): the option description, or %NULL
 * @user_data: user data
 *
 * Function used to iterate options.
 *
 * Not that @description string is not localized.
 *
 * Returns: %TRUE to stop the iteration, or %FALSE otherwise
 *
 * Since: 2.24
 */

/**
 * jsc_options_foreach:
 * @function: (scope call): a #JSCOptionsFunc callback
 * @user_data: callback user data
 *
 * Iterates all available options calling @function for each one. Iteration can
 * stop early if @function returns %FALSE.
 *
 * Since: 2.24
 */
void jsc_options_foreach(JSCOptionsFunc function, gpointer userData)
{
    g_return_if_fail(function);

#define VISIT_OPTION(type_, name_, defaultValue_, availability_, description_) \
    if (Options::Availability::availability_ == Options::Availability::Normal \
        || Options::isAvailable(Options::name_##ID, Options::Availability::availability_)) { \
        OptionsStorage::type_ defaultValue { };                            \
        auto optionType = jscOptionsType(defaultValue);                 \
        if (function (#name_, optionType, description_, userData))      \
            return;                                                     \
    }

    Options::initialize();
    FOR_EACH_JSC_OPTION(VISIT_OPTION)
#undef VISIT_OPTION
}

static gboolean setOptionEntry(const char* optionNameFull, const char* value, gpointer, GError** error)
{
    const char* optionName = optionNameFull + 6; // Remove the --jsc- prefix.
    GUniquePtr<char> option(g_strdup_printf("%s=%s", optionName, value));
    if (!Options::setOption(option.get())) {
        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "Failed parse value '%s' for %s", value, optionNameFull);
        return FALSE;
    }
    return TRUE;
}

/**
 * jsc_options_get_option_group:
 *
 * Create a #GOptionGroup to handle JSCOptions as command line arguments.
 * The options will be exposed as command line arguments with the form
 * <emphasis>--jsc-&lt;option&gt;=&lt;value&gt;</emphasis>.
 * Each entry in the returned #GOptionGroup is configured to apply the
 * corresponding option during command line parsing. Applications only need to
 * pass the returned group to g_option_context_add_group(), and the rest will
 * be taken care for automatically.
 *
 * Returns: (transfer full): a #GOptionGroup for the JSCOptions
 *
 * Since: 2.24
 */
GOptionGroup* jsc_options_get_option_group(void)
{
    // GOptionEntry works with const strings, so we need to keep the option names around.
    auto* names = new Vector<GUniquePtr<char>>;
    GOptionGroup* group = g_option_group_new("jsc", _("JSC Options"), _("Show JSC Options"), names, [] (gpointer data) {
        delete static_cast<Vector<GUniquePtr<char>>*>(data);
    });
    g_option_group_set_translation_domain(group, GETTEXT_PACKAGE);

    GArray* entries = g_array_new(TRUE, TRUE, sizeof(GOptionEntry));
#define REGISTER_OPTION(type_, name_, defaultValue_, availability_, description_) \
    if (Options::Availability::availability_ == Options::Availability::Normal \
        || Options::isAvailable(Options::name_##ID, Options::Availability::availability_)) { \
        GUniquePtr<char> name(g_strdup_printf("jsc-%s", #name_));       \
        entries = g_array_set_size(entries, entries->len + 1); \
        GOptionEntry* entry = &g_array_index(entries, GOptionEntry, entries->len - 1); \
        entry->long_name = name.get();                                  \
        entry->arg = G_OPTION_ARG_CALLBACK;                             \
        entry->arg_data = reinterpret_cast<gpointer>(setOptionEntry);   \
        entry->description = description_;                              \
        names->append(WTFMove(name));                                   \
    }

    Options::initialize();
    FOR_EACH_JSC_OPTION(REGISTER_OPTION)
#undef REGISTER_OPTION

    g_option_group_add_entries(group, reinterpret_cast<GOptionEntry*>(entries->data));
    return group;
}

/**
 * JSC_OPTIONS_USE_JIT:
 *
 * Allows the executable pages to be allocated for JIT and thunks if %TRUE.
 * Option type: %JSC_OPTION_BOOLEAN
 * Default value: %TRUE.
 *
 * Since: 2.24
 */

/**
 * JSC_OPTIONS_USE_DFG:
 *
 * Allows the DFG JIT to be used if %TRUE.
 * Option type: %JSC_OPTION_BOOLEAN
 * Default value: %TRUE.
 *
 * Since: 2.24
 */

/**
 * JSC_OPTIONS_USE_FTL:
 *
 * Allows the FTL JIT to be used if %TRUE.
 * Option type: %JSC_OPTION_BOOLEAN
 * Default value: %TRUE.
 *
 * Since: 2.24
 */

/**
 * JSC_OPTIONS_USE_LLINT:
 *
 * Allows the LLINT to be used if %TRUE.
 * Option type: %JSC_OPTION_BOOLEAN
 * Default value: %TRUE.
 *
 * Since: 2.24
 */
