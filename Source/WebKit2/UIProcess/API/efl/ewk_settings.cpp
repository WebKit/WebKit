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

using namespace WebKit;

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
