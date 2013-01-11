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

#include "EwkViewImpl.h"
#include "ewk_settings_private.h"
#include <WebKit2/WebPageGroup.h>
#include <WebKit2/WebPageProxy.h>
#include <WebKit2/WebPreferences.h>

#if ENABLE(SPELLCHECK)
#include "WKTextChecker.h"
#include "ewk_text_checker_private.h"
#include <Ecore.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#endif

using namespace WebKit;

const WebKit::WebPreferences* EwkSettings::preferences() const
{
    return m_viewImpl->page()->pageGroup()->preferences();
}

WebKit::WebPreferences* EwkSettings::preferences()
{
    return m_viewImpl->page()->pageGroup()->preferences();
}

#if ENABLE(SPELLCHECK)
static struct {
    bool isContinuousSpellCheckingEnabled : 1;
    Vector<String> spellCheckingLanguages;
    Ewk_Settings_Continuous_Spell_Checking_Change_Cb onContinuousSpellChecking;
} ewkTextCheckerSettings = { false, Vector<String>(), 0 };

static Eina_Bool onContinuousSpellCheckingIdler(void*)
{
    if (ewkTextCheckerSettings.onContinuousSpellChecking)
        ewkTextCheckerSettings.onContinuousSpellChecking(ewkTextCheckerSettings.isContinuousSpellCheckingEnabled);

    return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool spellCheckingLanguagesSetUpdate(void*)
{
    // FIXME: Consider to delegate calling of this method in WebProcess to do not delay/block UIProcess.
    Ewk_Text_Checker::updateSpellCheckingLanguages(ewkTextCheckerSettings.spellCheckingLanguages);
    return ECORE_CALLBACK_CANCEL;
}

static void spellCheckingLanguagesSet(const Vector<String>& newLanguages)
{
    ewkTextCheckerSettings.spellCheckingLanguages = newLanguages;
    ecore_idler_add(spellCheckingLanguagesSetUpdate, 0);
}
#endif // ENABLE(SPELLCHECK)

Eina_Bool ewk_settings_fullscreen_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
#if ENABLE(FULLSCREEN_API)
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);
    settings->preferences()->setFullScreenEnabled(enable);
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
    return settings->preferences()->fullScreenEnabled();
#else
    UNUSED_PARAM(settings);
    return false;
#endif
}

Eina_Bool ewk_settings_javascript_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    settings->preferences()->setJavaScriptEnabled(enable);

    return true;
}

Eina_Bool ewk_settings_javascript_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return settings->preferences()->javaScriptEnabled();
}

Eina_Bool ewk_settings_loads_images_automatically_set(Ewk_Settings* settings, Eina_Bool automatic)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    settings->preferences()->setLoadsImagesAutomatically(automatic);

    return true;
}

Eina_Bool ewk_settings_loads_images_automatically_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return settings->preferences()->loadsImagesAutomatically();
}

Eina_Bool ewk_settings_developer_extras_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    settings->preferences()->setDeveloperExtrasEnabled(enable);

    return true;
}

Eina_Bool ewk_settings_developer_extras_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return settings->preferences()->developerExtrasEnabled();
}

Eina_Bool ewk_settings_file_access_from_file_urls_allowed_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    settings->preferences()->setAllowFileAccessFromFileURLs(enable);

    return true;
}

Eina_Bool ewk_settings_file_access_from_file_urls_allowed_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return settings->preferences()->allowFileAccessFromFileURLs();
}

Eina_Bool ewk_settings_frame_flattening_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    settings->preferences()->setFrameFlatteningEnabled(enable);

    return true;
}

Eina_Bool ewk_settings_frame_flattening_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return settings->preferences()->frameFlatteningEnabled();
}

Eina_Bool ewk_settings_dns_prefetching_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    settings->preferences()->setDNSPrefetchingEnabled(enable);

    return true;
}

Eina_Bool ewk_settings_dns_prefetching_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return settings->preferences()->dnsPrefetchingEnabled();
}

void ewk_settings_continuous_spell_checking_change_cb_set(Ewk_Settings_Continuous_Spell_Checking_Change_Cb callback)
{
#if ENABLE(SPELLCHECK)
    ewkTextCheckerSettings.onContinuousSpellChecking = callback;
#endif
}

Eina_Bool ewk_settings_continuous_spell_checking_enabled_get()
{
#if ENABLE(SPELLCHECK)
    return ewkTextCheckerSettings.isContinuousSpellCheckingEnabled;
#else
    return false;
#endif
}

