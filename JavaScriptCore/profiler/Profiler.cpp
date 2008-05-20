/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Profiler.h"

#include "ExecState.h"
#include "function.h"
#include "ProfileNode.h"
#include "JSGlobalObject.h"
#include "Profile.h"

#include <stdio.h>

namespace KJS {

static Profiler* sharedProfiler = 0;
static const char* GlobalCodeExecution = "(global code)";
static const char* AnonymousFunction = "(anonymous function)";

static void getCallIdentifiers(ExecState*, Vector<CallIdentifier>& callIdentifiers);
static void getCallIdentifiers(ExecState*, JSObject*, Vector<CallIdentifier>& callIdentifiers);
static void getCallIdentifiers(ExecState*, const UString& sourceURL, int startingLineNumber, Vector<CallIdentifier>& callIdentifiers);
static void getCallIdentifierFromFunctionImp(FunctionImp*, Vector<CallIdentifier>& callIdentifiers);

Profiler* Profiler::profiler()
{
    if (!sharedProfiler)
        sharedProfiler = new Profiler;
    return sharedProfiler;
}

void Profiler::startProfiling(ExecState* exec, unsigned pageGroupIdentifier, const UString& title)
{
    if (m_profiling)
        return;

    m_pageGroupIdentifier = pageGroupIdentifier;

    m_currentProfile = Profile::create(title);
    m_profiling = true;
    
    Vector<CallIdentifier> callIdentifiers;
    getCallIdentifiers(exec, callIdentifiers);
    m_currentProfile->willExecute(callIdentifiers);
}

void Profiler::stopProfiling()
{
    m_profiling = false;

    if (!m_currentProfile)
        return;

    m_currentProfile->stopProfiling();
    m_allProfiles.append(m_currentProfile.release());
    m_currentProfile = 0;
}

static inline bool shouldExcludeFunction(ExecState* exec, JSObject* calledFunction)
{
    if (!calledFunction->inherits(&InternalFunctionImp::info))
        return false;
    // Don't record a call for function.call and function.apply.
    if (static_cast<InternalFunctionImp*>(calledFunction)->functionName() == exec->propertyNames().call)
        return true;
    if (static_cast<InternalFunctionImp*>(calledFunction)->functionName() == exec->propertyNames().apply)
        return true;
    return false;
}

void Profiler::willExecute(ExecState* exec, JSObject* calledFunction)
{
    if (!m_profiling || exec->lexicalGlobalObject()->pageGroupIdentifier() != m_pageGroupIdentifier)
        return;

    if (shouldExcludeFunction(exec, calledFunction))
        return;

    Vector<CallIdentifier> callIdentifiers;
    getCallIdentifiers(exec, calledFunction, callIdentifiers);
    m_currentProfile->willExecute(callIdentifiers);
}

void Profiler::willExecute(ExecState* exec, const UString& sourceURL, int startingLineNumber)
{
    if (!m_profiling || exec->lexicalGlobalObject()->pageGroupIdentifier() != m_pageGroupIdentifier)
        return;

    Vector<CallIdentifier> callIdentifiers;
    getCallIdentifiers(exec, sourceURL, startingLineNumber, callIdentifiers);
    m_currentProfile->willExecute(callIdentifiers);
}

void Profiler::didExecute(ExecState* exec, JSObject* calledFunction)
{
    if (!m_profiling || exec->lexicalGlobalObject()->pageGroupIdentifier() != m_pageGroupIdentifier)
        return;

    if (shouldExcludeFunction(exec, calledFunction))
        return;

    Vector<CallIdentifier> callIdentifiers;
    getCallIdentifiers(exec, calledFunction, callIdentifiers);
    m_currentProfile->didExecute(callIdentifiers);
}

void Profiler::didExecute(ExecState* exec, const UString& sourceURL, int startingLineNumber)
{
    if (!m_profiling || exec->lexicalGlobalObject()->pageGroupIdentifier() != m_pageGroupIdentifier)
        return;

    Vector<CallIdentifier> callIdentifiers;
    getCallIdentifiers(exec, sourceURL, startingLineNumber, callIdentifiers);
    m_currentProfile->didExecute(callIdentifiers);
}

void getCallIdentifiers(ExecState* exec, Vector<CallIdentifier>& callIdentifiers)
{
    for (ExecState* currentState = exec; currentState; currentState = currentState->callingExecState()) {
        if (FunctionImp* functionImp = currentState->function())
            getCallIdentifierFromFunctionImp(functionImp, callIdentifiers);
        else if (ScopeNode* scopeNode = currentState->scopeNode())
            callIdentifiers.append(CallIdentifier(GlobalCodeExecution, scopeNode->sourceURL(), (scopeNode->lineNo() + 1)) );   // FIXME: Why is the line number always off by one?
    }
}

void getCallIdentifiers(ExecState* exec, JSObject* calledFunction, Vector<CallIdentifier>& callIdentifiers)
{
    if (calledFunction->inherits(&FunctionImp::info))
        getCallIdentifierFromFunctionImp(static_cast<FunctionImp*>(calledFunction), callIdentifiers);
    else if (calledFunction->inherits(&InternalFunctionImp::info))
        callIdentifiers.append(CallIdentifier(static_cast<InternalFunctionImp*>(calledFunction)->functionName().ustring(), "", 0) );
    getCallIdentifiers(exec, callIdentifiers);
}

void getCallIdentifiers(ExecState* exec, const UString& sourceURL, int startingLineNumber, Vector<CallIdentifier>& callIdentifiers)
{
    callIdentifiers.append(CallIdentifier(GlobalCodeExecution, sourceURL, (startingLineNumber + 1)) );
    getCallIdentifiers(exec, callIdentifiers);
}

void getCallIdentifierFromFunctionImp(FunctionImp* functionImp, Vector<CallIdentifier>& callIdentifiers)
{
    UString name = functionImp->functionName().ustring();
    if (name.isEmpty())
        name = AnonymousFunction;

    callIdentifiers.append(CallIdentifier(name, functionImp->body->sourceURL(), functionImp->body->lineNo()) );
}

void Profiler::debugLog(UString message)
{
    printf("Profiler Log: %s\n", message.UTF8String().c_str());
}

}   // namespace KJS
