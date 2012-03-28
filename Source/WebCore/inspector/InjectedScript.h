/*
 * Copyright (C) 2010, 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InjectedScript_h
#define InjectedScript_h

#include "InjectedScriptManager.h"
#include "InspectorTypeBuilder.h"
#include "ScriptObject.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class InspectorArray;
class InspectorObject;
class InspectorValue;
class Node;
class ScriptFunctionCall;

typedef String ErrorString;

#if ENABLE(INSPECTOR)

class InjectedScript {
public:
    InjectedScript();
    ~InjectedScript() { }

    bool hasNoValue() const { return m_injectedScriptObject.hasNoValue(); }

    void evaluate(ErrorString*,
                  const String& expression,
                  const String& objectGroup,
                  bool includeCommandLineAPI,
                  bool returnByValue,
                  RefPtr<TypeBuilder::Runtime::RemoteObject>* result,
                  TypeBuilder::OptOutput<bool>* wasThrown);
    void callFunctionOn(ErrorString*,
                        const String& objectId,
                        const String& expression,
                        const String& arguments,
                        bool returnByValue,
                        RefPtr<TypeBuilder::Runtime::RemoteObject>* result,
                        TypeBuilder::OptOutput<bool>* wasThrown);
    void evaluateOnCallFrame(ErrorString*,
                             const ScriptValue& callFrames,
                             const String& callFrameId,
                             const String& expression,
                             const String& objectGroup,
                             bool includeCommandLineAPI,
                             bool returnByValue,
                             RefPtr<TypeBuilder::Runtime::RemoteObject>* result,
                             TypeBuilder::OptOutput<bool>* wasThrown);
    void getFunctionDetails(ErrorString*, const String& functionId, RefPtr<TypeBuilder::Debugger::FunctionDetails>* result);
    void getProperties(ErrorString*, const String& objectId, bool ownProperties, RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::PropertyDescriptor> >* result);
    Node* nodeForObjectId(const String& objectId);
    void releaseObject(const String& objectId);

#if ENABLE(JAVASCRIPT_DEBUGGER)
    PassRefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CallFrame> > wrapCallFrames(const ScriptValue&);
#endif

    PassRefPtr<TypeBuilder::Runtime::RemoteObject> wrapObject(ScriptValue, const String& groupName) const;
    PassRefPtr<TypeBuilder::Runtime::RemoteObject> wrapNode(Node*, const String& groupName);
    PassRefPtr<TypeBuilder::Runtime::RemoteObject> wrapSerializedObject(SerializedScriptValue*, const String& groupName) const;
    void inspectNode(Node*);
    void releaseObjectGroup(const String&);
    ScriptState* scriptState() const { return m_injectedScriptObject.scriptState(); }

private:
    friend InjectedScript InjectedScriptManager::injectedScriptFor(ScriptState*);
    typedef bool (*InspectedStateAccessCheck)(ScriptState*);
    InjectedScript(ScriptObject, InspectedStateAccessCheck);

    bool canAccessInspectedWindow() const;
    ScriptValue callFunctionWithEvalEnabled(ScriptFunctionCall&, bool& hadException) const;
    void makeCall(ScriptFunctionCall&, RefPtr<InspectorValue>* result);
    void makeEvalCall(ErrorString*, ScriptFunctionCall&, RefPtr<TypeBuilder::Runtime::RemoteObject>* result, TypeBuilder::OptOutput<bool>* wasThrown);
    ScriptValue nodeAsScriptValue(Node*);

    ScriptObject m_injectedScriptObject;
    InspectedStateAccessCheck m_inspectedStateAccessCheck;
};

#endif

} // namespace WebCore

#endif
