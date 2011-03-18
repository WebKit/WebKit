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
// WTF_USE_CF and WTF_USE_JSC undefined, causing build failures in this 
// file. But Mac doesn't have a config.h for WebKit, so we can't include the 
// Windows one here. For now we can just define WTF_USE_CF,  WTF_USE_JSC, and
// WTF_USE_CFNETWORK manually, but we need a better long-term solution.
#ifndef WTF_USE_CF
#define WTF_USE_CF 1
#endif

#ifndef WTF_USE_JSC
#define WTF_USE_JSC 1
#endif

// Leave these set to nothing until we switch Mac and Win ports over to 
// using the export macros.
#define JS_EXPORT_PRIVATE
#define WTF_EXPORT_PRIVATE

#if defined(WIN32) || defined(_WIN32)
#ifndef WTF_USE_CFNETWORK
#define WTF_USE_CFNETWORK 1
#endif
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

#include <WebCore/Frame.h>
#include <WebCore/InspectorFrontendClientLocal.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformString.h>

#include <wtf/PassOwnPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

using namespace WebCore;

static const char* inspectorStartsAttachedSetting = "inspectorStartsAttached";

static inline CFStringRef createKeyForPreferences(const String& key)
{
    RetainPtr<CFStringRef> keyCFString(AdoptCF, key.createCFString());
    return CFStringCreateWithFormat(0, 0, CFSTR("WebKit Web Inspector Setting - %@"), keyCFString.get());
}

static void populateSetting(const String& key, String* setting)
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

static void storeSetting(const String& key, const String& setting)
{
    RetainPtr<CFPropertyListRef> objectToStore;
    objectToStore.adoptCF(setting.createCFString());
    ASSERT(objectToStore);

    RetainPtr<CFStringRef> preferencesKey(AdoptCF, createKeyForPreferences(key));
    CFPreferencesSetAppValue(preferencesKey.get(), objectToStore.get(), kCFPreferencesCurrentApplication);
}

bool WebInspectorClient::sendMessageToFrontend(const String& message)
{
    return doDispatchMessageOnFrontendPage(m_frontendPage, message);
}

bool WebInspectorClient::inspectorStartsAttached()
{
    String value;
    populateSetting(inspectorStartsAttachedSetting, &value);
    if (value.isEmpty())
        return true;
    return value == "true";
}

void WebInspectorClient::setInspectorStartsAttached(bool attached)
{
    storeSetting(inspectorStartsAttachedSetting, attached ? "true" : "false");
}

void WebInspectorClient::releaseFrontendPage()
{
    m_frontendPage = 0;
}

void WebInspectorClient::saveSessionSetting(const String& key, const String& value)
{
    if (!key.isEmpty())
        m_sessionSettings.set(key, value);
}

void WebInspectorClient::loadSessionSetting(const String& key, String* value)
{
    if (!key.isEmpty())
        *value = m_sessionSettings.get(key);
}

WTF::PassOwnPtr<WebCore::InspectorFrontendClientLocal::Settings> WebInspectorClient::createFrontendSettings()
{
    class InspectorFrontendSettingsCF : public WebCore::InspectorFrontendClientLocal::Settings {
    public:
        virtual ~InspectorFrontendSettingsCF() { }
        virtual String getProperty(const String& name)
        {
            String value;
            populateSetting(name, &value);
            return value;
        }

        virtual void setProperty(const String& name, const String& value)
        {
            storeSetting(name, value);
        }
    };
    return adoptPtr<WebCore::InspectorFrontendClientLocal::Settings>(new InspectorFrontendSettingsCF());
}
