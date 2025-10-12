/*
 * Copyright (C) 2008-2023 Apple Inc. All rights reserved.
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

#include "WebInspectorClient.h"

#include <CoreFoundation/CoreFoundation.h>
#include <WebCore/InspectorFrontendClientLocal.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/Page.h>
#include <wtf/RetainPtr.h>
#include <wtf/cf/TypeCastsCF.h>

static constexpr auto inspectorStartsAttachedSetting = "inspectorStartsAttached"_s;
static constexpr auto inspectorAttachDisabledSetting = "inspectorAttachDisabled"_s;

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
    return loadSetting(inspectorAttachDisabledSetting) == "true"_s;
}

void WebInspectorClient::setInspectorAttachDisabled(bool disabled)
{
    storeSetting(inspectorAttachDisabledSetting, disabled ? "true"_s : "false"_s);
}

void WebInspectorClient::deleteInspectorStartsAttached()
{
    deleteSetting(inspectorAttachDisabledSetting);
}

bool WebInspectorClient::inspectorStartsAttached()
{
    return loadSetting(inspectorStartsAttachedSetting) == "true"_s;
}

void WebInspectorClient::setInspectorStartsAttached(bool attached)
{
    storeSetting(inspectorStartsAttachedSetting, attached ? "true"_s : "false"_s);
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
