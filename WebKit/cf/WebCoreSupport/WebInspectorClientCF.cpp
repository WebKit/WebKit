/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// FIXME: On Windows, we require all WebKit source files to include config.h
// before including any other files. Failing to include config.h will leave
// WTF_PLATFORM_CF and WTF_USE_JSC undefined, causing build failures in this 
// file. But Mac doesn't have a config.h for WebKit, so we can't include the 
// Windows one here. For now we can just define WTF_PLATFORM_CF and WTF_USE_JSC
// manually, but we need a better long-term solution.
#ifndef WTF_PLATFORM_CF
#define WTF_PLATFORM_CF 1
#endif

#ifndef WTF_USE_JSC
#define WTF_USE_JSC 1
#endif

#if defined(WIN32) || defined(_WIN32)
#if defined(BUILDING_JavaScriptCore) || defined(BUILDING_WTF)
#define JS_EXPORTDATA __declspec(dllexport)
#else
#define JS_EXPORTDATA __declspec(dllimport)
#endif
#define JS_EXPORTCLASS JS_EXPORTDATA
#else
#define JS_EXPORTDATA
#define JS_EXPORTCLASS
#endif

#include "WebInspectorClient.h"

#include <CoreFoundation/CoreFoundation.h>

#include <WebCore/PlatformString.h>

#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

using namespace WebCore;

static inline CFStringRef createKeyForPreferences(const String& key)
{
    RetainPtr<CFStringRef> keyCFString(AdoptCF, key.createCFString());
    return CFStringCreateWithFormat(0, 0, CFSTR("WebKit Web Inspector Setting - %@"), keyCFString.get());
}

void WebInspectorClient::populateSetting(const String& key, String* setting)
{
    RetainPtr<CFStringRef> preferencesKey(AdoptCF, createKeyForPreferences(key));
    RetainPtr<CFPropertyListRef> value(AdoptCF, CFPreferencesCopyAppValue(preferencesKey.get(), kCFPreferencesCurrentApplication));

    if (!value)
        return;

    CFTypeID type = CFGetTypeID(value.get());
    if (type == CFStringGetTypeID())
        *setting = static_cast<String>(static_cast<CFStringRef>(value.get()));
    else if (type == CFBooleanGetTypeID())
        *setting = static_cast<bool>(CFBooleanGetValue(static_cast<CFBooleanRef>(value.get()))) ? "true" : "false";
    else
        *setting = "";
}

void WebInspectorClient::storeSetting(const String& key, const String& setting)
{
    RetainPtr<CFPropertyListRef> objectToStore;
    objectToStore.adoptCF(setting.createCFString());
    ASSERT(objectToStore);

    RetainPtr<CFStringRef> preferencesKey(AdoptCF, createKeyForPreferences(key));
    CFPreferencesSetAppValue(preferencesKey.get(), objectToStore.get(), kCFPreferencesCurrentApplication);
}
