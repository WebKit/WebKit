/*
 * Copyright (C) 2008-2021 Apple Inc. All Rights Reserved.
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
// USE_CF undefined, causing build failures in this file. But Mac doesn't have
// a config.h for WebKit, so we can't include the Windows one here. For now we
// define USE_CF manually here, but it would be good to find a better solution,
// likely by making "config.h" a "prefix file" in the Windows build configuration.
#ifndef USE_CF
#define USE_CF 1
#endif

#include <wtf/Platform.h>

#if PLATFORM(WIN) && !defined(USE_CG)
#define USE_CG 1
#endif

// NOTE: These need to appear up top, as they declare macros used in the JS and WTF headers.
#include <JavaScriptCore/JSExportMacros.h>
#include <wtf/ExportMacros.h>

#include "WebInspectorClient.h"

#include <CoreFoundation/CoreFoundation.h>
#include <WebCore/Frame.h>
#include <WebCore/InspectorFrontendClientLocal.h>
#include <WebCore/Page.h>
#include <wtf/RetainPtr.h>
#include <wtf/cf/TypeCastsCF.h>

static constexpr const char* inspectorStartsAttachedSetting = "inspectorStartsAttached";
static constexpr const char* inspectorAttachDisabledSetting = "inspectorAttachDisabled";

static RetainPtr<CFStringRef> createKeyForPreferences(const String& key)
{
    return adoptCF(CFStringCreateWithFormat(0, 0, CFSTR("WebKit Web Inspector Setting - %@"), key.createCFString().get()));
}

static String loadSetting(const String& key)
{
    auto value = adoptCF(CFPreferencesCopyAppValue(createKeyForPreferences(key).get(), kCFPreferencesCurrentApplication));
    if (auto string = dynamic_cf_cast<CFStringRef>(value.get()))
        return string;
    if (value == kCFBooleanTrue)
        return "true"_s;
    if (value == kCFBooleanFalse)
        return "false"_s;
    return { };
}

static void storeSetting(const String& key, const String& setting)
{
    CFPreferencesSetAppValue(createKeyForPreferences(key).get(), setting.createCFString().get(), kCFPreferencesCurrentApplication);
}

static void deleteSetting(const String& key)
{
    CFPreferencesSetAppValue(createKeyForPreferences(key).get(), nullptr, kCFPreferencesCurrentApplication);
}

void WebInspectorClient::sendMessageToFrontend(const String& message)
{
    m_frontendClient->frontendAPIDispatcher().dispatchMessageAsync(message);
}

bool WebInspectorClient::inspectorAttachDisabled()
{
    return loadSetting(inspectorAttachDisabledSetting) == "true";
}

void WebInspectorClient::setInspectorAttachDisabled(bool disabled)
{
    storeSetting(inspectorAttachDisabledSetting, disabled ? "true" : "false");
}

void WebInspectorClient::deleteInspectorStartsAttached()
{
    deleteSetting(inspectorAttachDisabledSetting);
}

bool WebInspectorClient::inspectorStartsAttached()
{
    return loadSetting(inspectorStartsAttachedSetting) == "true";
}

void WebInspectorClient::setInspectorStartsAttached(bool attached)
{
    storeSetting(inspectorStartsAttachedSetting, attached ? "true" : "false");
}

void WebInspectorClient::deleteInspectorAttachDisabled()
{
    deleteSetting(inspectorStartsAttachedSetting);
}

std::unique_ptr<WebCore::InspectorFrontendClientLocal::Settings> WebInspectorClient::createFrontendSettings()
{
    class InspectorFrontendSettingsCF : public WebCore::InspectorFrontendClientLocal::Settings {
    private:
        String getProperty(const String& name) final { return loadSetting(name); }
        void setProperty(const String& name, const String& value) final { storeSetting(name, value); }
        void deleteProperty(const String& name) final { deleteSetting(name); }
    };
    return makeUnique<InspectorFrontendSettingsCF>();
}
