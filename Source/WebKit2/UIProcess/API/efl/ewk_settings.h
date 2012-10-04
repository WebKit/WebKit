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

/**
 * @file    ewk_settings.h
 * @brief   Describes the settings API.
 *
 * @note The ewk_settings is for setting the preference of specific ewk_view.
 * We can get the ewk_settings from ewk_view using ewk_view_settings_get() API.
 */

#ifndef ewk_settings_h
#define ewk_settings_h

#include <Eina.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Creates a type name for _Ewk_Settings */
typedef struct _Ewk_Settings Ewk_Settings;

/**
 * Enables/disables the Javascript Fullscreen API. The Javascript API allows
 * to request full screen mode, for more information see:
 * http://dvcs.w3.org/hg/fullscreen/raw-file/tip/Overview.html
 *
 * Default value for Javascript Fullscreen API setting is @c EINA_TRUE .
 *
 * @param settings settings object to enable Javascript Fullscreen API
 * @param enable @c EINA_TRUE to enable Javascript Fullscreen API or
 *               @c EINA_FALSE to disable
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE on failure
 */
EAPI Eina_Bool ewk_settings_fullscreen_enabled_set(Ewk_Settings *settings, Eina_Bool enable);

/**
 * Returns whether the Javascript Fullscreen API is enabled or not.
 *
 * @param settings settings object to query whether Javascript Fullscreen API is enabled
 *
 * @return @c EINA_TRUE if the Javascript Fullscreen API is enabled
 *         @c EINA_FALSE if not or on failure
 */
EAPI Eina_Bool ewk_settings_fullscreen_enabled_get(const Ewk_Settings *settings);

/**
 * Enables/disables the javascript executing.
 *
 * @param settings settings object to set javascript executing
 * @param enable @c EINA_TRUE to enable javascript executing
 *        @c EINA_FALSE to disable
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE on failure
 */
EAPI Eina_Bool ewk_settings_javascript_enabled_set(Ewk_Settings *settings, Eina_Bool enable);

/**
 * Returns the javascript can be executable or not.
 *
 * @param settings settings object to query if the javascript can be executed
 *
 * @return @c EINA_TRUE if the javascript can be executed
 *         @c EINA_FALSE if not or on failure
 */
EAPI Eina_Bool ewk_settings_javascript_enabled_get(const Ewk_Settings *settings);

/**
 * Enables/disables auto loading of the images.
 *
 * @param settings settings object to set auto loading of the images
 * @param automatic @c EINA_TRUE to enable auto loading of the images,
 *        @c EINA_FALSE to disable
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE on failure
 */
EAPI Eina_Bool ewk_settings_loads_images_automatically_set(Ewk_Settings *settings, Eina_Bool automatic);

/**
 * Returns the images can be loaded automatically or not.
 *
 * @param settings settings object to get auto loading of the images
 *
 * @return @c EINA_TRUE if the images are loaded automatically,
 *         @c EINA_FALSE if not or on failure
 */
EAPI Eina_Bool ewk_settings_loads_images_automatically_get(const Ewk_Settings *settings);

/**
 * Enables/disables developer extensions.
 *
 * By default, the developer extensions are disabled.
 *
 * @param settings settings object to set developer extensions
 * @param enable @c EINA_TRUE to enable developer extensions, @c EINA_FALSE to disable
 *
 * @return @c EINA_TRUE on success or @EINA_FALSE on failure
 */
EAPI Eina_Bool ewk_settings_developer_extras_enabled_set(Ewk_Settings *settings, Eina_Bool enable);

/**
 * Queries if developer extensions are enabled.
 *
 * By default, the developer extensions are disabled.
 *
 * @param settings settings object to set developer extensions
 *
 * @return @c EINA_TRUE if developer extensions are enabled
           @c EINA_FALSE if not or on failure
 */
EAPI Eina_Bool ewk_settings_developer_extras_enabled_get(const Ewk_Settings *settings);

/**
 * Allow / Disallow file access from file:// URLs.
 *
 * By default, file access from file:// URLs is not allowed.
 *
 * @param settings settings object to set file access permission
 *
 * @return @c EINA_TRUE on success or @EINA_FALSE on failure
 */
EAPI Eina_Bool ewk_settings_file_access_from_file_urls_allowed_set(Ewk_Settings *settings, Eina_Bool enable);

/**
 * Queries if  file access from file:// URLs is allowed.
 *
 * By default, file access from file:// URLs is not allowed.
 *
 * @param settings settings object to query file access permission
 *
 * @return @c EINA_TRUE if file access from file:// URLs is allowed
           @c EINA_FALSE if not or on failure
 */
EAPI Eina_Bool ewk_settings_file_access_from_file_urls_allowed_get(const Ewk_Settings *settings);

#ifdef __cplusplus
}
#endif
#endif // ewk_settings_h
