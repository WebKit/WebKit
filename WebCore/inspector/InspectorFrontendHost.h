/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#ifndef InspectorFrontendHost_h
#define InspectorFrontendHost_h

#include "Console.h"
#include "ContextMenuSelectionHandler.h"
#include "InspectorController.h"
#include "PlatformString.h"

#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class ContextMenuItem;
class Event;
class InspectorClient;
class Node;

class InspectorFrontendHost : public RefCounted<InspectorFrontendHost>
{
public:
    static PassRefPtr<InspectorFrontendHost> create(InspectorController* inspectorController, InspectorClient* client)
    {
        return adoptRef(new InspectorFrontendHost(inspectorController, client));
    }

    ~InspectorFrontendHost();

    InspectorController* inspectorController() { return m_inspectorController; }

    void disconnectController() { m_inspectorController = 0; }

    void loaded();
    void attach();
    void detach();
    void closeWindow();
    void windowUnloading();

    void setAttachedWindowHeight(unsigned height);
    void moveWindowBy(float x, float y) const;

    String localizedStringsURL();
    String hiddenPanels();
    const String& platform() const;
    const String& port() const;

    void addResourceSourceToFrame(long identifier, Node* frame);
    bool addSourceToFrame(const String& mimeType, const String& source, Node* frame);

    String setting(const String& key);
    void setSetting(const String& key, const String& value);

    // Called from [Custom] implementations.
    void showContextMenu(Event*, Vector<ContextMenuItem>& items);
    void contextMenuItemSelected(ContextMenuItem*);
    void contextMenuCleared();

private:
    class MenuSelectionHandler : public ContextMenuSelectionHandler {
    public:
        static PassRefPtr<MenuSelectionHandler> create(InspectorFrontendHost* frontendHost)
        {
            return adoptRef(new MenuSelectionHandler(frontendHost));
        }

        virtual ~MenuSelectionHandler() { }

        void disconnect() { m_frontendHost = 0; }

        virtual void contextMenuItemSelected(ContextMenuItem* item)
        {
            if (m_frontendHost)
                m_frontendHost->contextMenuItemSelected(item);
        }

        virtual void contextMenuCleared()
        {
            if (m_frontendHost)
                m_frontendHost->contextMenuCleared();
        }

    private:
        MenuSelectionHandler(InspectorFrontendHost* frontendHost)
            : m_frontendHost(frontendHost) { }
        InspectorFrontendHost* m_frontendHost;
    };

    InspectorFrontendHost(InspectorController* inspectorController, InspectorClient* client);

    InspectorController* m_inspectorController;
    InspectorClient* m_client;
    RefPtr<MenuSelectionHandler> m_menuSelectionHandler;
};

} // namespace WebCore

#endif // !defined(InspectorFrontendHost_h)
