/*
 * Copyright (C) 2012 Samsung Electronics
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "ewk_settings.h"

#include "EwkView.h"
#include "WKAPICast.h"
#include "WKPreferencesRef.h"
#include "WKString.h"
#include "WebPreferences.h"
#include "ewk_settings_private.h"

using namespace WebKit;

Eina_Bool ewk_settings_fullscreen_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
#if ENABLE(FULLSCREEN_API)
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);
    WKPreferencesSetFullScreenEnabled(settings->preferences(), enable);
    return true;
#else
    UNUSED_PARAM(settings);
    UNUSED_PARAM(enable);
    return false;
#endif
}

Eina_Bool ewk_settings_fullscreen_enabled_get(const Ewk_Settings* settings)
{
#if ENABLE(FULLSCREEN_API)
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);
    return WKPreferencesGetFullScreenEnabled(settings->preferences());
#else
    UNUSED_PARAM(settings);
    return false;
#endif
}

Eina_Bool ewk_settings_javascript_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetJavaScriptEnabled(settings->preferences(), enable);

    return true;
}

Eina_Bool ewk_settings_javascript_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return WKPreferencesGetJavaScriptEnabled(settings->preferences());
}

Eina_Bool ewk_settings_loads_images_automatically_set(Ewk_Settings* settings, Eina_Bool automatic)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetLoadsImagesAutomatically(settings->preferences(), automatic);

    return true;
}

Eina_Bool ewk_settings_loads_images_automatically_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return WKPreferencesGetLoadsImagesAutomatically(settings->preferences());
}

Eina_Bool ewk_settings_developer_extras_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetDeveloperExtrasEnabled(settings->preferences(), enable);

    return true;
}

Eina_Bool ewk_settings_developer_extras_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return WKPreferencesGetDeveloperExtrasEnabled(settings->preferences());
}

Eina_Bool ewk_settings_file_access_from_file_urls_allowed_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetFileAccessFromFileURLsAllowed(settings->preferences(), enable);

    return true;
}

Eina_Bool ewk_settings_file_access_from_file_urls_allowed_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return WKPreferencesGetFileAccessFromFileURLsAllowed(settings->preferences());
}

Eina_Bool ewk_settings_frame_flattening_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetFrameFlatteningEnabled(settings->preferences(), enable);

    return true;
}

Eina_Bool ewk_settings_frame_flattening_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return WKPreferencesGetFrameFlatteningEnabled(settings->preferences());
}

Eina_Bool ewk_settings_dns_prefetching_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetDNSPrefetchingEnabled(settings->preferences(), enable);

    return true;
}

Eina_Bool ewk_settings_dns_prefetching_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return WKPreferencesGetDNSPrefetchingEnabled(settings->preferences());
}

Eina_Bool ewk_settings_encoding_detector_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetEncodingDetectorEnabled(settings->preferences(), enable);

    return true;
}

Eina_Bool ewk_settings_encoding_detector_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return WKPreferencesGetEncodingDetectorEnabled(settings->preferences());
}

Eina_Bool ewk_settings_default_text_encoding_name_set(Ewk_Settings* settings, const char* encoding)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    settings->setDefaultTextEncodingName(encoding);

    return true;
}

const char* ewk_settings_default_text_encoding_name_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, nullptr);

    return settings->defaultTextEncodingName();
}

Eina_Bool ewk_settings_preferred_minimum_contents_width_set(Ewk_Settings *settings, unsigned width)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    toImpl(settings->preferences())->setLayoutFallbackWidth(width);

    return true;
}

unsigned ewk_settings_preferred_minimum_contents_width_get(const Ewk_Settings *settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return toImpl(settings->preferences())->layoutFallbackWidth();
}

Eina_Bool ewk_settings_offline_web_application_cache_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetOfflineWebApplicationCacheEnabled(settings->preferences(), enable);

    return true;
}

Eina_Bool ewk_settings_offline_web_application_cache_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return WKPreferencesGetOfflineWebApplicationCacheEnabled(settings->preferences());
}

Eina_Bool ewk_settings_scripts_can_open_windows_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetJavaScriptCanOpenWindowsAutomatically(settings->preferences(), enable);

    return true;
}

Eina_Bool ewk_settings_scripts_can_open_windows_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return WKPreferencesGetJavaScriptCanOpenWindowsAutomatically(settings->preferences());
}

Eina_Bool ewk_settings_local_storage_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetLocalStorageEnabled(settings->preferences(), enable);

    return true;
}

Eina_Bool ewk_settings_local_storage_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return WKPreferencesGetLocalStorageEnabled(settings->preferences());
}

Eina_Bool ewk_settings_plugins_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetPluginsEnabled(settings->preferences(), enable);

    return true;
}

Eina_Bool ewk_settings_plugins_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return WKPreferencesGetPluginsEnabled(settings->preferences());
}

Eina_Bool ewk_settings_default_font_size_set(Ewk_Settings* settings, int size)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetDefaultFontSize(settings->preferences(), size);

    return true;
}

int ewk_settings_default_font_size_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, 0);

    return WKPreferencesGetDefaultFontSize(settings->preferences());
}

Eina_Bool ewk_settings_private_browsing_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetPrivateBrowsingEnabled(settings->preferences(), enable);

    return true;
}

Eina_Bool ewk_settings_private_browsing_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return WKPreferencesGetPrivateBrowsingEnabled(settings->preferences());
}

Eina_Bool ewk_settings_text_autosizing_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
#if ENABLE(TEXT_AUTOSIZING)
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetTextAutosizingEnabled(settings->preferences(), enable);

    return true;
#else
    UNUSED_PARAM(settings);
    UNUSED_PARAM(enable);
    return false;
#endif
}

Eina_Bool ewk_settings_text_autosizing_enabled_get(const Ewk_Settings* settings)
{
#if ENABLE(TEXT_AUTOSIZING)
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return WKPreferencesGetTextAutosizingEnabled(settings->preferences());
#else
    UNUSED_PARAM(settings);
    return false;
#endif
}

Eina_Bool ewk_settings_spatial_navigation_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetSpatialNavigationEnabled(settings->preferences(), enable);

    return true;
}

Eina_Bool ewk_settings_spatial_navigation_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return WKPreferencesGetSpatialNavigationEnabled(settings->preferences());
}

void EwkSettings::setDefaultTextEncodingName(const char* name)
{
    if (m_defaultTextEncodingName == name)
        return;

    WKPreferencesSetDefaultTextEncodingName(preferences(), WKStringCreateWithUTF8CString(name));
    m_defaultTextEncodingName = name;
}

const char* EwkSettings::defaultTextEncodingName() const
{
    return m_defaultTextEncodingName;
}
