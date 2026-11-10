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
#include <wtf/Assertions.h>
#include <wtf/RetainPtr.h>
#include <wtf/cf/TypeCastsCF.h>

static constexpr auto inspectorStartsAttachedSetting = "inspectorStartsAttached"_s;
static constexpr auto inspectorAttachDisabledSetting = "inspectorAttachDisabled"_s;

static RetainPtr<CFStringRef> createKeyForPreferences(StringImpl* key)
{
    return adoptCF(CFStringCreateWithFormat(0, 0, CFSTR("WebKit Web Inspector Setting - %@"), createCFString(key).get()));
}

static RefPtr<StringImpl> loadSetting(StringImpl* key)
{
    auto value = adoptCF(CFPreferencesCopyAppValue(createKeyForPreferences(key).get(), kCFPreferencesCurrentApplication));
    if (RetainPtr string = dynamic_cf_cast<CFStringRef>(value.get()))
        return StringImpl::create(string.get());
    if (value == kCFBooleanTrue)
        return "true"_si;
    if (value == kCFBooleanFalse)
        return "false"_si;
    return { };
}

static void storeSetting(StringImpl* key, StringImpl* setting)
{
    CFPreferencesSetAppValue(createKeyForPreferences(key).get(), createCFString(setting).get(), kCFPreferencesCurrentApplication);
}

static void deleteSetting(StringImpl* key)
{
    CFPreferencesSetAppValue(createKeyForPreferences(key).get(), nullptr, kCFPreferencesCurrentApplication);
}

void WebInspectorClient::sendMessageToFrontend(StringImpl* message)
{
    ASSERT(m_frontendClient);
    protect(m_frontendClient->frontendAPIDispatcher())->dispatchMessageAsync(message);
}

bool WebInspectorClient::inspectorAttachDisabled()
{
    return equal(loadSetting(StringImpl::create(inspectorAttachDisabledSetting).ptr()), "true"_s);
}

void WebInspectorClient::setInspectorAttachDisabled(bool disabled)
{
    storeSetting(StringImpl::create(inspectorAttachDisabledSetting).ptr(), StringImpl::create(disabled ? "true"_s : "false"_s).ptr());
}

void WebInspectorClient::deleteInspectorStartsAttached()
{
    deleteSetting(StringImpl::create(inspectorAttachDisabledSetting).ptr());
}

bool WebInspectorClient::inspectorStartsAttached()
{
    return equal(loadSetting(StringImpl::create(inspectorStartsAttachedSetting).ptr()), "true"_s);
}

void WebInspectorClient::setInspectorStartsAttached(bool attached)
{
    storeSetting(StringImpl::create(inspectorStartsAttachedSetting).ptr(), StringImpl::create(attached ? "true"_s : "false"_s).ptr());
}

void WebInspectorClient::deleteInspectorAttachDisabled()
{
    deleteSetting(StringImpl::create(inspectorStartsAttachedSetting).ptr());
}

std::unique_ptr<WebCore::InspectorFrontendClientLocal::Settings> WebInspectorClient::createFrontendSettings()
{
    class InspectorFrontendSettingsCF : public WebCore::InspectorFrontendClientLocal::Settings {
    private:
        RefPtr<StringImpl> getProperty(StringImpl* name) final { return loadSetting(name); }
        void setProperty(StringImpl* name, StringImpl* value) final { storeSetting(name, value); }
        void deleteProperty(StringImpl* name) final { deleteSetting(name); }
    };
    return makeUnique<InspectorFrontendSettingsCF>();
}
