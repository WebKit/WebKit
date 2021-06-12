/*
 * Copyright (C) 2007-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ContextMenu.h"
#include "ContextMenuProvider.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class DOMWrapperWorld;
class Event;
class FrontendMenuProvider;
class InspectorFrontendClient;
class Page;

class InspectorFrontendHost : public RefCounted<InspectorFrontendHost> {
public:
    static Ref<InspectorFrontendHost> create(InspectorFrontendClient* client, Page* frontendPage)
    {
        return adoptRef(*new InspectorFrontendHost(client, frontendPage));
    }

    WEBCORE_EXPORT ~InspectorFrontendHost();
    WEBCORE_EXPORT void disconnectClient();

    WEBCORE_EXPORT void addSelfToGlobalObjectInWorld(DOMWrapperWorld&);

    void loaded();
    void closeWindow();
    void reopen();
    void reset();

    void bringToFront();
    void inspectedURLChanged(const String&);

    bool supportsShowCertificate() const;
    bool showCertificate(const String& serializedCertificate);

    void setZoomFactor(float);
    float zoomFactor();

    void setForcedAppearance(String);

    String userInterfaceLayoutDirection();

    bool supportsDockSide(const String&);
    void requestSetDockSide(const String&);

    void setAttachedWindowHeight(unsigned);
    void setAttachedWindowWidth(unsigned);

    void setSheetRect(float x, float y, unsigned width, unsigned height);

    void startWindowDrag();
    void moveWindowBy(float x, float y) const;

    bool isRemote() const;
    String localizedStringsURL() const;
    String backendCommandsURL() const;
    unsigned inspectionLevel() const;

    String platform() const;
    String platformVersionName() const;
    String port() const;

    struct DebuggableInfo {
        String debuggableType;
        String targetPlatformName;
        String targetBuildVersion;
        String targetProductVersion;
        bool targetIsSimulator;
    };
    DebuggableInfo debuggableInfo() const;

    void copyText(const String& text);
    void killText(const String& text, bool shouldPrependToKillRing, bool shouldStartNewSequence);
    void openURLExternally(const String& url);
    bool canSave();
    void save(const String& url, const String& content, bool base64Encoded, bool forceSaveAs);
    void append(const String& url, const String& content);
    void close(const String& url);

    struct ContextMenuItem {
        String type;
        String label;
        std::optional<int> id;
        std::optional<bool> enabled;
        std::optional<bool> checked;
        std::optional<Vector<ContextMenuItem>> subItems;
    };
    void showContextMenu(Event&, Vector<ContextMenuItem>&&);

    void sendMessageToBackend(const String& message);
    void dispatchEventAsContextMenuEvent(Event&);

    bool isUnderTest();
    bool isExperimentalBuild();
    void unbufferedLog(const String& message);

    void beep();
    void inspectInspector();
    bool isBeingInspected();
    void setAllowsInspectingInspector(bool);

    bool supportsDiagnosticLogging();
#if ENABLE(INSPECTOR_TELEMETRY)
    bool diagnosticLoggingAvailable();
    void logDiagnosticEvent(const String& eventName, const String& payload);
#endif

    bool supportsWebExtensions();
#if ENABLE(INSPECTOR_EXTENSIONS)
    void didShowExtensionTab(const String& extensionID, const String& extensionTabID);
    void didHideExtensionTab(const String& extensionID, const String& extensionTabID);
#endif

private:
#if ENABLE(CONTEXT_MENUS)
    friend class FrontendMenuProvider;
#endif
    WEBCORE_EXPORT InspectorFrontendHost(InspectorFrontendClient*, Page* frontendPage);

    InspectorFrontendClient* m_client;
    Page* m_frontendPage;
#if ENABLE(CONTEXT_MENUS)
    FrontendMenuProvider* m_menuProvider;
#endif
};

} // namespace WebCore
