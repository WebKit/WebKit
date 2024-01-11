/*
 * Copyright (C) 2003, 2006, 2010, 2013, 2017 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Forward.h>
#include <wtf/Vector.h>

#if PLATFORM(COCOA)
#import <CoreFoundation/CoreFoundation.h>
#import <wtf/RetainPtr.h>
#endif

namespace WTF {

enum class ShouldMinimizeLanguages : bool { No, Yes };

WTF_EXPORT_PRIVATE String defaultLanguage(ShouldMinimizeLanguages = ShouldMinimizeLanguages::Yes); // Thread-safe.
WTF_EXPORT_PRIVATE Vector<String> userPreferredLanguages(ShouldMinimizeLanguages = ShouldMinimizeLanguages::Yes); // Thread-safe, returns BCP 47 language tags.
WTF_EXPORT_PRIVATE void overrideUserPreferredLanguages(const Vector<String>&);
WTF_EXPORT_PRIVATE size_t indexOfBestMatchingLanguageInList(const String& language, const Vector<String>& languageList, bool& exactMatch);
WTF_EXPORT_PRIVATE bool userPrefersSimplifiedChinese();

// Called from platform specific code when the user's preferred language(s) change.
WTF_EXPORT_PRIVATE void languageDidChange();

// The observer function will be called when system language changes.
typedef void (*LanguageChangeObserverFunction)(void* context);
WTF_EXPORT_PRIVATE void addLanguageChangeObserver(void* context, LanguageChangeObserverFunction);
WTF_EXPORT_PRIVATE void removeLanguageChangeObserver(void* context);
WTF_EXPORT_PRIVATE String displayNameForLanguageLocale(const String&);

Vector<String> platformUserPreferredLanguages(ShouldMinimizeLanguages = ShouldMinimizeLanguages::Yes);

#if PLATFORM(COCOA)
bool canMinimizeLanguages();
WTF_EXPORT_PRIVATE void listenForLanguageChangeNotifications();
RetainPtr<CFArrayRef> minimizedLanguagesFromLanguages(CFArrayRef);
#endif

} // namespace WTF

using WTF::ShouldMinimizeLanguages;
using WTF::defaultLanguage;
using WTF::userPreferredLanguages;
using WTF::overrideUserPreferredLanguages;
using WTF::indexOfBestMatchingLanguageInList;
using WTF::userPrefersSimplifiedChinese;
using WTF::addLanguageChangeObserver;
using WTF::removeLanguageChangeObserver;
using WTF::displayNameForLanguageLocale;

