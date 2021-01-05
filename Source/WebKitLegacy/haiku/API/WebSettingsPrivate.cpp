/*
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>
#include "WebSettingsPrivate.h"

#include "Settings.h"
#include "WebSettings.h"

#include <Font.h>
#include <Path.h>
#include <PathFinder.h>

namespace BPrivate {

BList WebSettingsPrivate::sAllSettings(128);

WebSettingsPrivate::WebSettingsPrivate(WebCore::Settings* settings)
    : settings(settings)
    , localStoragePath()
    , offlineStorageDefaultQuota(5 * 1024 * 1024)
    , serifFontFamilySet(false)
    , sansSerifFontFamilySet(false)
    , fixedFontFamilySet(false)
    , standardFontFamilySet(false)
    , defaultFontSizeSet(false)
    , defaultFixedFontSizeSet(false)
    , javascriptEnabled(true)
    , localStorageEnabled(false)
    , databasesEnabled(false)
    , offlineWebApplicationCacheEnabled(false)
{
	apply();
	if (settings)
	    sAllSettings.AddItem(this);
	else {
		// Initialize some default settings
		// TODO: Get these from the system settings.
		font_family ff;
		font_style fs;

		be_plain_font->GetFamilyAndStyle(&ff, &fs);
		sansSerifFontFamily = ff;

		be_fixed_font->GetFamilyAndStyle(&ff, &fs);
		fixedFontFamily = ff;

		serifFontFamily = "Noto Serif Display";
		standardFontFamily = serifFontFamily;
		defaultFontSize = 14;
		defaultFixedFontSize = 14;
	}
}

WebSettingsPrivate::~WebSettingsPrivate()
{
	if (settings)
	    sAllSettings.RemoveItem(this);
}

void WebSettingsPrivate::apply()
{
	if (settings) {
		WebSettingsPrivate* global = BWebSettings::Default()->fData;
	    // Apply default values
	    settings->setLoadsImagesAutomatically(true);
	    settings->setMinimumFontSize(5);
	    settings->setMinimumLogicalFontSize(5);
	    settings->setShouldPrintBackgrounds(true);
	    settings->setScriptEnabled(javascriptEnabled);
//	    settings->setShowsURLsInToolTips(true);
	    settings->setEditingBehaviorType(WebCore::EditingBehaviorType::Mac);
	    settings->setLocalStorageEnabled(global->localStorageEnabled);
	    settings->setLocalStorageDatabasePath(global->localStoragePath);
	    settings->setDefaultTextEncodingName("UTF-8");
        settings->setNeedsSiteSpecificQuirks(true);

        char path[256];
        status_t result = find_path((void*)&WebSettingsPrivate::apply,
            B_FIND_PATH_DATA_DIRECTORY,
            "/WebKit/Directory Listing Template.html", path, 256);
        if (result != B_OK) {
            find_directory(B_SYSTEM_NONPACKAGED_DATA_DIRECTORY, 0, false, path,
				256);
            strcat(path, "/WebKit/Directory Listing Template.html");
        }
        settings->setFTPDirectoryTemplatePath(path);

//      settings->setShowDebugBorders(true);

	    // Apply local or global settings
		if (defaultFontSizeSet)
            settings->setDefaultFontSize(defaultFontSize);
		else
            settings->setDefaultFontSize(global->defaultFontSize);

		if (defaultFixedFontSizeSet)
            settings->setDefaultFixedFontSize(defaultFixedFontSize);
		else
            settings->setDefaultFixedFontSize(global->defaultFixedFontSize);

		if (serifFontFamilySet)
            settings->setSerifFontFamily(serifFontFamily.String());
		else
            settings->setSerifFontFamily(global->serifFontFamily.String());

		if (sansSerifFontFamilySet)
            settings->setSansSerifFontFamily(sansSerifFontFamily.String());
		else
            settings->setSansSerifFontFamily(global->sansSerifFontFamily.String());

		if (fixedFontFamilySet)
            settings->setFixedFontFamily(fixedFontFamily.String());
		else
            settings->setFixedFontFamily(global->fixedFontFamily.String());

		if (standardFontFamilySet)
            settings->setStandardFontFamily(standardFontFamily.String());
		else
            settings->setStandardFontFamily(global->standardFontFamily.String());
	} else {
	    int32 count = sAllSettings.CountItems();
	    for (int32 i = 0; i < count; i++) {
	        WebSettingsPrivate* webSettings = reinterpret_cast<WebSettingsPrivate*>(
	            sAllSettings.ItemAtFast(i));
	        if (webSettings != this)
	            webSettings->apply();
	    }
	}
}

} // namespace BPrivate
