/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorEnvironment_h
#define InspectorEnvironment_h

#include "CallData.h"

namespace JSC {
class SourceCode;
}

namespace Inspector {

typedef JSC::JSValue (*InspectorFunctionCallHandler)(JSC::ExecState* exec, JSC::JSValue functionObject, JSC::CallType callType, const JSC::CallData& callData, JSC::JSValue thisValue, const JSC::ArgList& args);
typedef JSC::JSValue (*InspectorEvaluateHandler)(JSC::ExecState*, const JSC::SourceCode&, JSC::JSValue thisValue, JSC::JSValue* exception);

class InspectorEnvironment {
public:
    virtual ~InspectorEnvironment() { }
    virtual bool developerExtrasEnabled() const = 0;
    virtual bool canAccessInspectedScriptState(JSC::ExecState*) const = 0;
    virtual InspectorFunctionCallHandler functionCallHandler() const = 0;
    virtual InspectorEvaluateHandler evaluateHandler() const = 0;
    virtual void willCallInjectedScriptFunction(JSC::ExecState*, const String& scriptName, int scriptLine) = 0;
    virtual void didCallInjectedScriptFunction(JSC::ExecState*) = 0;
};

} // namespace Inspector

#endif // !defined(InspectorEnvironment_h)
