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
#else
#define JS_EXPORTDATA
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

void WebInspectorClient::populateSetting(const String& key, InspectorController::Setting& setting)
{
    RetainPtr<CFStringRef> preferencesKey(AdoptCF, createKeyForPreferences(key));
    RetainPtr<CFPropertyListRef> value(AdoptCF, CFPreferencesCopyAppValue(preferencesKey.get(), kCFPreferencesCurrentApplication));

    if (!value)
        return;

    CFTypeID type = CFGetTypeID(value.get());
    if (type == CFStringGetTypeID())
        setting.set(static_cast<String>(static_cast<CFStringRef>(value.get())));
    else if (type == CFBooleanGetTypeID())
        setting.set(static_cast<bool>(CFBooleanGetValue(static_cast<CFBooleanRef>(value.get()))));
    else if (type == CFNumberGetTypeID()) {
        CFNumberRef number = static_cast<CFNumberRef>(value.get());
        if (CFNumberIsFloatType(number)) {
            double doubleNumber = 0.0;
            CFNumberGetValue(static_cast<CFNumberRef>(value.get()), kCFNumberDoubleType, &doubleNumber);
            setting.set(doubleNumber);
        } else {
            long longNumber = 0;
            CFNumberGetValue(static_cast<CFNumberRef>(value.get()), kCFNumberLongType, &longNumber);
            setting.set(longNumber);
        }
    } else if (type == CFArrayGetTypeID()) {
        Vector<String> strings;

        CFArrayRef array = static_cast<CFArrayRef>(value.get());
        unsigned length = CFArrayGetCount(array);
        for (unsigned i = 0; i < length; ++i) {
            CFStringRef string = static_cast<CFStringRef>(CFArrayGetValueAtIndex(array, i));
            if (CFGetTypeID(string) == CFStringGetTypeID())
                strings.append(static_cast<String>(static_cast<CFStringRef>(string)));
        }

        setting.set(strings);
    } else
        ASSERT_NOT_REACHED();
}

void WebInspectorClient::storeSetting(const String& key, const InspectorController::Setting& setting)
{
    RetainPtr<CFPropertyListRef> objectToStore;

    switch (setting.type()) {
        default:
        case InspectorController::Setting::NoType:
            ASSERT_NOT_REACHED();
            break;
        case InspectorController::Setting::StringType:
            objectToStore.adoptCF(setting.string().createCFString());
            break;
        case InspectorController::Setting::BooleanType:
            objectToStore = (setting.booleanValue() ? kCFBooleanTrue : kCFBooleanFalse);
            break;

        case InspectorController::Setting::DoubleType: {
            double value = setting.doubleValue();
            objectToStore.adoptCF(CFNumberCreate(0, kCFNumberDoubleType, &value));
            break;
        }

        case InspectorController::Setting::IntegerType: {
            long value = setting.integerValue();
            objectToStore.adoptCF(CFNumberCreate(0, kCFNumberLongType, &value));
            break;
        }

        case InspectorController::Setting::StringVectorType: {
            const Vector<String>& strings = setting.stringVector();
            const unsigned length = strings.size();

            RetainPtr<CFMutableArrayRef> array(AdoptCF, CFArrayCreateMutable(0, length, &kCFTypeArrayCallBacks));

            for (unsigned i = 0; i < length; ++i) {
                RetainPtr<CFStringRef> string(AdoptCF, strings[i].createCFString());
                CFArraySetValueAtIndex(array.get(), i, string.get());
            }

            objectToStore = array;
            break;
        }
    }

    ASSERT(objectToStore);

    RetainPtr<CFStringRef> preferencesKey(AdoptCF, createKeyForPreferences(key));
    CFPreferencesSetAppValue(preferencesKey.get(), objectToStore.get(), kCFPreferencesCurrentApplication);
}

void WebInspectorClient::removeSetting(const String& key)
{
    RetainPtr<CFStringRef> preferencesKey(AdoptCF, createKeyForPreferences(key));
    CFPreferencesSetAppValue(preferencesKey.get(), 0, kCFPreferencesCurrentApplication);
}
