/*
* Copyright (C) 2014 Apple Inc. All rights reserved.
* Copyright (C) 2011 Google Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1.  Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
* 2.  Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in the
*     documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef InspectorConsoleAgent_h
#define InspectorConsoleAgent_h

#if ENABLE(INSPECTOR)

#include "InspectorJSBackendDispatchers.h"
#include "InspectorJSFrontendDispatchers.h"
#include "inspector/ConsoleTypes.h"
#include "inspector/InspectorAgentBase.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>

namespace JSC {
class ExecState;
}

namespace Inspector {

class ConsoleMessage;
class InjectedScriptManager;
class ScriptArguments;
class ScriptCallStack;
typedef String ErrorString;

class JS_EXPORT_PRIVATE InspectorConsoleAgent : public InspectorAgentBase, public InspectorConsoleBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorConsoleAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorConsoleAgent(InjectedScriptManager*);
    virtual ~InspectorConsoleAgent();

    virtual void didCreateFrontendAndBackend(InspectorFrontendChannel*, InspectorBackendDispatcher*) override;
    virtual void willDestroyFrontendAndBackend(InspectorDisconnectReason) override;

    virtual void enable(ErrorString*) override;
    virtual void disable(ErrorString*) override;
    virtual void clearMessages(ErrorString*) override;
    virtual void setMonitoringXHREnabled(ErrorString*, bool enabled) = 0;
    virtual void addInspectedNode(ErrorString*, int nodeId) = 0;
    virtual void addInspectedHeapObject(ErrorString*, int inspectedHeapObjectId) = 0;

    virtual bool isWorkerAgent() const = 0;

    bool enabled() const { return m_enabled; }
    void reset();

    void addMessageToConsole(MessageSource, MessageType, MessageLevel, const String& message, JSC::ExecState*, PassRefPtr<ScriptArguments>, unsigned long requestIdentifier = 0);
    void addMessageToConsole(MessageSource, MessageType, MessageLevel, const String& message, const String& scriptID, unsigned lineNumber, unsigned columnNumber, JSC::ExecState* = nullptr, unsigned long requestIdentifier = 0);
    void addMessageToConsole(MessageSource, MessageType, MessageLevel, const String& message, PassRefPtr<ScriptCallStack>, unsigned long requestIdentifier = 0);

    Vector<unsigned> consoleMessageArgumentCounts() const;

    void startTiming(const String& title);
    void stopTiming(const String& title, PassRefPtr<ScriptCallStack>);
    void count(JSC::ExecState*, PassRefPtr<ScriptArguments>);

protected:
    void addConsoleMessage(PassOwnPtr<ConsoleMessage>);

    InjectedScriptManager* m_injectedScriptManager;
    std::unique_ptr<InspectorConsoleFrontendDispatcher> m_frontendDispatcher;
    RefPtr<InspectorConsoleBackendDispatcher> m_backendDispatcher;
    ConsoleMessage* m_previousMessage;
    Vector<OwnPtr<ConsoleMessage>> m_consoleMessages;
    int m_expiredConsoleMessageCount;
    HashMap<String, unsigned> m_counts;
    HashMap<String, double> m_times;
    bool m_enabled;
};

} // namespace Inspector

#endif // ENABLE(INSPECTOR)

#endif // !defined(InspectorConsoleAgent_h)
