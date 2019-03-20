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

#pragma once

#include "InstrumentingAgents.h"
#include <JavaScriptCore/PerGlobalObjectWrapperWorld.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class JSValue;
}

namespace WebCore {

class Database;
class EventTarget;
class JSDOMGlobalObject;
class Storage;

struct EventListenerInfo;

class CommandLineAPIHost : public RefCounted<CommandLineAPIHost> {
public:
    static Ref<CommandLineAPIHost> create();
    ~CommandLineAPIHost();

    void init(RefPtr<InstrumentingAgents> instrumentingAgents)
    {
        m_instrumentingAgents = instrumentingAgents;
    }

    void disconnect();

    void clearConsoleMessages();
    void copyText(const String& text);

    class InspectableObject {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        virtual JSC::JSValue get(JSC::ExecState&);
        virtual ~InspectableObject() = default;
    };
    void addInspectedObject(std::unique_ptr<InspectableObject>);
    JSC::JSValue inspectedObject(JSC::ExecState&);
    void inspect(JSC::ExecState&, JSC::JSValue objectToInspect, JSC::JSValue hints);

    struct ListenerEntry {
        JSC::Strong<JSC::JSObject> listener;
        bool useCapture;
        bool passive;
        bool once;
    };

    using EventListenersRecord = Vector<WTF::KeyValuePair<String, Vector<ListenerEntry>>>;
    EventListenersRecord getEventListeners(JSC::ExecState&, EventTarget&);

    String databaseId(Database&);
    String storageId(Storage&);

    JSC::JSValue wrapper(JSC::ExecState*, JSDOMGlobalObject*);
    void clearAllWrappers();

private:
    CommandLineAPIHost();

    RefPtr<InstrumentingAgents> m_instrumentingAgents;
    std::unique_ptr<InspectableObject> m_inspectedObject; // $0
    Inspector::PerGlobalObjectWrapperWorld m_wrappers;
};

} // namespace WebCore
