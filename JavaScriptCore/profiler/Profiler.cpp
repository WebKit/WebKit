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

#include "FunctionCallProfile.h"
#include <kjs/ExecState.h>
#include <kjs/function.h>

namespace KJS {

static Profiler* sharedProfiler = 0;
static const char* Script = "[SCRIPT] ";

Profiler* Profiler::profiler()
{
    if (!sharedProfiler)
        sharedProfiler = new Profiler;
    return sharedProfiler;
}

void Profiler::startProfiling()
{
    if (m_profiling)
        return;

    // FIXME: When multi-threading is supported this will be a vector and calls
    // into the profiler will need to know which thread it is executing on.
    m_callTree.set(new FunctionCallProfile("Thread_1"));
    m_profiling = true;
}

void Profiler::stopProfiling()
{
    m_profiling = false;
}

void Profiler::willExecute(ExecState* exec, JSObject* calledFunction)
{
    ASSERT(m_profiling);

    Vector<UString> callStackNames;
    getStackNames(callStackNames, exec, calledFunction);
    insertStackNamesInTree(callStackNames);
}

void Profiler::willExecute(ExecState* exec, const UString& sourceURL, int startingLineNumber)
{
    ASSERT(m_profiling);

    Vector<UString> callStackNames;
    getStackNames(callStackNames, exec, sourceURL, startingLineNumber);
    insertStackNamesInTree(callStackNames);
}

void Profiler::didExecute(ExecState* exec, JSObject* calledFunction)
{
    ASSERT(m_profiling);

    Vector<UString> callStackNames;
    getStackNames(callStackNames, exec, calledFunction);
    m_callTree->didExecute(callStackNames, 0);
}

void Profiler::didExecute(ExecState* exec, const UString& sourceURL, int startingLineNumber)
{
    ASSERT(m_profiling);

    Vector<UString> callStackNames;
    getStackNames(callStackNames, exec, sourceURL, startingLineNumber);
    m_callTree->didExecute(callStackNames, 0);
}

void Profiler::insertStackNamesInTree(const Vector<UString>& callStackNames)
{
    FunctionCallProfile* callTreeInsertionPoint = 0;
    FunctionCallProfile* foundNameInTree = m_callTree.get();
    NameIterator callStackLocation = callStackNames.begin();

    while (callStackLocation != callStackNames.end() && foundNameInTree) {
        callTreeInsertionPoint = foundNameInTree;
        foundNameInTree = callTreeInsertionPoint->findChild(*callStackLocation);
        ++callStackLocation;
    }

    if (!foundNameInTree) {   // Insert remains of the stack into the call tree.
        --callStackLocation;
        for (FunctionCallProfile* next; callStackLocation != callStackNames.end(); ++callStackLocation) {
            next = new FunctionCallProfile(*callStackLocation);
            callTreeInsertionPoint->addChild(next);
            callTreeInsertionPoint = next;
        }
    } else    // We are calling a function that is already in the call tree.
        foundNameInTree->willExecute();
}

void Profiler::getStackNames(Vector<UString>& names, ExecState* exec) const
{
    UString currentName;
    for (ExecState* currentState = exec; currentState; currentState = currentState->callingExecState()) {
        if (FunctionImp* functionImp = exec->function())
            names.prepend(getFunctionName(functionImp));
        else if (ScopeNode* scopeNode = exec->scopeNode())
            names.prepend(Script + scopeNode->sourceURL() + ": " + UString::from(scopeNode->lineNo() + 1));   // FIXME: Why is the line number always off by one?
    }
}

void Profiler::getStackNames(Vector<UString>& names, ExecState* exec, JSObject* calledFunction) const
{
    getStackNames(names, exec);

    if (calledFunction->inherits(&FunctionImp::info))
        names.append(getFunctionName(static_cast<FunctionImp*>(calledFunction)));
    else if (calledFunction->inherits(&InternalFunctionImp::info))
        names.append(static_cast<InternalFunctionImp*>(calledFunction)->functionName().ustring());
}


void Profiler::getStackNames(Vector<UString>& names, ExecState* exec, const UString& sourceURL, int startingLineNumber) const
{
    getStackNames(names, exec);
    names.append(Script + sourceURL + ": " + UString::from(startingLineNumber + 1));
}

UString Profiler::getFunctionName(FunctionImp* functionImp) const
{
    UString name = functionImp->functionName().ustring();
    int lineNumber = functionImp->body->lineNo();
    UString URL = functionImp->body->sourceURL();

    return (name.isEmpty() ? "[anonymous function]" : name) + " " + URL + ": " + UString::from(lineNumber);
}

void Profiler::printDataSampleStyle() const
{
    printf("Call graph:\n");
    m_callTree->printDataSampleStyle(0);

    // FIXME: Since no one seems to understand this part of sample's output I will implement it when I have a better idea of what it's meant to be doing.
    printf("\nTotal number in stack (recursive counted multiple, when >=5):\n");
    printf("\nSort by top of stack, same collapsed (when >= 5):\n");
}

void Profiler::debugLog(UString message)
{
    printf("Profiler Log: %s\n", message.UTF8String().c_str());
}

}   // namespace KJS
