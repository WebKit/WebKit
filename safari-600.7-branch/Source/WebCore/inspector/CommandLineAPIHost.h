/*
 * Copyright (C) 2007, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef CommandLineAPIHost_h
#define CommandLineAPIHost_h

#include "ScriptState.h"
#include <runtime/ConsoleTypes.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace Deprecated {
class ScriptValue;
}

namespace Inspector {
class InspectorAgent;
class InspectorConsoleAgent;
class InspectorObject;
class InspectorValue;
}

namespace WebCore {

class Database;
class InspectorDOMAgent;
class InspectorDOMStorageAgent;
class InspectorDatabaseAgent;
class Node;
class Storage;

struct EventListenerInfo;

class CommandLineAPIHost : public RefCounted<CommandLineAPIHost> {
public:
    static PassRefPtr<CommandLineAPIHost> create();
    ~CommandLineAPIHost();

    void init(Inspector::InspectorAgent* inspectorAgent
        , Inspector::InspectorConsoleAgent* consoleAgent
        , InspectorDOMAgent* domAgent
        , InspectorDOMStorageAgent* domStorageAgent
#if ENABLE(SQL_DATABASE)
        , InspectorDatabaseAgent* databaseAgent
#endif
        )
    {
        m_inspectorAgent = inspectorAgent;
        m_consoleAgent = consoleAgent;
        m_domAgent = domAgent;
        m_domStorageAgent = domStorageAgent;
#if ENABLE(SQL_DATABASE)
        m_databaseAgent = databaseAgent;
#endif
    }

    void disconnect();

    void clearConsoleMessages();
    void copyText(const String& text);

    class InspectableObject {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        virtual Deprecated::ScriptValue get(JSC::ExecState*);
        virtual ~InspectableObject() { }
    };
    void addInspectedObject(std::unique_ptr<InspectableObject>);
    void clearInspectedObjects();
    InspectableObject* inspectedObject(unsigned index);
    void inspectImpl(PassRefPtr<Inspector::InspectorValue> objectToInspect, PassRefPtr<Inspector::InspectorValue> hints);

    void getEventListenersImpl(Node*, Vector<EventListenerInfo>& listenersArray);

#if ENABLE(SQL_DATABASE)
    String databaseIdImpl(Database*);
#endif
    String storageIdImpl(Storage*);

private:
    CommandLineAPIHost();

    Inspector::InspectorAgent* m_inspectorAgent;
    Inspector::InspectorConsoleAgent* m_consoleAgent;
    InspectorDOMAgent* m_domAgent;
    InspectorDOMStorageAgent* m_domStorageAgent;
#if ENABLE(SQL_DATABASE)
    InspectorDatabaseAgent* m_databaseAgent;
#endif

    Vector<std::unique_ptr<InspectableObject>> m_inspectedObjects;
    std::unique_ptr<InspectableObject> m_defaultInspectableObject;
};

} // namespace WebCore

#endif // !defined(CommandLineAPIHost_h)
