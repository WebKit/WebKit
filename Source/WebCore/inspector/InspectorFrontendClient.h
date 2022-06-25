/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CertificateInfo.h"
#include "Color.h"
#include "DiagnosticLoggingClient.h"
#include "FrameIdentifier.h"
#include "InspectorDebuggableType.h"
#include "UserInterfaceLayoutDirection.h"
#include <wtf/Forward.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if ENABLE(INSPECTOR_EXTENSIONS)
namespace Inspector {
using ExtensionID = String;
using ExtensionTabID = String;
}
#endif

namespace WebCore {

class FloatRect;
class InspectorFrontendAPIDispatcher;
class Page;

class InspectorFrontendClient : public CanMakeWeakPtr<InspectorFrontendClient> {
public:
    enum class DockSide {
        Undocked = 0,
        Right,
        Left,
        Bottom,
    };

    virtual ~InspectorFrontendClient() = default;

    WEBCORE_EXPORT virtual void windowObjectCleared() = 0;
    virtual void frontendLoaded() = 0;

    virtual void pagePaused() = 0;
    virtual void pageUnpaused() = 0;

    virtual void startWindowDrag() = 0;
    virtual void moveWindowBy(float x, float y) = 0;

    // Information about the debuggable.
    virtual bool isRemote() const = 0;
    virtual String localizedStringsURL() const = 0;
    virtual String backendCommandsURL() const = 0;
    virtual Inspector::DebuggableType debuggableType() const = 0;
    virtual String targetPlatformName() const = 0;
    virtual String targetBuildVersion() const = 0;
    virtual String targetProductVersion() const = 0;
    virtual bool targetIsSimulator() const = 0;
    virtual unsigned inspectionLevel() const = 0;

    virtual void bringToFront() = 0;
    virtual void closeWindow() = 0;
    virtual void reopen() = 0;
    virtual void resetState() = 0;

    enum class Appearance {
        System,
        Light,
        Dark,
    };
    WEBCORE_EXPORT virtual void setForcedAppearance(Appearance) = 0;

    virtual UserInterfaceLayoutDirection userInterfaceLayoutDirection() const = 0;

    WEBCORE_EXPORT virtual bool supportsDockSide(DockSide) = 0;
    WEBCORE_EXPORT virtual void requestSetDockSide(DockSide) = 0;
    WEBCORE_EXPORT virtual void changeAttachedWindowHeight(unsigned) = 0;
    WEBCORE_EXPORT virtual void changeAttachedWindowWidth(unsigned) = 0;

    WEBCORE_EXPORT virtual void changeSheetRect(const FloatRect&) = 0;

    WEBCORE_EXPORT virtual void openURLExternally(const String& url) = 0;
    WEBCORE_EXPORT virtual void revealFileExternally(const String& path) = 0;

    // Keep in sync with `WI.FileUtilities.SaveMode` and `InspectorFrontendHost::SaveMode`.
    enum class SaveMode : uint8_t {
        SingleFile,
        FileVariants,
    };
    struct SaveData {
        String displayType;
        String url;
        String content;
        bool base64Encoded;

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static std::optional<SaveData> decode(Decoder&);
    };
    virtual bool canSave(SaveMode) = 0;
    virtual void save(Vector<SaveData>&&, bool forceSaveAs) = 0;

    virtual bool canLoad() = 0;
    virtual void load(const String& path, CompletionHandler<void(const String&)>&&) = 0;

    virtual bool canPickColorFromScreen() = 0;
    virtual void pickColorFromScreen(CompletionHandler<void(const std::optional<WebCore::Color>&)>&&) = 0;

    virtual void inspectedURLChanged(const String&) = 0;
    virtual void showCertificate(const CertificateInfo&) = 0;

#if ENABLE(INSPECTOR_TELEMETRY)
    virtual bool supportsDiagnosticLogging() { return false; }
    virtual bool diagnosticLoggingAvailable() { return false; }
    virtual void logDiagnosticEvent(const String& /* eventName */, const DiagnosticLoggingClient::ValueDictionary&) { }
#endif

#if ENABLE(INSPECTOR_EXTENSIONS)
    virtual bool supportsWebExtensions() { return false; }
    virtual void didShowExtensionTab(const Inspector::ExtensionID&, const Inspector::ExtensionTabID&, FrameIdentifier) { }
    virtual void didHideExtensionTab(const Inspector::ExtensionID&, const Inspector::ExtensionTabID&) { }
    virtual void didNavigateExtensionTab(const Inspector::ExtensionID&, const Inspector::ExtensionTabID&, const URL&) { }
    virtual void inspectedPageDidNavigate(const URL&) { }
#endif

    WEBCORE_EXPORT virtual void sendMessageToBackend(const String&) = 0;
    WEBCORE_EXPORT virtual InspectorFrontendAPIDispatcher& frontendAPIDispatcher() = 0;
    WEBCORE_EXPORT virtual Page* frontendPage() = 0;

    WEBCORE_EXPORT virtual bool isUnderTest() = 0;
};

template<class Encoder>
void InspectorFrontendClient::SaveData::encode(Encoder& encoder) const
{
    encoder << displayType;
    encoder << url;
    encoder << content;
    encoder << base64Encoded;
}

template<class Decoder>
std::optional<InspectorFrontendClient::SaveData> InspectorFrontendClient::SaveData::decode(Decoder& decoder)
{
#define DECODE(name, type) \
    std::optional<type> name; \
    decoder >> name; \
    if (!name) \
        return std::nullopt; \

    DECODE(displayType, String)
    DECODE(url, String)
    DECODE(content, String)
    DECODE(base64Encoded, bool)

#undef DECODE

    return { {
        WTFMove(*displayType),
        WTFMove(*url),
        WTFMove(*content),
        WTFMove(*base64Encoded),
    } };
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::InspectorFrontendClient::Appearance> {
    using values = EnumValues<
        WebCore::InspectorFrontendClient::Appearance,
        WebCore::InspectorFrontendClient::Appearance::System,
        WebCore::InspectorFrontendClient::Appearance::Light,
        WebCore::InspectorFrontendClient::Appearance::Dark
    >;
};

template<> struct EnumTraits<WebCore::InspectorFrontendClient::SaveMode> {
    using values = EnumValues<
        WebCore::InspectorFrontendClient::SaveMode,
        WebCore::InspectorFrontendClient::SaveMode::SingleFile,
        WebCore::InspectorFrontendClient::SaveMode::FileVariants
    >;
};

} // namespace WTF
