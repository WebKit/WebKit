/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "InspectorInstrumentation.h"
#include "JSExecState.h"
#include <JavaScriptCore/FunctionExecutable.h>

namespace WebCore {

template<typename Type, Type jsType, class DataType>
inline void JSExecState::instrumentFunctionInternal(ScriptExecutionContext* context, Type callType, const DataType& callData)
{
    if (!InspectorInstrumentation::timelineAgentTracking(context))
        return;

    String resourceName;
    int lineNumber = 1;
    int columnNumber = 1;
    if (callType == jsType) {
        resourceName = callData.js.functionExecutable->sourceURL();
        lineNumber = callData.js.functionExecutable->firstLine();
        columnNumber = callData.js.functionExecutable->startColumn();
    } else
        resourceName = "undefined";
    InspectorInstrumentation::willCallFunction(context, resourceName, lineNumber, columnNumber);
}

inline void JSExecState::instrumentFunctionCall(ScriptExecutionContext* context, JSC::CallType type, const JSC::CallData& data)
{
    instrumentFunctionInternal<JSC::CallType, JSC::CallType::JS, JSC::CallData>(context, type, data);
}

inline void JSExecState::instrumentFunctionConstruct(ScriptExecutionContext* context, JSC::ConstructType type, const JSC::ConstructData& data)
{
    instrumentFunctionInternal<JSC::ConstructType, JSC::ConstructType::JS, JSC::ConstructData>(context, type, data);
}

} // namespace WebCore
