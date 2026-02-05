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
#include "Language.h"

#include <Application.h>
#include <LocaleRoster.h>
#include <Message.h>
#include <MessageFilter.h>
#include <wtf/text/WTFString.h>


namespace WTF {

static filter_result languagePreferencesDidChange(BMessage*, BHandler**, BMessageFilter*)
{
    languageDidChange();
    return B_DISPATCH_MESSAGE;
}

void listenForLanguageChangeNotifications()
{
    static std::once_flag addedListener;
    std::call_once(addedListener, [&] {
        if (be_app->Lock()) {
            BMessageFilter* localeListener = new BMessageFilter(B_LOCALE_CHANGED, languagePreferencesDidChange);
            be_app->AddCommonFilter(localeListener);
            be_app->Unlock();
        }
    });
}

Vector<String> platformUserPreferredLanguages(WTF::ShouldMinimizeLanguages)
{
    BMessage languages;
    int32 count = 0;

    BLocaleRoster::Default()->Refresh();
    if (BLocaleRoster::Default()->GetPreferredLanguages(&languages) != B_OK
        || languages.GetInfo("language", nullptr, &count) != B_OK
        || !count)
        return { "en"_s };

    Vector<String> preferredLanguages(count, [&](size_t i) {
        BString language;
        languages.FindString("language", i, &language);
        language.ReplaceAll('_', '-');
        return String::fromUTF8(language.String());
    });

    return preferredLanguages;
}

}
