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

/** Creates a type name for Ewk_Settings */
typedef struct Ewk_Settings Ewk_Settings;

/**
 * Creates a type name for the callback function used to notify the client when
 * the continuous spell checking setting was changed by WebKit.
 *
 * @param enable @c EINA_TRUE if continuous spell checking is enabled or @c EINA_FALSE if it's disabled
 */
typedef void (*Ewk_Settings_Continuous_Spell_Checking_Change_Cb)(Eina_Bool enable);


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
 * By default, JavaScript execution is enabled.
 *
 * @param settings settings object to set javascript executing
 * @param enable @c EINA_TRUE to enable javascript executing
 *               @c EINA_FALSE to disable
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE on failure
 */
EAPI Eina_Bool ewk_settings_javascript_enabled_set(Ewk_Settings *settings, Eina_Bool enable);

/**
 * Returns whether JavaScript execution is enabled.
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
 * By default, auto loading of the images is enabled.
 *
 * @param settings settings object to set auto loading of the images
 * @param automatic @c EINA_TRUE to enable auto loading of the images
 *                  @c EINA_FALSE to disable
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE on failure
 */
EAPI Eina_Bool ewk_settings_loads_images_automatically_set(Ewk_Settings *settings, Eina_Bool automatic);

/**
 * Returns whether the images can be loaded automatically or not.
 *
 * @param settings settings object to get auto loading of the images
 *
 * @return @c EINA_TRUE if the images are loaded automatically
 *         @c EINA_FALSE if not or on failure
 */
EAPI Eina_Bool ewk_settings_loads_images_automatically_get(const Ewk_Settings *settings);

/**
 * Enables/disables developer extensions.
 *
 * By default, the developer extensions are disabled.
 *
 * @param settings settings object to set developer extensions
 * @param enable @c EINA_TRUE to enable developer extensions
 *               @c EINA_FALSE to disable
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
 * @param enable @c EINA_TRUE to enable file access permission
 *               @c EINA_FALSE to disable
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
 *         @c EINA_FALSE if not or on failure
 */
EAPI Eina_Bool ewk_settings_file_access_from_file_urls_allowed_get(const Ewk_Settings *settings);

/**
 * Enables/disables frame flattening.
 *
 * By default, the frame flattening is disabled.
 *
 * @param settings settings object to set the frame flattening
 * @param enable @c EINA_TRUE to enable the frame flattening
 *               @c EINA_FALSE to disable
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE on failure
 *
 * @see ewk_settings_enable_frame_flattening_get()
 */
EAPI Eina_Bool ewk_settings_frame_flattening_enabled_set(Ewk_Settings *settings, Eina_Bool enable);

/**
 * Returns whether the frame flattening is enabled.
 *
 * The frame flattening is a feature which expands sub frames until all the frames become
 * one scrollable page.
 *
 * @param settings settings object to get the frame flattening.
 *
 * @return @c EINA_TRUE if the frame flattening is enabled
 *         @c EINA_FALSE if not or on failure
 */
EAPI Eina_Bool ewk_settings_frame_flattening_enabled_get(const Ewk_Settings *settings);

/**
 * Enables/disables DNS prefetching.
 *
 * By default, DNS prefetching is disabled.
 *
 * @param settings settings object to set DNS prefetching
 * @param enable @c EINA_TRUE to enable DNS prefetching or
 *               @c EINA_FALSE to disable
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE on failure
 *
 * @see ewk_settings_DNS_prefetching_enabled_get()
 */
EAPI Eina_Bool ewk_settings_dns_prefetching_enabled_set(Ewk_Settings *settings, Eina_Bool enable);

/**
 * Returns whether DNS prefetching is enabled or not.
 *
 * DNS prefetching is an attempt to resolve domain names before a user tries to follow a link.
 *
 * @param settings settings object to get DNS prefetching
 *
 * @return @c EINA_TRUE if DNS prefetching is enabled
 *         @c EINA_FALSE if not or on failure
 */
EAPI Eina_Bool ewk_settings_dns_prefetching_enabled_get(const Ewk_Settings *settings);

/**
 * Sets a callback function used to notify the client when
 * the continuous spell checking setting was changed by WebKit.
 *
 * Specifying of this callback is needed if the application wants to receive notifications
 * once WebKit changes this setting.
 * If the application is not interested, this callback is not set.
 * Changing of this setting at the WebKit level can be made as a result of modifying
 * options in a Context Menu by a user.
 *
 * @param cb a new callback function to set or @c NULL to invalidate the previous one
 */
EAPI void ewk_settings_continuous_spell_checking_change_cb_set(Ewk_Settings_Continuous_Spell_Checking_Change_Cb cb);