void ewk_settings_continuous_spell_checking_enabled_set(Eina_Bool enable)
{
#if ENABLE(SPELLCHECK)
    enable = !!enable;
    if (ewkTextCheckerSettings.isContinuousSpellCheckingEnabled != enable) {
        ewkTextCheckerSettings.isContinuousSpellCheckingEnabled = enable;

        WKTextCheckerContinuousSpellCheckingEnabledStateChanged(enable);

        // Sets the default language if user didn't specify any.
        if (enable && !Ewk_Text_Checker::hasDictionary())
            spellCheckingLanguagesSet(Vector<String>());

        if (ewkTextCheckerSettings.onContinuousSpellChecking)
            ecore_idler_add(onContinuousSpellCheckingIdler, 0);
    }
#endif
}

Eina_List* ewk_settings_spell_checking_available_languages_get()
{
    Eina_List* listOflanguages = 0;
#if ENABLE(SPELLCHECK)
    const Vector<String>& languages = Ewk_Text_Checker::availableSpellCheckingLanguages();
    size_t numberOfLanuages = languages.size();

    for (size_t i = 0; i < numberOfLanuages; ++i)
        listOflanguages = eina_list_append(listOflanguages, eina_stringshare_add(languages[i].utf8().data()));
#endif
    return listOflanguages;
}

void ewk_settings_spell_checking_languages_set(const char* languages)
{
#if ENABLE(SPELLCHECK)
    Vector<String> newLanguages;
    String::fromUTF8(languages).split(',', newLanguages);

    spellCheckingLanguagesSet(newLanguages);
#endif
}

Eina_List* ewk_settings_spell_checking_languages_get()
{
    Eina_List* listOflanguages = 0;
#if ENABLE(SPELLCHECK)
    Vector<String> languages = Ewk_Text_Checker::loadedSpellCheckingLanguages();
    size_t numberOfLanuages = languages.size();

    for (size_t i = 0; i < numberOfLanuages; ++i)
        listOflanguages = eina_list_append(listOflanguages, eina_stringshare_add(languages[i].utf8().data()));

#endif
    return listOflanguages;
}

Eina_Bool ewk_settings_encoding_detector_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    settings->preferences()->setUsesEncodingDetector(enable);

    return true;
}

Eina_Bool ewk_settings_encoding_detector_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return settings->preferences()->usesEncodingDetector();
}

Eina_Bool ewk_settings_preferred_minimum_contents_width_set(Ewk_Settings *settings, unsigned width)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    settings->preferences()->setLayoutFallbackWidth(width);

    return true;
}

unsigned ewk_settings_preferred_minimum_contents_width_get(const Ewk_Settings *settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return settings->preferences()->layoutFallbackWidth();
}

Eina_Bool ewk_settings_offline_web_application_cache_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);
    settings->preferences()->setOfflineWebApplicationCacheEnabled(enable);

    return true;
}

Eina_Bool ewk_settings_offline_web_application_cache_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return settings->preferences()->offlineWebApplicationCacheEnabled();
}

Eina_Bool ewk_settings_scripts_can_open_windows_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);
    settings->preferences()->setJavaScriptCanOpenWindowsAutomatically(enable);

    return true;
}

Eina_Bool ewk_settings_scripts_can_open_windows_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return settings->preferences()->javaScriptCanOpenWindowsAutomatically();
}

Eina_Bool ewk_settings_local_storage_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    settings->preferences()->setLocalStorageEnabled(enable);

    return true;
}

Eina_Bool ewk_settings_local_storage_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return settings->preferences()->localStorageEnabled();
}

Eina_Bool ewk_settings_plugins_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    settings->preferences()->setPluginsEnabled(enable);

    return true;
}

Eina_Bool ewk_settings_plugins_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return settings->preferences()->pluginsEnabled();
}

Eina_Bool ewk_settings_default_font_size_set(Ewk_Settings* settings, int size)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    settings->preferences()->setDefaultFontSize(size);

    return true;
}

int ewk_settings_default_font_size_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, 0);

    return settings->preferences()->defaultFontSize();
}

Eina_Bool ewk_settings_private_browsing_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    settings->preferences()->setPrivateBrowsingEnabled(enable);

    return true;
}

Eina_Bool ewk_settings_private_browsing_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return settings->preferences()->privateBrowsingEnabled();
}

Eina_Bool ewk_settings_text_autosizing_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
#if ENABLE(TEXT_AUTOSIZING)
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    settings->preferences()->setTextAutosizingEnabled(enable);

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

    return settings->preferences()->textAutosizingEnabled();
#else
    UNUSED_PARAM(settings);
    return false;
#endif
}

