/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
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
#include "FocusController.h"
#include "Frame.h"
#include "HitTestResult.h"
#include "InspectorController.h"
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
#include "UserGestureIndicator.h"
#include <JavaScriptCore/ScriptFunctionCall.h>
#include <pal/system/Sound.h>
#include <wtf/StdLibExtras.h>
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

    virtual ~FrontendMenuProvider()
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
    auto& state = *execStateFromPage(world, m_frontendPage);
    auto& vm = state.vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(state.lexicalGlobalObject());
    globalObject.putDirect(vm, JSC::Identifier::fromString(&vm, "InspectorFrontendHost"), toJS<IDLInterface<InspectorFrontendHost>>(state, globalObject, *this));
    if (UNLIKELY(scope.exception()))
        reportException(&state, scope.exception());
}

void InspectorFrontendHost::loaded()
{
    if (m_client)
        m_client->frontendLoaded();
}

void InspectorFrontendHost::requestSetDockSide(const String& side)
{
    if (!m_client)
        return;
    if (side == "undocked")
        m_client->requestSetDockSide(InspectorFrontendClient::DockSide::Undocked);
    else if (side == "right")
        m_client->requestSetDockSide(InspectorFrontendClient::DockSide::Right);
    else if (side == "left")
        m_client->requestSetDockSide(InspectorFrontendClient::DockSide::Left);
    else if (side == "bottom")
        m_client->requestSetDockSide(InspectorFrontendClient::DockSide::Bottom);
}

void InspectorFrontendHost::closeWindow()
{
    if (m_client) {
        m_client->closeWindow();
        disconnectClient(); // Disconnect from client.
    }
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

String InspectorFrontendHost::localizedStringsURL()
{
    return m_client ? m_client->localizedStringsURL() : String();
}

String InspectorFrontendHost::backendCommandsURL()
{
    return m_client ? m_client->backendCommandsURL() : String();
}

String InspectorFrontendHost::debuggableType()
{
    return m_client ? m_client->debuggableType() : String();
}

unsigned InspectorFrontendHost::inspectionLevel()
{
    return m_client ? m_client->inspectionLevel() : 1;
}

String InspectorFrontendHost::platform()
{
#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
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

String InspectorFrontendHost::port()
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

void InspectorFrontendHost::openInNewTab(const String& url)
{
    if (WebCore::protocolIsJavaScript(url))
        return;

    if (m_client)
        m_client->openInNewTab(url);
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
        auto action = static_cast<ContextMenuAction>(ContextMenuItemBaseCustomTag + item.id.value_or(0));
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

    auto& state = *execStateFromPage(debuggerWorld(), m_frontendPage);
    auto value = state.lexicalGlobalObject()->get(&state, JSC::Identifier::fromString(&state.vm(), "InspectorFrontendAPI"));
    ASSERT(value);
    ASSERT(value.isObject());
    auto* frontendAPIObject = asObject(value);
    
    ContextMenu menu;
    populateContextMenu(WTFMove(items), menu);

    auto menuProvider = FrontendMenuProvider::create(this, { &state, frontendAPIObject }, menu.items());
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
    IntPoint mousePoint { mouseEvent.clientX(), mouseEvent.clientY() };
    auto& frame = *downcast<Node>(mouseEvent.target())->document().frame();

    m_frontendPage->contextMenuController().showContextMenuAt(frame, mousePoint);
#else
    UNUSED_PARAM(event);
#endif
}

bool InspectorFrontendHost::isUnderTest()
{
    return m_client && m_client->isUnderTest();
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
    if (m_frontendPage)
        m_frontendPage->inspectorController().show();
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

    CertificateInfo certificateInfo;
    WTF::Persistence::Decoder decoder(data.data(), data.size());
    if (!decoder.decode(certificateInfo))
        return false;

    if (certificateInfo.isEmpty())
        return false;

    m_client->showCertificate(certificateInfo);
    return true;
}

} // namespace WebCore
