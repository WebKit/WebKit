/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#if ENABLE(INSPECTOR)

#include "InjectedScriptBase.h"
#include "InspectorJSTypeBuilders.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace Deprecated {
class ScriptObject;
}

namespace Inspector {

class InjectedScriptModule;
class InspectorEnvironment;

class JS_EXPORT_PRIVATE InjectedScript : public InjectedScriptBase {
public:
    InjectedScript();
    InjectedScript(Deprecated::ScriptObject, InspectorEnvironment*);
    virtual ~InjectedScript();

    void evaluate(ErrorString*, const String& expression, const String& objectGroup, bool includeCommandLineAPI, bool returnByValue, bool generatePreview, RefPtr<TypeBuilder::Runtime::RemoteObject>* result, TypeBuilder::OptOutput<bool>* wasThrown);
    void callFunctionOn(ErrorString*, const String& objectId, const String& expression, const String& arguments, bool returnByValue, bool generatePreview, RefPtr<TypeBuilder::Runtime::RemoteObject>* result, TypeBuilder::OptOutput<bool>* wasThrown);
    void evaluateOnCallFrame(ErrorString*, const Deprecated::ScriptValue& callFrames, const String& callFrameId, const String& expression, const String& objectGroup, bool includeCommandLineAPI, bool returnByValue, bool generatePreview, RefPtr<TypeBuilder::Runtime::RemoteObject>* result, TypeBuilder::OptOutput<bool>* wasThrown);
    void getFunctionDetails(ErrorString*, const String& functionId, RefPtr<TypeBuilder::Debugger::FunctionDetails>* result);
    void getProperties(ErrorString*, const String& objectId, bool ownProperties, bool ownAndGetterProperties, RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::PropertyDescriptor>>* result);
    void getInternalProperties(ErrorString*, const String& objectId, RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::InternalPropertyDescriptor>>* result);

    PassRefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CallFrame>> wrapCallFrames(const Deprecated::ScriptValue&);
    PassRefPtr<TypeBuilder::Runtime::RemoteObject> wrapObject(const Deprecated::ScriptValue&, const String& groupName, bool generatePreview = false) const;
    PassRefPtr<TypeBuilder::Runtime::RemoteObject> wrapTable(const Deprecated::ScriptValue& table, const Deprecated::ScriptValue& columns) const;

    Deprecated::ScriptValue findObjectById(const String& objectId) const;
    void inspectObject(Deprecated::ScriptValue);
    void releaseObject(const String& objectId);
    void releaseObjectGroup(const String& objectGroup);

private:
    friend class InjectedScriptModule;
};

} // namespace Inspector

#endif // ENABLE(INSPECTOR)

#endif // InjectedScript_h
