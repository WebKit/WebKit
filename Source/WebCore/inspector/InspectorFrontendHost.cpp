/*
 * Copyright (C) 2007-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
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

#include "config.h"
#include "InspectorFrontendHost.h"

#include "CertificateInfo.h"
#include "ContextMenu.h"
#include "ContextMenuController.h"
#include "ContextMenuItem.h"
#include "ContextMenuProvider.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include "Editor.h"
#include "Event.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "Frame.h"
#include "HitTestResult.h"
#include "InspectorController.h"
#include "InspectorDebuggableType.h"
#include "InspectorFrontendClient.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMExceptionHandling.h"
#include "JSExecState.h"
#include "JSInspectorFrontendHost.h"
#include "MouseEvent.h"
#include "Node.h"
#include "Page.h"
#include "Pasteboard.h"
#include "ScriptState.h"
#include "Settings.h"
#include "UserGestureIndicator.h"
#include <JavaScriptCore/ScriptFunctionCall.h>
#include <pal/system/Sound.h>
#include <wtf/JSONValues.h>
#include <wtf/StdLibExtras.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/text/Base64.h>

namespace WebCore {

using namespace Inspector;

#if ENABLE(CONTEXT_MENUS)
class FrontendMenuProvider : public ContextMenuProvider {
public:
    static Ref<FrontendMenuProvider> create(InspectorFrontendHost* frontendHost, Deprecated::ScriptObject frontendApiObject, const Vector<ContextMenuItem>& items)
    {
        return adoptRef(*new FrontendMenuProvider(frontendHost, frontendApiObject, items));
    }
    
    void disconnect()
    {
        m_frontendApiObject = { };
        m_frontendHost = nullptr;
    }
    
private:
    FrontendMenuProvider(InspectorFrontendHost* frontendHost, Deprecated::ScriptObject frontendApiObject, const Vector<ContextMenuItem>& items)
        : m_frontendHost(frontendHost)
        , m_frontendApiObject(frontendApiObject)
        , m_items(items)
    {
    }

    ~FrontendMenuProvider() override
    {
        contextMenuCleared();
    }
    
    void populateContextMenu(ContextMenu* menu) override
    {
        for (auto& item : m_items)
            menu->appendItem(item);
    }
    
    void contextMenuItemSelected(ContextMenuAction action, const String&) override
    {
        if (m_frontendHost) {
            UserGestureIndicator gestureIndicator(ProcessingUserGesture);
            int itemNumber = action - ContextMenuItemBaseCustomTag;

            Deprecated::ScriptFunctionCall function(m_frontendApiObject, "contextMenuItemSelected", WebCore::functionCallHandlerFromAnyThread);
            function.appendArgument(itemNumber);
            function.call();
        }
    }
    
    void contextMenuCleared() override
    {
        if (m_frontendHost) {
            Deprecated::ScriptFunctionCall function(m_frontendApiObject, "contextMenuCleared", WebCore::functionCallHandlerFromAnyThread);
            function.call();

            m_frontendHost->m_menuProvider = nullptr;
        }
        m_items.clear();
    }

    InspectorFrontendHost* m_frontendHost;
    Deprecated::ScriptObject m_frontendApiObject;
    Vector<ContextMenuItem> m_items;
};
#endif

InspectorFrontendHost::InspectorFrontendHost(InspectorFrontendClient* client, Page* frontendPage)
    : m_client(client)
    , m_frontendPage(frontendPage)
#if ENABLE(CONTEXT_MENUS)
    , m_menuProvider(nullptr)
#endif
{
}

InspectorFrontendHost::~InspectorFrontendHost()
{
    ASSERT(!m_client);
}

void InspectorFrontendHost::disconnectClient()
{
    m_client = nullptr;
#if ENABLE(CONTEXT_MENUS)
    if (m_menuProvider)
        m_menuProvider->disconnect();
#endif
    m_frontendPage = nullptr;
}

void InspectorFrontendHost::addSelfToGlobalObjectInWorld(DOMWrapperWorld& world)
{
    auto& lexicalGlobalObject = *execStateFromPage(world, m_frontendPage);
    auto& vm = lexicalGlobalObject.vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject);
    globalObject.putDirect(vm, JSC::Identifier::fromString(vm, "InspectorFrontendHost"), toJS<IDLInterface<InspectorFrontendHost>>(lexicalGlobalObject, globalObject, *this));
    if (UNLIKELY(scope.exception()))
        reportException(&lexicalGlobalObject, scope.exception());
}

void InspectorFrontendHost::loaded()
{
    if (m_client)
        m_client->frontendLoaded();
}

static Optional<InspectorFrontendClient::DockSide> dockSideFromString(const String& dockSideString)
{
    if (dockSideString == "undocked")
        return InspectorFrontendClient::DockSide::Undocked;
    if (dockSideString == "right")
        return InspectorFrontendClient::DockSide::Right;
    if (dockSideString == "left")
        return InspectorFrontendClient::DockSide::Left;
    if (dockSideString == "bottom")
        return InspectorFrontendClient::DockSide::Bottom;
    return WTF::nullopt;
}

bool InspectorFrontendHost::supportsDockSide(const String& dockSideString)
{
    if (!m_client)
        return false;

    auto dockSide = dockSideFromString(dockSideString);
    if (!dockSide)
        return false;

    return m_client->supportsDockSide(dockSide.value());
}

void InspectorFrontendHost::requestSetDockSide(const String& dockSideString)
{
    if (!m_client)
        return;

    auto dockSide = dockSideFromString(dockSideString);
    if (!dockSide)
        return;

    m_client->requestSetDockSide(dockSide.value());
}

void InspectorFrontendHost::closeWindow()
{
    if (m_client) {
        m_client->closeWindow();
        disconnectClient(); // Disconnect from client.
    }
}

void InspectorFrontendHost::reopen()
{
    if (m_client)
        m_client->reopen();
}

void InspectorFrontendHost::reset()
{
    if (m_client)
        m_client->resetState();
    reopen();
}

void InspectorFrontendHost::bringToFront()
{
    if (m_client)
        m_client->bringToFront();
}

void InspectorFrontendHost::inspectedURLChanged(const String& newURL)
{
    if (m_client)
        m_client->inspectedURLChanged(newURL);
}

void InspectorFrontendHost::setZoomFactor(float zoom)
{
    if (m_frontendPage)
        m_frontendPage->mainFrame().setPageAndTextZoomFactors(zoom, 1);
}

float InspectorFrontendHost::zoomFactor()
{
    if (m_frontendPage)
        return m_frontendPage->mainFrame().pageZoomFactor();

    return 1.0;
}

void InspectorFrontendHost::setForcedAppearance(String appearance)
{
    if (appearance == "light"_s) {
        if (m_frontendPage)
            m_frontendPage->setUseDarkAppearanceOverride(false);
        if (m_client)
            m_client->setForcedAppearance(InspectorFrontendClient::Appearance::Light);
        return;
    }

    if (appearance == "dark"_s) {
        if (m_frontendPage)
            m_frontendPage->setUseDarkAppearanceOverride(true);
        if (m_client)
            m_client->setForcedAppearance(InspectorFrontendClient::Appearance::Dark);
        return;
    }

    if (m_frontendPage)
        m_frontendPage->setUseDarkAppearanceOverride(WTF::nullopt);
    if (m_client)
        m_client->setForcedAppearance(InspectorFrontendClient::Appearance::System);
}

String InspectorFrontendHost::userInterfaceLayoutDirection()
{
    if (m_client && m_client->userInterfaceLayoutDirection() == UserInterfaceLayoutDirection::RTL)
        return "rtl"_s;

    return "ltr"_s;
}

void InspectorFrontendHost::setAttachedWindowHeight(unsigned height)
{
    if (m_client)
        m_client->changeAttachedWindowHeight(height);
}

void InspectorFrontendHost::setAttachedWindowWidth(unsigned width)
{
    if (m_client)
        m_client->changeAttachedWindowWidth(width);
}

void InspectorFrontendHost::setSheetRect(float x, float y, unsigned width, unsigned height)
{
    if (m_client)
        m_client->changeSheetRect(FloatRect(x, y, width, height));
}

void InspectorFrontendHost::startWindowDrag()
{
    if (m_client)
        m_client->startWindowDrag();
}

void InspectorFrontendHost::moveWindowBy(float x, float y) const
{
    if (m_client)
        m_client->moveWindowBy(x, y);
}

bool InspectorFrontendHost::isRemote() const
{
    return m_client && m_client->isRemote();
}

String InspectorFrontendHost::localizedStringsURL() const
{
    return m_client ? m_client->localizedStringsURL() : String();
}

String InspectorFrontendHost::backendCommandsURL() const
{
    return m_client ? m_client->backendCommandsURL() : String();
}

static String debuggableTypeToString(DebuggableType debuggableType)
{
    switch (debuggableType) {
    case DebuggableType::ITML:
        return "itml"_s;
    case DebuggableType::JavaScript:
        return "javascript"_s;
    case DebuggableType::Page:
        return "page"_s;
    case DebuggableType::ServiceWorker:
        return "service-worker"_s;
    case DebuggableType::WebPage:
        return "web-page"_s;
    }

    ASSERT_NOT_REACHED();
    return String();
}

InspectorFrontendHost::DebuggableInfo InspectorFrontendHost::debuggableInfo() const
{
    if (!m_client)
        return {debuggableTypeToString(DebuggableType::JavaScript), "Unknown"_s, "Unknown"_s, "Unknown"_s, false};

    return {
        debuggableTypeToString(m_client->debuggableType()),
        m_client->targetPlatformName(),
        m_client->targetBuildVersion(),
        m_client->targetProductVersion(),
        m_client->targetIsSimulator(),
    };
}

unsigned InspectorFrontendHost::inspectionLevel() const
{
    return m_client ? m_client->inspectionLevel() : 1;
}

String InspectorFrontendHost::platform() const
{
#if PLATFORM(COCOA)
    return "mac"_s;
#elif OS(WINDOWS)
    return "windows"_s;
#elif OS(LINUX)
    return "linux"_s;
#elif OS(FREEBSD)
    return "freebsd"_s;
#elif OS(OPENBSD)
    return "openbsd"_s;
#else
    return "unknown"_s;
#endif
}

String InspectorFrontendHost::port() const
{
#if PLATFORM(GTK)
    return "gtk"_s;
#else
    return "unknown"_s;
#endif
}

void InspectorFrontendHost::copyText(const String& text)
{
    Pasteboard::createForCopyAndPaste()->writePlainText(text, Pasteboard::CannotSmartReplace);
}

void InspectorFrontendHost::killText(const String& text, bool shouldPrependToKillRing, bool shouldStartNewSequence)
{
    if (!m_frontendPage)
        return;

    Editor& editor = m_frontendPage->focusController().focusedOrMainFrame().editor();
    editor.setStartNewKillRingSequence(shouldStartNewSequence);
    Editor::KillRingInsertionMode insertionMode = shouldPrependToKillRing ? Editor::KillRingInsertionMode::PrependText : Editor::KillRingInsertionMode::AppendText;
    editor.addTextToKillRing(text, insertionMode);
}

void InspectorFrontendHost::openURLExternally(const String& url)
{
    if (WTF::protocolIsJavaScript(url))
        return;

    if (m_client)
        m_client->openURLExternally(url);
}

bool InspectorFrontendHost::canSave()
{
    if (m_client)
        return m_client->canSave();
    return false;
}

void InspectorFrontendHost::save(const String& url, const String& content, bool base64Encoded, bool forceSaveAs)
{
    if (m_client)
        m_client->save(url, content, base64Encoded, forceSaveAs);
}

void InspectorFrontendHost::append(const String& url, const String& content)
{
    if (m_client)
        m_client->append(url, content);
}

void InspectorFrontendHost::close(const String&)
{
}

void InspectorFrontendHost::sendMessageToBackend(const String& message)
{
    if (m_client)
        m_client->sendMessageToBackend(message);
}

#if ENABLE(CONTEXT_MENUS)

static void populateContextMenu(Vector<InspectorFrontendHost::ContextMenuItem>&& items, ContextMenu& menu)
{
    for (auto& item : items) {
        if (item.type == "separator") {
            menu.appendItem({ SeparatorType, ContextMenuItemTagNoAction, { } });
            continue;
        }

        if (item.type == "subMenu" && item.subItems) {
            ContextMenu subMenu;
            populateContextMenu(WTFMove(*item.subItems), subMenu);

            menu.appendItem({ SubmenuType, ContextMenuItemTagNoAction, item.label, &subMenu });
            continue;
        }

        auto type = item.type == "checkbox" ? CheckableActionType : ActionType;
        auto action = static_cast<ContextMenuAction>(ContextMenuItemBaseCustomTag + item.id.valueOr(0));
        ContextMenuItem menuItem = { type, action, item.label };
        if (item.enabled)
            menuItem.setEnabled(*item.enabled);
        if (item.checked)
            menuItem.setChecked(*item.checked);
        menu.appendItem(menuItem);
    }
}
#endif

void InspectorFrontendHost::showContextMenu(Event& event, Vector<ContextMenuItem>&& items)
{
#if ENABLE(CONTEXT_MENUS)
    ASSERT(m_frontendPage);

    auto& lexicalGlobalObject = *execStateFromPage(debuggerWorld(), m_frontendPage);
    auto& vm = lexicalGlobalObject.vm();
    auto value = lexicalGlobalObject.get(&lexicalGlobalObject, JSC::Identifier::fromString(vm, "InspectorFrontendAPI"));
    ASSERT(value);
    ASSERT(value.isObject());
    auto* frontendAPIObject = asObject(value);
    
    ContextMenu menu;
    populateContextMenu(WTFMove(items), menu);

    auto menuProvider = FrontendMenuProvider::create(this, { &lexicalGlobalObject, frontendAPIObject }, menu.items());
    m_menuProvider = menuProvider.ptr();
    m_frontendPage->contextMenuController().showContextMenu(event, menuProvider);
#else
    UNUSED_PARAM(event);
    UNUSED_PARAM(items);
#endif
}

void InspectorFrontendHost::dispatchEventAsContextMenuEvent(Event& event)
{
#if ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
    if (!is<MouseEvent>(event))
        return;

    auto& mouseEvent = downcast<MouseEvent>(event);
    auto& frame = *downcast<Node>(mouseEvent.target())->document().frame();
    m_frontendPage->contextMenuController().showContextMenuAt(frame, roundedIntPoint(mouseEvent.absoluteLocation()));
#else
    UNUSED_PARAM(event);
#endif
}

bool InspectorFrontendHost::isUnderTest()
{
    return m_client && m_client->isUnderTest();
}

bool InspectorFrontendHost::isExperimentalBuild()
{
#if ENABLE(EXPERIMENTAL_FEATURES)
    return true;
#else
    return false;
#endif
}

void InspectorFrontendHost::unbufferedLog(const String& message)
{
    // This is used only for debugging inspector tests.
    WTFLogAlways("%s", message.utf8().data());
}

void InspectorFrontendHost::beep()
{
    PAL::systemBeep();
}

void InspectorFrontendHost::inspectInspector()
{
    if (m_frontendPage) {
        m_frontendPage->settings().setDeveloperExtrasEnabled(true);
        m_frontendPage->inspectorController().show();
    }
}

bool InspectorFrontendHost::isBeingInspected()
{
    if (!m_frontendPage)
        return false;

    InspectorController& inspectorController = m_frontendPage->inspectorController();
    return inspectorController.hasLocalFrontend() || inspectorController.hasRemoteFrontend();
}

bool InspectorFrontendHost::supportsShowCertificate() const
{
#if PLATFORM(COCOA)
    return true;
#else
    return false;
#endif
}

bool InspectorFrontendHost::showCertificate(const String& serializedCertificate)
{
    if (!m_client)
        return false;

    Vector<uint8_t> data;
    if (!base64Decode(serializedCertificate, data))
        return false;

    WTF::Persistence::Decoder decoder(data.data(), data.size());
    Optional<CertificateInfo> certificateInfo;
    decoder >> certificateInfo;
    if (!certificateInfo)
        return false;

    if (certificateInfo->isEmpty())
        return false;

    m_client->showCertificate(*certificateInfo);
    return true;
}

bool InspectorFrontendHost::supportsDiagnosticLogging()
{
#if ENABLE(INSPECTOR_TELEMETRY)
    return m_client && m_client->supportsDiagnosticLogging();
#else
    return false;
#endif
}

#if ENABLE(INSPECTOR_TELEMETRY)
bool InspectorFrontendHost::diagnosticLoggingAvailable()
{
    return m_client && m_client->diagnosticLoggingAvailable();
}

static Optional<DiagnosticLoggingClient::ValuePayload> valuePayloadFromJSONValue(Ref<JSON::Value>&& value)
{
    switch (value->type()) {
    case JSON::Value::Type::Array:
    case JSON::Value::Type::Null:
    case JSON::Value::Type::Object:
        ASSERT_NOT_REACHED();
        return WTF::nullopt;

    case JSON::Value::Type::Boolean:
        return DiagnosticLoggingClient::ValuePayload(value->asBoolean().valueOr(false));

    case JSON::Value::Type::Double:
        return DiagnosticLoggingClient::ValuePayload(value->asDouble().valueOr(0));

    case JSON::Value::Type::Integer:
        return DiagnosticLoggingClient::ValuePayload(static_cast<long long>(value->asInteger().valueOr(0)));

    case JSON::Value::Type::String:
        return DiagnosticLoggingClient::ValuePayload(value->asString());
    }

    ASSERT_NOT_REACHED();
    return WTF::nullopt;
}

void InspectorFrontendHost::logDiagnosticEvent(const String& eventName, const String& payloadString)
{
    if (!supportsDiagnosticLogging())
        return;

    auto payloadValue = JSON::Value::parseJSON(payloadString);
    if (!payloadValue)
        return;

    auto payloadObject = payloadValue->asObject();
    if (!payloadObject)
        return;

    DiagnosticLoggingClient::ValueDictionary dictionary;
    for (auto& [key, value] : *payloadObject) {
        if (auto valuePayload = valuePayloadFromJSONValue(WTFMove(value)))
            dictionary.set(key, WTFMove(valuePayload.value()));
    }

    m_client->logDiagnosticEvent(makeString("WebInspector."_s, eventName), dictionary);
}
#endif // ENABLE(INSPECTOR_TELEMETRY)

} // namespace WebCore
