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

#include "WebKitDLL.h"

#include "WebLocalizableStrings.h"

#if USE(CF)

#include <CoreFoundation/CoreFoundation.h>

#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/RetainPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ThreadingPrimitives.h>

class LocalizedString;

WebLocalizableStringsBundle WebKitLocalizableStringsBundle = { "com.apple.WebKit", 0 };

typedef HashMap<String, LocalizedString*> LocalizedStringMap;

static Lock mainBundleLocStringsLock;

static LocalizedStringMap& mainBundleLocStrings()
{
    static NeverDestroyed<LocalizedStringMap> map;
    return map;
}

static Lock frameworkLocStringsLock;

static LocalizedStringMap frameworkLocStrings()
{
    static NeverDestroyed<LocalizedStringMap> map;
    return map;
}

class LocalizedString {
    WTF_MAKE_NONCOPYABLE(LocalizedString);
public:
    LocalizedString(RetainPtr<CFStringRef>&& string)
        : m_cfString(WTFMove(string))
    {
        ASSERT(m_cfString);
    }

    operator LPCTSTR() const;
    operator CFStringRef() const { return m_cfString.get(); }

private:
    RetainPtr<CFStringRef> m_cfString;
    mutable String m_string;
};

LocalizedString::operator LPCTSTR() const
{
    if (!m_string.isEmpty())
        return m_string.wideCharacters().data();

    m_string = m_cfString.get();

    for (unsigned int i = 1; i < m_string.length(); i++)
        if (m_string[i] == '@' && (m_string[i - 1] == '%' || (i > 2 && m_string[i - 1] == '$' && m_string[i - 2] >= '1' && m_string[i - 2] <= '9' && m_string[i - 3] == '%')))
            m_string = makeStringByReplacing(m_string, i, 1, "s"_s);

    return m_string.wideCharacters().data();
}

static void createWebKitBundle()
{
    static NeverDestroyed<RetainPtr<CFBundleRef>> bundle;
    static bool initialized;

    if (initialized)
        return;
    initialized = true;

    WCHAR pathStr[MAX_PATH];
    DWORD length = ::GetModuleFileNameW(gInstance, pathStr, MAX_PATH);
    if (!length || (length == MAX_PATH && GetLastError() == ERROR_INSUFFICIENT_BUFFER))
        return;

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
        return;

    if (wcscat_s(pathStr, MAX_PATH, L"\\WebKit.resources"))
        return;

    String bundlePathString(pathStr);
    if (!bundlePathString)
        return;

    auto bundleURLRef = adoptCF(CFURLCreateWithFileSystemPath(0, bundlePathString.createCFString().get(), kCFURLWindowsPathStyle, true));
    if (!bundleURLRef)
        return;

    bundle.get() = adoptCF(CFBundleCreate(0, bundleURLRef.get()));
}

static CFBundleRef cfBundleForStringsBundle(WebLocalizableStringsBundle* stringsBundle)
{
    if (!stringsBundle) {
        static CFBundleRef mainBundle = CFBundleGetMainBundle();
        return mainBundle;
    }

    createWebKitBundle();

    if (!stringsBundle->bundle)
        stringsBundle->bundle = CFBundleGetBundleWithIdentifier(adoptCF(CFStringCreateWithCString(0, stringsBundle->identifier, kCFStringEncodingASCII)).get());
    return stringsBundle->bundle;
}

static RetainPtr<CFStringRef> copyLocalizedStringFromBundle(WebLocalizableStringsBundle* stringsBundle, const String& key)
{
    static CFStringRef notFound = CFSTR("localized string not found");

    CFBundleRef bundle = cfBundleForStringsBundle(stringsBundle);
    if (!bundle)
        return notFound;

    auto result = adoptCF(CFCopyLocalizedStringWithDefaultValue(key.createCFString().get(), 0, bundle, notFound, 0));

    ASSERT_WITH_MESSAGE(result != notFound, "could not find localizable string %s in bundle", key.utf8().data());
    return result;
}

static LocalizedString* findCachedString(WebLocalizableStringsBundle* stringsBundle, const String& key)
{
    if (!stringsBundle) {
        Locker locker { mainBundleLocStringsLock };
        return mainBundleLocStrings().get(key);
    }

    if (stringsBundle->bundle == WebKitLocalizableStringsBundle.bundle) {
        Locker locker { frameworkLocStringsLock };
        return frameworkLocStrings().get(key);
    }

    return 0;
}

static void cacheString(WebLocalizableStringsBundle* stringsBundle, const String& key, LocalizedString* value)
{
    if (!stringsBundle) {
        Locker locker { mainBundleLocStringsLock };
        mainBundleLocStrings().set(key, value);
        return;
    }

    Locker locker { frameworkLocStringsLock };
    frameworkLocStrings().set(key, value);
}

static const LocalizedString& localizedString(WebLocalizableStringsBundle* stringsBundle, const String& key)
{
    LocalizedString* string = findCachedString(stringsBundle, key);
    if (string)
        return *string;

    string = new LocalizedString(copyLocalizedStringFromBundle(stringsBundle, key));
    cacheString(stringsBundle, key, string);

    return *string;
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

    return localizedString(stringsBundle, String::fromUTF8(key));
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

    return localizedString(stringsBundle, String(key));
}

void SetWebLocalizedStringMainBundle(CFBundleRef)
{
}
#endif
