/*
 * Copyright (C) 2021 Igalia S.L.
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
#include "WebKitMemoryPressureSettings.h"

#include <wtf/MemoryPressureHandler.h>

/**
 * SECTION: WebKitMemoryPressureSettings
 * @Short_description: A boxed type representing the settings for the MemoryPressureHandler
 * @Title: WebKitMemoryPressureSettings
 * @See_also: #WebKitWebContext, #WebKitWebsiteDataManager
 *
 * #WebKitMemoryPressureSettings is a boxed type that can be used to provide some custom settings
 * to control how the memory pressure situations are handled by the different processes.
 *
 * The memory pressure system implemented inside the different process will try to keep the memory usage
 * under the defined memory limit. In order to do that, it will check the used memory with a user defined
 * frequency and decide whether it should try to release memory. The thresholds passed will define how urgent
 * is to release that memory.
 *
 * Take into account that badly defined parameters can greatly reduce the performance of the engine. For
 * example, setting memory limit too low with a fast poll interval can cause the process to constantly
 * be trying to release memory.
 *
 * A #WebKitMemoryPressureSettings can be passed to a #WebKitWebContext constructor, and the settings will
 * be applied to all the web processes created by that context.
 * 
 * A #WebKitMemoryPressureSettings can be passed to webkit_website_data_manager_set_memory_pressure_settings(),
 * and the settings will be applied to all the network processes created after that call by any instance of
 * #WebKitWebsiteDataManager.
 *
 * Since: 2.34
 */
struct _WebKitMemoryPressureSettings {
    MemoryPressureHandler::Configuration configuration;
};

G_DEFINE_BOXED_TYPE(WebKitMemoryPressureSettings, webkit_memory_pressure_settings, webkit_memory_pressure_settings_copy, webkit_memory_pressure_settings_free);

/**
 * webkit_memory_pressure_settings_new:
 *
 * Create a new #WebKitMemoryPressureSettings with the default values.
 *
 * Returns: (transfer full): A new #WebKitMemoryPressureSettings instance filled with the default values.
 *
 * Since: 2.34
 */
WebKitMemoryPressureSettings* webkit_memory_pressure_settings_new()
{
    WebKitMemoryPressureSettings* memoryPressureSettings = static_cast<WebKitMemoryPressureSettings*>(fastMalloc(sizeof(WebKitMemoryPressureSettings)));
    new (memoryPressureSettings) WebKitMemoryPressureSettings;

    return memoryPressureSettings;
}

/**
 * webkit_memory_pressure_settings_copy:
 * @settings: a #WebKitMemoryPressureSettings
 *
 * Make a copy of @settings.
 *
 * Returns: (transfer full): A copy of of the passed #WebKitMemoryPressureSettings.
 *
 * Since: 2.34
 */
WebKitMemoryPressureSettings* webkit_memory_pressure_settings_copy(WebKitMemoryPressureSettings* settings)
{
    g_return_val_if_fail(settings, nullptr);

    WebKitMemoryPressureSettings* copy = static_cast<WebKitMemoryPressureSettings*>(fastZeroedMalloc(sizeof(WebKitMemoryPressureSettings)));

    copy->configuration = settings->configuration;

    return copy;
}

/**
 * webkit_memory_pressure_settings_free:
 * @settings: a #WebKitMemoryPressureSettings
 *
 * Free the #WebKitMemoryPressureSettings.
 *
 * Since: 2.34
 */
void webkit_memory_pressure_settings_free(WebKitMemoryPressureSettings* settings)
{
    g_return_if_fail(settings);

    fastFree(settings);
}

/**
 * webkit_memory_pressure_settings_set_memory_limit:
 * @settings: a #WebKitMemoryPressureSettings
 * @memory_limit: amount of memory (in MB) that the process is allowed to use.
 *
 * Sets @memory_limit the memory limit value to @settings.
 *
 * The default value is the system's RAM size with a maximum of 3GB.
 *
 * Since: 2.34
 */
void webkit_memory_pressure_settings_set_memory_limit(WebKitMemoryPressureSettings* settings, guint memoryLimit)
{
    g_return_if_fail(settings);
    g_return_if_fail(memoryLimit);

    settings->configuration.baseThreshold = memoryLimit * MB;
}

/**
 * webkit_memory_pressure_settings_get_memory_limit:
 * @settings: a #WebKitMemoryPressureSettings
 *
 * Returns: the value in MB of the memory limit inside @settings.
 *
 * Since: 2.34
 */
guint webkit_memory_pressure_settings_get_memory_limit(WebKitMemoryPressureSettings* settings)
{
    g_return_val_if_fail(settings, 0);

    return settings->configuration.baseThreshold / MB;
}

/**
 * webkit_memory_pressure_settings_set_conservative_threshold:
 * @settings: a #WebKitMemoryPressureSettings
 * @value: fraction of the memory limit where the conservative policy starts working.
 *
 * Sets @value as the fraction of the defined memory limit where the conservative
 * policy starts working. This policy will try to reduce the memory footprint by
 * releasing non critical memory.
 *
 * The threshold must be bigger than 0 and smaller than 1, and it must be smaller
 * than the strict threshold defined in @settings. The default value is 0.33.
 *
 * Since: 2.34
 */
