/*
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
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


#include "config.h"
#include <wtf/text/WTFString.h>
#include "Language.h"

/* Can't include these the normal way, because WTF header names collide. */
#include <locale/Collator.h>
#include <support/Locker.h>

#include <Locale.h>
#include <LocaleRoster.h>
#include <stdio.h>


namespace WTF {

void setPlatformUserPreferredLanguagesChangedCallback(void (*)()) { }

static String platformLanguage()
{
	static BString locale;
	static bool initialized = false;
	if (!initialized) {
		initialized = true;
		BLanguage language;
		if (BLocale::Default()->GetLanguage(&language) == B_OK)
			locale = language.ID();
		else
			locale = "c";
		locale.ReplaceAll('_', '-');
	}
	return locale.String();
}

Vector<String> platformUserPreferredLanguages(WTF::ShouldMinimizeLanguages)
{
    Vector<String> userPreferredLanguages;
    userPreferredLanguages.append(platformLanguage());
    return userPreferredLanguages;
}

}
