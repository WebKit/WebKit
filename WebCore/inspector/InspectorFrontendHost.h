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
#include "ContextMenu.h"
#include "ContextMenuProvider.h"
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

    void copyText(const String& text);

    // Called from [Custom] implementations.
    void showContextMenu(Event*, const Vector<ContextMenuItem*>& items);

private:
    class MenuProvider : public ContextMenuProvider {
    public:
        static PassRefPtr<MenuProvider> create(InspectorFrontendHost* frontendHost, const Vector<ContextMenuItem*>& items)
        {
            return adoptRef(new MenuProvider(frontendHost, items));
        }

        virtual ~MenuProvider()
        {
            contextMenuCleared();
        }

        void disconnect()
        {
            m_frontendHost = 0;
        }

        virtual void populateContextMenu(ContextMenu* menu)
        {
            for (size_t i = 0; i < m_items.size(); ++i)
                menu->appendItem(*m_items[i]);
        }

        virtual void contextMenuItemSelected(ContextMenuItem* item)
        {
            if (m_frontendHost)
                m_frontendHost->contextMenuItemSelected(item);
        }

        virtual void contextMenuCleared()
        {
            if (m_frontendHost)
                m_frontendHost->contextMenuCleared();
            deleteAllValues(m_items);
            m_items.clear();
        }

    private:
        MenuProvider(InspectorFrontendHost* frontendHost,  const Vector<ContextMenuItem*>& items)
            : m_frontendHost(frontendHost)
            , m_items(items) { }
        InspectorFrontendHost* m_frontendHost;
        Vector<ContextMenuItem*> m_items;
    };

    InspectorFrontendHost(InspectorController* inspectorController, InspectorClient* client);

    void contextMenuItemSelected(ContextMenuItem*);
    void contextMenuCleared();

    InspectorController* m_inspectorController;
    InspectorClient* m_client;
    RefPtr<MenuProvider> m_menuProvider;
};

} // namespace WebCore

#endif // !defined(InspectorFrontendHost_h)
