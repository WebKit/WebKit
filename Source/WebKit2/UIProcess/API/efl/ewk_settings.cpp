/*
 * Copyright (C) 2012 Samsung Electronics
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

#include "ewk_settings_private.h"
#include <WebKit2/WKPreferences.h>
#include <WebKit2/WKPreferencesPrivate.h>

#if ENABLE(SPELLCHECK)
#include "WKTextChecker.h"
#include "WebKitTextChecker.h"
#include <Ecore.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#endif

using namespace WebKit;

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
    updateSpellCheckingLanguages(ewkTextCheckerSettings.spellCheckingLanguages);
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
    WKPreferencesSetFullScreenEnabled(settings->preferences.get(), enable);
    return true;
#else
    return false;
#endif
}

Eina_Bool ewk_settings_fullscreen_enabled_get(const Ewk_Settings* settings)
{
#if ENABLE(FULLSCREEN_API)
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);
    return WKPreferencesGetFullScreenEnabled(settings->preferences.get());
#else
    return false;
#endif
}

Eina_Bool ewk_settings_javascript_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetJavaScriptEnabled(settings->preferences.get(), enable);

    return true;
}

Eina_Bool ewk_settings_javascript_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return WKPreferencesGetJavaScriptEnabled(settings->preferences.get());
}

Eina_Bool ewk_settings_loads_images_automatically_set(Ewk_Settings* settings, Eina_Bool automatic)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetLoadsImagesAutomatically(settings->preferences.get(), automatic);

    return true;
}

Eina_Bool ewk_settings_loads_images_automatically_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return WKPreferencesGetLoadsImagesAutomatically(settings->preferences.get());
}

Eina_Bool ewk_settings_developer_extras_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetDeveloperExtrasEnabled(settings->preferences.get(), enable);

    return true;
}

Eina_Bool ewk_settings_developer_extras_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return WKPreferencesGetDeveloperExtrasEnabled(settings->preferences.get());
}

Eina_Bool ewk_settings_file_access_from_file_urls_allowed_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetFileAccessFromFileURLsAllowed(settings->preferences.get(), enable);

    return true;
}

Eina_Bool ewk_settings_file_access_from_file_urls_allowed_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return WKPreferencesGetFileAccessFromFileURLsAllowed(settings->preferences.get());
}

Eina_Bool ewk_settings_frame_flattening_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetFrameFlatteningEnabled(settings->preferences.get(), enable);

    return true;
}

Eina_Bool ewk_settings_frame_flattening_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return WKPreferencesGetFrameFlatteningEnabled(settings->preferences.get());
}

Eina_Bool ewk_settings_dns_prefetching_enabled_set(Ewk_Settings* settings, Eina_Bool enable)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    WKPreferencesSetDNSPrefetchingEnabled(settings->preferences.get(), enable);

    return true;
}

Eina_Bool ewk_settings_dns_prefetching_enabled_get(const Ewk_Settings* settings)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(settings, false);

    return WKPreferencesGetDNSPrefetchingEnabled(settings->preferences.get());
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
        if (enable && loadedSpellCheckingLanguages().isEmpty())
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
    Vector<String> languages = availableSpellCheckingLanguages();
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
    Vector<String> languages = loadedSpellCheckingLanguages();
    size_t numberOfLanuages = languages.size();

    for (size_t i = 0; i < numberOfLanuages; ++i)
        listOflanguages = eina_list_append(listOflanguages, eina_stringshare_add(languages[i].utf8().data()));

#endif
    return listOflanguages;
}