void webkit_memory_pressure_settings_set_conservative_threshold(WebKitMemoryPressureSettings* settings, gdouble value)
{
    g_return_if_fail(settings);
    g_return_if_fail(value > 0 && value < 1);
    g_return_if_fail(value < settings->configuration.strictThresholdFraction);

    settings->configuration.conservativeThresholdFraction = value;
}

/**
 * webkit_memory_pressure_settings_get_conservative_threshold:
 * @settings: a #WebKitMemoryPressureSettings
 *
 * Returns: the value of the the conservative threshold inside @settings.
 *
 * Since: 2.34
 */
gdouble webkit_memory_pressure_settings_get_conservative_threshold(WebKitMemoryPressureSettings* settings)
{
    g_return_val_if_fail(settings, 0);

    return settings->configuration.conservativeThresholdFraction;
}

/**
 * webkit_memory_pressure_settings_set_strict_threshold:
 * @settings: a #WebKitMemoryPressureSettings
 * @value: fraction of the memory limit where the strict policy starts working.
 *
 * Sets @value as the fraction of the defined memory limit where the strict
 * policy starts working. This policy will try to reduce the memory footprint by
 * releasing critical memory.
 *
 * The threshold must be bigger than 0 and smaller than 1. Also, it must be bigger
 * than the conservative threshold defined in @settings, and smaller than the kill
 * threshold if the latter is not 0. The default value is 0.5.
 *
 * Since: 2.34
 */
void webkit_memory_pressure_settings_set_strict_threshold(WebKitMemoryPressureSettings* settings, gdouble value)
{
    g_return_if_fail(settings);
    g_return_if_fail(value > 0 && value < 1);
    g_return_if_fail(value > settings->configuration.conservativeThresholdFraction);
    g_return_if_fail(!settings->configuration.killThresholdFraction || value < settings->configuration.killThresholdFraction);

    settings->configuration.strictThresholdFraction = value;
}

/**
 * webkit_memory_pressure_settings_get_strict_threshold:
 * @settings: a #WebKitMemoryPressureSettings
 *
 * Returns: the value of the the strict threshold inside @settings.
 *
 * Since: 2.34
 */
gdouble webkit_memory_pressure_settings_get_strict_threshold(WebKitMemoryPressureSettings* settings)
{
    g_return_val_if_fail(settings, 0);

    return settings->configuration.strictThresholdFraction;
}

/**
 * webkit_memory_pressure_settings_set_kill_threshold:
 * @settings: a #WebKitMemoryPressureSettings
 * @value: fraction of the memory limit where the process will be killed because
 *   of excessive memory usage.
 *
 * Sets @value as the fraction of the defined memory limit where the process will be
 * killed.
 *
 * The threshold must be a value bigger or equal to 0. A value of 0 means that the process
 * is never killed. If the threshold is not 0, then it must be bigger than the strict threshold
 * defined in @settings. The threshold can also have values bigger than 1. The default value is 0.
 *
 * Since: 2.34
 */
void webkit_memory_pressure_settings_set_kill_threshold(WebKitMemoryPressureSettings* settings, gdouble value)
{
    g_return_if_fail(settings);
    g_return_if_fail(value >= 0);
    g_return_if_fail(!value || value > settings->configuration.strictThresholdFraction);

    settings->configuration.killThresholdFraction = value ? std::make_optional(value) : std::nullopt;
}

/**
 * webkit_memory_pressure_settings_get_kill_threshold:
 * @settings: a #WebKitMemoryPressureSettings
 *
 * Returns: the value of the the kill threshold inside @settings.
 *
 * Since: 2.34
 */
gdouble webkit_memory_pressure_settings_get_kill_threshold(WebKitMemoryPressureSettings* settings)
{
    g_return_val_if_fail(settings, 0);

    return settings->configuration.killThresholdFraction.value_or(0);
}


/**
 * webkit_memory_pressure_settings_set_poll_interval:
 * @settings: a #WebKitMemoryPressureSettings
 * @value: period (in seconds) between memory usage measurements.
 *
 * Sets @value as the poll interval used by @settings.
 *
 * The poll interval value must be bigger than 0. The default value is 30 seconds.
 *
 * Since: 2.34
 */
void webkit_memory_pressure_settings_set_poll_interval(WebKitMemoryPressureSettings* settings, gdouble value)
{
    g_return_if_fail(settings);
    g_return_if_fail(value > 0);

    settings->configuration.pollInterval = Seconds(value);
}

/**
 * webkit_memory_pressure_settings_get_poll_interval:
 * @settings: a #WebKitMemoryPressureSettings
 *
 * Returns: the value in seconds of the the poll interval inside @settings.
 *
 * Since: 2.34
 */
gdouble webkit_memory_pressure_settings_get_poll_interval(WebKitMemoryPressureSettings* settings)
{
    g_return_val_if_fail(settings, 0);

    return settings->configuration.pollInterval.seconds();
}

const MemoryPressureHandler::Configuration& webkitMemoryPressureSettingsGetMemoryPressureHandlerConfiguration(WebKitMemoryPressureSettings* settings)
{
    return settings->configuration;
}
