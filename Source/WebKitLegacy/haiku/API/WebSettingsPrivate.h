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

#ifndef WebSettingsPrivate_h
#define WebSettingsPrivate_h

#include <List.h>
#include <String.h>

namespace WebCore {
class Settings;
}

namespace BPrivate {

class WebSettingsPrivate {
public:
    WebSettingsPrivate(WebCore::Settings* settings = 0);
    ~WebSettingsPrivate();

    void apply();

    WebCore::Settings* settings;

    BString localStoragePath;
    int64 offlineStorageDefaultQuota;

	BString serifFontFamily;
	BString sansSerifFontFamily;
	BString fixedFontFamily;
	BString standardFontFamily;
	float defaultFontSize;
	float defaultFixedFontSize;

	bool serifFontFamilySet : 1;
	bool sansSerifFontFamilySet : 1;
	bool fixedFontFamilySet : 1;
	bool standardFontFamilySet : 1;
	bool defaultFontSizeSet : 1;
	bool defaultFixedFontSizeSet : 1;
	bool javascriptEnabled : 1;
	bool localStorageEnabled : 1;
	bool databasesEnabled : 1;
	bool offlineWebApplicationCacheEnabled : 1;

private:
	static BList sAllSettings;
};

} // namespace BPrivate

#endif // WebSettingsPrivate_h
