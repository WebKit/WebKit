/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#if ENABLE(INSPECTOR)

#include "InspectorFrontendHost.h"

#include "ContextMenu.h"
#include "ContextMenuController.h"
#include "ContextMenuItem.h"
#include "ContextMenuProvider.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include "Event.h"
#include "HitTestResult.h"
#include "InspectorFrontendClient.h"
#include "JSMainThreadExecState.h"
#include "MainFrame.h"
#include "MouseEvent.h"
#include "Node.h"
#include "Page.h"
#include "Pasteboard.h"
#include "ScriptGlobalObject.h"
#include "ScriptState.h"
#include "Sound.h"
#include "UserGestureIndicator.h"
#include <bindings/ScriptFunctionCall.h>
#include <wtf/StdLibExtras.h>

using namespace Inspector;

namespace WebCore {

#if ENABLE(CONTEXT_MENUS)
class FrontendMenuProvider : public ContextMenuProvider {
public:
    static PassRefPtr<FrontendMenuProvider> create(InspectorFrontendHost* frontendHost, Deprecated::ScriptObject frontendApiObject, const Vector<ContextMenuItem>& items)
    {
        return adoptRef(new FrontendMenuProvider(frontendHost, frontendApiObject, items));
    }
    
    void disconnect()
    {
        m_frontendApiObject = Deprecated::ScriptObject();
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
    
    virtual void populateContextMenu(ContextMenu* menu) override
    {
        for (size_t i = 0; i < m_items.size(); ++i)
            menu->appendItem(m_items[i]);
    }
    
    virtual void contextMenuItemSelected(ContextMenuItem* item) override
    {
        if (m_frontendHost) {
            UserGestureIndicator gestureIndicator(DefinitelyProcessingUserGesture);
            int itemNumber = item->action() - ContextMenuItemBaseCustomTag;

            Deprecated::ScriptFunctionCall function(m_frontendApiObject, "contextMenuItemSelected", WebCore::functionCallHandlerFromAnyThread);
            function.appendArgument(itemNumber);
            function.call();
        }
    }
    
    virtual void contextMenuCleared() override
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
        m_client->requestSetDockSide(InspectorFrontendClient::UNDOCKED);
    else if (side == "right")
        m_client->requestSetDockSide(InspectorFrontendClient::DOCKED_TO_RIGHT);
    else if (side == "bottom")
        m_client->requestSetDockSide(InspectorFrontendClient::DOCKED_TO_BOTTOM);
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

void InspectorFrontendHost::setZoomFactor(float zoom)
{
    m_frontendPage->mainFrame().setPageAndTextZoomFactors(zoom, 1);
}

void InspectorFrontendHost::inspectedURLChanged(const String& newURL)
{
    if (m_client)
        m_client->inspectedURLChanged(newURL);
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

void InspectorFrontendHost::setToolbarHeight(unsigned height)
{
    if (m_client)
        m_client->setToolbarHeight(height);
}

void InspectorFrontendHost::moveWindowBy(float x, float y) const
{
    if (m_client)
        m_client->moveWindowBy(x, y);
}

String InspectorFrontendHost::localizedStringsURL()
{
    return m_client ? m_client->localizedStringsURL() : "";
}

String InspectorFrontendHost::debuggableType()
{
    return ASCIILiteral("web");
}

void InspectorFrontendHost::copyText(const String& text)
{
    Pasteboard::createForCopyAndPaste()->writePlainText(text, Pasteboard::CannotSmartReplace);
}

void InspectorFrontendHost::openInNewTab(const String& url)
{
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
void InspectorFrontendHost::showContextMenu(Event* event, const Vector<ContextMenuItem>& items)
{
    if (!event)
        return;

    ASSERT(m_frontendPage);
    JSC::ExecState* frontendExecState = execStateFromPage(debuggerWorld(), m_frontendPage);
    Deprecated::ScriptObject frontendApiObject;
    if (!ScriptGlobalObject::get(frontendExecState, "InspectorFrontendAPI", frontendApiObject)) {
        ASSERT_NOT_REACHED();
        return;
    }
    RefPtr<FrontendMenuProvider> menuProvider = FrontendMenuProvider::create(this, frontendApiObject, items);
    m_frontendPage->contextMenuController().showContextMenu(event, menuProvider);
    m_menuProvider = menuProvider.get();
}
#endif

void InspectorFrontendHost::dispatchEventAsContextMenuEvent(Event* event)
{
#if ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
    if (!event || !event->isMouseEvent())
        return;

    Frame* frame = event->target()->toNode()->document().frame();
    MouseEvent* mouseEvent = toMouseEvent(event);
    IntPoint mousePoint = IntPoint(mouseEvent->clientX(), mouseEvent->clientY());

    m_frontendPage->contextMenuController().showContextMenuAt(frame, mousePoint);
#else
    UNUSED_PARAM(event);
#endif
}

bool InspectorFrontendHost::isUnderTest()
{
    return m_client && m_client->isUnderTest();
}

void InspectorFrontendHost::beep()
{
    systemBeep();
}

bool InspectorFrontendHost::canSaveAs()
{
    return false;
}

bool InspectorFrontendHost::canInspectWorkers()
{
    return false;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