/**
 * Queries if continuous spell checking is enabled.
 *
 * @return @c EINA_TRUE if continuous spell checking is enabled or @c EINA_FALSE if it's disabled
 */
EAPI Eina_Bool ewk_settings_continuous_spell_checking_enabled_get(void);

/**
 * Enables/disables continuous spell checking.
 *
 * Additionally, this function calls a callback function (if defined) to notify
 * the client about the change of the setting.
 * This feature is disabled by default.
 *
 * @see ewk_settings_continuous_spell_checking_change_cb_set
 *
 * @param enable @c EINA_TRUE to enable continuous spell checking or @c EINA_FALSE to disable
 */
EAPI void ewk_settings_continuous_spell_checking_enabled_set(Eina_Bool enable);

/**
 * Gets the the list of all available the spell checking languages to use.
 *
 * @see ewk_settings_spell_checking_languages_set
 *
 * @return the list with available spell checking languages, or @c NULL on failure
 *         the Eina_List and its items should be freed after, use eina_stringshare_del()
 */
EAPI Eina_List *ewk_settings_spell_checking_available_languages_get(void);

/**
 * Sets @a languages as the list of languages to use by default WebKit
 * implementation of spellchecker feature with Enchant library support.
 *
 * If @languages is @c NULL, the default language is used.
 * If the default language can not be determined then any available dictionary will be used.
 *
 * @note This function invalidates the previously set languages.
 *       The dictionaries are requested asynchronously.
 *
 * @param languages a list of comma (',') separated language codes
 *        of the form 'en_US', ie, language_VARIANT, may be @c NULL.
 */
EAPI void ewk_settings_spell_checking_languages_set(const char *languages);

/**
 * Gets the the list of the spell checking languages in use.
 *
 * @see ewk_settings_spell_checking_available_languages_get
 * @see ewk_settings_spell_checking_languages_set
 *
 * @return the list with the spell checking languages in use,
 *         the Eina_List and its items should be freed after, use eina_stringshare_del()
 */
EAPI Eina_List *ewk_settings_spell_checking_languages_get(void);

/**
 * Enables/disables the encoding detector.
 *
 * By default, the encoding detector is disabled.
 *
 * @param settings settings object to set the encoding detector
 * @param enable @c EINA_TRUE to enable the encoding detector,
 *        @c EINA_FALSE to disable
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE on failure
 */
EAPI Eina_Bool ewk_settings_encoding_detector_enabled_set(Ewk_Settings *settings, Eina_Bool enable);

/**
* Returns whether the encoding detector is enabled or not.
 *
 * @param settings settings object to query whether encoding detector is enabled
 *
 * @return @c EINA_TRUE if the encoding detector is enabled
 *         @c EINA_FALSE if not or on failure
 */
EAPI Eina_Bool ewk_settings_encoding_detector_enabled_get(const Ewk_Settings *settings);

/**
 * Sets preferred minimum contents width which is used as default minimum contents width
 * for non viewport meta element sites.
 *
 * By default, preferred minimum contents width is equal to @c 980.
 *
 * @param settings settings object to set the encoding detector
 * @param enable @c EINA_TRUE to enable the encoding detector,
 *        @c EINA_FALSE to disable
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE on failure
 */
EAPI Eina_Bool ewk_settings_preferred_minimum_contents_width_set(Ewk_Settings *settings, unsigned width);

/**
 * Returns preferred minimum contents width or @c 0 on failure.
 *
 * @param settings settings object to query preferred minimum contents width
 *
 * @return preferred minimum contents width
 *         @c 0 on failure
 */
EAPI unsigned ewk_settings_preferred_minimum_contents_width_get(const Ewk_Settings *settings);

/**
 * Enables/disables the offline application cache.
 *
 * By default, the offline application cache is enabled.
 *
 * @param settings settings object to set the offline application cache state
 * @param enable @c EINA_TRUE to enable the offline application cache,
 *        @c EINA_FALSE to disable
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE on failure
 */
EAPI Eina_Bool ewk_settings_offline_web_application_cache_enabled_set(Ewk_Settings *settings, Eina_Bool enable);

/**
 * Returns whether the offline application cache is enabled or not.
 *
 * @param settings settings object to query whether offline application cache is enabled
 *
 * @return @c EINA_TRUE if the offline application cache is enabled
 *         @c EINA_FALSE if disabled or on failure
 */
EAPI Eina_Bool ewk_settings_offline_web_application_cache_enabled_get(const Ewk_Settings *settings);

#ifdef __cplusplus
}
#endif
#endif // ewk_settings_h
