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

#include <JavaScriptCore/JSCJSValue.h>
#include <WebCore/ContextMenu.h>
#include <WebCore/ContextMenuProvider.h>
#include <WebCore/InspectorFrontendClient.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CanvasPath;
class CanvasRenderingContext2D;
class DOMWrapperWorld;
class DeferredPromise;
class Event;
class File;
class FrontendMenuProvider;
class HTMLIFrameElement;
class OffscreenCanvasRenderingContext2D;
class Page;
class Path2D;
template<typename> class ExceptionOr;

class InspectorFrontendHost : public RefCountedAndCanMakeWeakPtr<InspectorFrontendHost> {
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
    void inspectedURLChanged(StringImpl*);

    NODELETE bool supportsShowCertificate() const;
    bool showCertificate(StringImpl* serializedCertificate);

    void setZoomFactor(float);
    float zoomFactor();

    void setForcedAppearance(RefPtr<StringImpl>);

    RefPtr<StringImpl> userInterfaceLayoutDirection();

    bool supportsDockSide(StringImpl*);
    void requestSetDockSide(StringImpl*);

    void setAttachedWindowHeight(unsigned);
    void setAttachedWindowWidth(unsigned);

    void setSheetRect(float x, float y, unsigned width, unsigned height);

    void startWindowDrag();
    void moveWindowBy(float x, float y) const;

    bool isRemote() const;
    RefPtr<StringImpl> localizedStringsURL() const;
    RefPtr<StringImpl> backendCommandsURL() const;
    unsigned inspectionLevel() const;

    RefPtr<StringImpl> platform() const;
    RefPtr<StringImpl> platformVersionName() const;

    struct DebuggableInfo {
        RefPtr<StringImpl> debuggableType;
        RefPtr<StringImpl> targetPlatformName;
        RefPtr<StringImpl> targetBuildVersion;
        RefPtr<StringImpl> targetProductVersion;
        bool targetIsSimulator;
    };
    DebuggableInfo debuggableInfo() const;

    void copyText(StringImpl* text);
    void killText(StringImpl* text, bool shouldPrependToKillRing, bool shouldStartNewSequence);

    void openURLExternally(StringImpl* url);
    void revealFileExternally(StringImpl* path);

    void requestDisableSiteIsolation();

    using SaveMode = InspectorFrontendClient::SaveMode;
    using SaveData = InspectorFrontendClient::SaveData;
    bool canSave(SaveMode);
    void save(Vector<SaveData>&&, bool forceSaveAs);

    bool canLoad();
    void load(StringImpl* path, Ref<DeferredPromise>&&);

    bool canPickColorFromScreen();
    void pickColorFromScreen(Ref<DeferredPromise>&&);

    struct ContextMenuItem {
        RefPtr<StringImpl> type;
        RefPtr<StringImpl> label;
        std::optional<int> id;
        std::optional<bool> enabled;
        std::optional<bool> checked;
        std::optional<Vector<ContextMenuItem>> subItems;
    };
    void showContextMenu(Event&, Vector<ContextMenuItem>&&);

    void sendMessageToBackend(StringImpl* message);
    void dispatchEventAsContextMenuEvent(Event&);

    bool isUnderTest();
    void unbufferedLog(StringImpl* message);

    void beep();
    void inspectInspector();
    bool isBeingInspected();
    void setAllowsInspectingInspector(bool);

    bool engineeringSettingsAllowed();

    bool supportsDiagnosticLogging();
#if ENABLE(INSPECTOR_TELEMETRY)
    bool diagnosticLoggingAvailable();
    void logDiagnosticEvent(StringImpl* eventName, StringImpl* payload);
#endif

    bool supportsWebExtensions();
#if ENABLE(INSPECTOR_EXTENSIONS)
    void didShowExtensionTab(StringImpl* extensionID, StringImpl* extensionTabID, HTMLIFrameElement& extensionFrame);
    void didHideExtensionTab(StringImpl* extensionID, StringImpl* extensionTabID);
    void didNavigateExtensionTab(StringImpl* extensionID, StringImpl* extensionTabID, StringImpl* url);
    void inspectedPageDidNavigate(StringImpl* url);
    ExceptionOr<JSC::JSValue> evaluateScriptInExtensionTab(HTMLIFrameElement& extensionFrame, StringImpl* scriptSource);
#endif

    // IDL extensions.

    NODELETE RefPtr<StringImpl> getPath(const File&) const;

    float getCurrentX(const CanvasRenderingContext2D&) const;
    float getCurrentY(const CanvasRenderingContext2D&) const;
    Ref<Path2D> getPath(const CanvasRenderingContext2D&) const;
    void setPath(CanvasRenderingContext2D&, Path2D&) const;

#if ENABLE(OFFSCREEN_CANVAS)
    float getCurrentX(const OffscreenCanvasRenderingContext2D&) const;
    float getCurrentY(const OffscreenCanvasRenderingContext2D&) const;
    Ref<Path2D> getPath(const OffscreenCanvasRenderingContext2D&) const;
    void setPath(OffscreenCanvasRenderingContext2D&, Path2D&) const;
#endif

private:
#if ENABLE(CONTEXT_MENUS)
    friend class FrontendMenuProvider;
#endif
    WEBCORE_EXPORT InspectorFrontendHost(InspectorFrontendClient*, Page* frontendPage);

    InspectorFrontendClient* m_client;
    WeakPtr<Page> m_frontendPage;
#if ENABLE(CONTEXT_MENUS)
    WeakPtr<FrontendMenuProvider> m_menuProvider;
#endif
};

} // namespace WebCore
