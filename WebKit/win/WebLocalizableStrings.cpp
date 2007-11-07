/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
#include "WebKitDLL.h"

#include "WebLocalizableStrings.h"

#pragma warning(push, 0)
#include <WebCore/CString.h>
#include <WebCore/PlatformString.h>
#include <WebCore/StringHash.h>
#pragma warning(pop)

#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <CoreFoundation/CoreFoundation.h>

using namespace WebCore;

WebLocalizableStringsBundle WebKitLocalizableStringsBundle = { "com.apple.WebKit", 0 };

static CFBundleRef localizedStringsMainBundle;
static HashMap<String, String> mainBundleLocStrings;
static HashMap<String, String> frameworkLocStrings;

static CFBundleRef createWebKitBundle()
{
    WCHAR pathStr[MAX_PATH];
    DWORD length = ::GetModuleFileNameW(gInstance, pathStr, MAX_PATH);
    if (!length || (length == MAX_PATH && GetLastError() == ERROR_INSUFFICIENT_BUFFER))
        return 0;

    bool found = false;
    for (int i = length - 1; i >= 0; i--) {
        // warning C6385: Invalid data: accessing 'pathStr', the readable size is '520' bytes, but '2000' bytes might be read
        #pragma warning(suppress: 6385)
        if (pathStr[i] == L'\\') {
            // warning C6386: Buffer overrun: accessing 'pathStr', the writable size is '520' bytes, but '1996' bytes might be written
            #pragma warning(suppress: 6386)
            pathStr[i] = 0;
            found = true;
            break;
        }
    }
    if (!found)
        return 0;

    if (wcscat_s(pathStr, MAX_PATH, L"\\WebKit.resources"))
        return 0;

    String bundlePathString(pathStr);
    CFStringRef bundlePathCFString = bundlePathString.createCFString();
    if (!bundlePathCFString)
        return 0;

    CFURLRef bundleURLRef = CFURLCreateWithFileSystemPath(0, bundlePathCFString, kCFURLWindowsPathStyle, true);
    CFRelease(bundlePathCFString);
    if (!bundleURLRef)
        return 0;

    CFBundleRef bundle = CFBundleCreate(0, bundleURLRef);
    CFRelease(bundleURLRef);
    return bundle;
}

void SetWebLocalizedStringMainBundle(CFBundleRef bundle)
{
    if (bundle)
        CFRetain(bundle);
    if (localizedStringsMainBundle)
        CFRelease(localizedStringsMainBundle);
    localizedStringsMainBundle = bundle;
}

static CFStringRef copyLocalizedStringFromBundle(WebLocalizableStringsBundle* stringsBundle, const String& key)
{
    static CFStringRef notFound = CFSTR("localized string not found");

    CFBundleRef bundle;
    if (!stringsBundle) {
        static CFBundleRef mainBundle;
        if (!mainBundle) {
            mainBundle = localizedStringsMainBundle;
            if (!mainBundle)
                return notFound;
        }
        bundle = mainBundle;
    } else {
        bundle = stringsBundle->bundle;
        if (!bundle) {
            bundle = createWebKitBundle();
            if (!bundle)
                return notFound;
            stringsBundle->bundle = bundle;
        }
    }

    RetainPtr<CFStringRef> keyString(AdoptCF, key.createCFString());
    CFStringRef result = CFCopyLocalizedStringWithDefaultValue(keyString.get(), 0, bundle, notFound, 0);

    ASSERT_WITH_MESSAGE(result != notFound, "could not find localizable string %s in bundle", key);
    return result;
}

static bool findCachedStringInMap(const HashMap<String, String>& map, const String& key, String& outString)
{
    HashMap<String, String>::const_iterator it = map.find(key);
    if (it == map.end())
        return false;

    outString = it->second;
    return true;
}

static bool findCachedString(WebLocalizableStringsBundle* stringsBundle, const String& key, String& outString)
{
    if (!stringsBundle && findCachedStringInMap(mainBundleLocStrings, key, outString))
        return true;

    return stringsBundle && stringsBundle->bundle == WebKitLocalizableStringsBundle.bundle && findCachedStringInMap(frameworkLocStrings, key, outString);
}

static void cacheString(WebLocalizableStringsBundle* stringsBundle, const String& key, const String& value)
{
    if (!stringsBundle)
        mainBundleLocStrings.set(key, value);
    else if (stringsBundle)
        frameworkLocStrings.set(key, value);
}

static CFStringRef localizedString(WebLocalizableStringsBundle* stringsBundle, const String& key)
{
    String found;
    if (findCachedString(stringsBundle, key, found))
        return found.createCFString();

    RetainPtr<CFStringRef> cfStr(AdoptCF, copyLocalizedStringFromBundle(stringsBundle, key));

    cacheString(stringsBundle, key, cfStr.get());

    return cfStr.releaseRef();
}

static LPCTSTR localizedLPCTSTR(WebLocalizableStringsBundle* stringsBundle, const String& key)
{
    String found;
    if (findCachedString(stringsBundle, key, found))
        return found.charactersWithNullTermination();

    RetainPtr<CFStringRef> cfStr(AdoptCF, copyLocalizedStringFromBundle(stringsBundle, key));

    String str(cfStr.get());
    for (unsigned int i=1; i<str.length(); i++)
        if (str[i] == '@' && (str[i - 1] == '%' || (i > 2 && str[i - 1] == '$' && str[i - 2] >= '1' && str[i - 2] <= '9' && str[i - 3] == '%')))
            str.replace(i, 1, "s");
    LPCTSTR lpszStr = str.charactersWithNullTermination();

    cacheString(stringsBundle, key, str);

    return lpszStr;
}

CFStringRef WebLocalizedStringUTF8(WebLocalizableStringsBundle* stringsBundle, LPCSTR key)
{
    if (!key)
        return 0;

    return localizedString(stringsBundle, String::fromUTF8(key));
}

LPCTSTR WebLocalizedLPCTSTRUTF8(WebLocalizableStringsBundle* stringsBundle, LPCSTR key)
{
    if (!key)
        return 0;

    return localizedLPCTSTR(stringsBundle, String::fromUTF8(key));
}

// These functions are deprecated.

CFStringRef WebLocalizedString(WebLocalizableStringsBundle* stringsBundle, LPCTSTR key)
{
    if (!key)
        return 0;

    return localizedString(stringsBundle, String(key));
}

LPCTSTR WebLocalizedLPCTSTR(WebLocalizableStringsBundle* stringsBundle, LPCTSTR key)
{
    if (!key)
        return 0;

    return localizedLPCTSTR(stringsBundle, String(key));
}
