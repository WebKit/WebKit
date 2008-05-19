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
static const char* Script = "[SCRIPT] ";

static void getStackNames(Vector<UString>&, ExecState*);
static void getStackNames(Vector<UString>&, ExecState*, JSObject*);
static void getStackNames(Vector<UString>&, ExecState*, const UString& sourceURL, int startingLineNumber);
static UString getFunctionName(FunctionImp*);

Profiler* Profiler::profiler()
{
    if (!sharedProfiler)
        sharedProfiler = new Profiler;
    return sharedProfiler;
}

void Profiler::startProfiling(unsigned pageGroupIdentifier, const UString& title)
{
    if (m_profiling)
        return;

    m_pageGroupIdentifier = pageGroupIdentifier;

    m_currentProfile = Profile::create(title);
    m_profiling = true;
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

void Profiler::willExecute(ExecState* exec, JSObject* calledFunction)
{
    if (!m_profiling || exec->lexicalGlobalObject()->pageGroupIdentifier() != m_pageGroupIdentifier)
        return;

    Vector<UString> callStackNames;
    getStackNames(callStackNames, exec, calledFunction);
    m_currentProfile->willExecute(callStackNames);
}

void Profiler::willExecute(ExecState* exec, const UString& sourceURL, int startingLineNumber)
{
    if (!m_profiling || exec->lexicalGlobalObject()->pageGroupIdentifier() != m_pageGroupIdentifier)
        return;

    Vector<UString> callStackNames;
    getStackNames(callStackNames, exec, sourceURL, startingLineNumber);
    m_currentProfile->willExecute(callStackNames);
}

void Profiler::didExecute(ExecState* exec, JSObject* calledFunction)
{
    if (!m_profiling || exec->lexicalGlobalObject()->pageGroupIdentifier() != m_pageGroupIdentifier)
        return;

    Vector<UString> callStackNames;
    getStackNames(callStackNames, exec, calledFunction);
    m_currentProfile->didExecute(callStackNames);
}

void Profiler::didExecute(ExecState* exec, const UString& sourceURL, int startingLineNumber)
{
    if (!m_profiling || exec->lexicalGlobalObject()->pageGroupIdentifier() != m_pageGroupIdentifier)
        return;

    Vector<UString> callStackNames;
    getStackNames(callStackNames, exec, sourceURL, startingLineNumber);
    m_currentProfile->didExecute(callStackNames);
}

void getStackNames(Vector<UString>& names, ExecState* exec)
{
    for (ExecState* currentState = exec; currentState; currentState = currentState->callingExecState()) {
        if (FunctionImp* functionImp = currentState->function())
            names.append(getFunctionName(functionImp));
        else if (ScopeNode* scopeNode = currentState->scopeNode())
            names.append(Script + scopeNode->sourceURL() + ": " + UString::from(scopeNode->lineNo() + 1));   // FIXME: Why is the line number always off by one?
    }
}

void getStackNames(Vector<UString>& names, ExecState* exec, JSObject* calledFunction)
{
    if (calledFunction->inherits(&FunctionImp::info))
        names.append(getFunctionName(static_cast<FunctionImp*>(calledFunction)));
    else if (calledFunction->inherits(&InternalFunctionImp::info))
        names.append(static_cast<InternalFunctionImp*>(calledFunction)->functionName().ustring());
    getStackNames(names, exec);
}


void getStackNames(Vector<UString>& names, ExecState* exec, const UString& sourceURL, int startingLineNumber)
{
    names.append(Script + sourceURL + ": " + UString::from(startingLineNumber + 1));
    getStackNames(names, exec);
}

UString getFunctionName(FunctionImp* functionImp)
{
    UString name = functionImp->functionName().ustring();
    int lineNumber = functionImp->body->lineNo();
    UString URL = functionImp->body->sourceURL();

    return (name.isEmpty() ? "[anonymous function]" : name) + " " + URL + ": " + UString::from(lineNumber);
}

void Profiler::debugLog(UString message)
{
    printf("Profiler Log: %s\n", message.UTF8String().c_str());
}

}   // namespace KJS
