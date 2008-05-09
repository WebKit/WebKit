/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "Profile.h"

#include "FunctionCallProfile.h"
#include "JSGlobalObject.h"
#include "ExecState.h"
#include "function.h"

#include <stdio.h>

namespace KJS {

typedef Vector<UString>::const_iterator NameIterator;

Profile::Profile(const UString& title)
    : m_title(title)
{
    // FIXME: When multi-threading is supported this will be a vector and calls
    // into the profiler will need to know which thread it is executing on.
    m_callTree.set(new FunctionCallProfile("Thread_1"));
}

void Profile::willExecute(const Vector<UString>& callStackNames)
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

void Profile::didExecute(Vector<UString> stackNames)
{
    m_callTree->didExecute(stackNames, 0);    
}

void Profile::printDataInspectorStyle() const
{
    printf("Call graph:\n");
    m_callTree->printDataInspectorStyle(0);
}

typedef pair<UString::Rep*, unsigned> NameCountPair;

static inline bool functionNameCountPairComparator(const NameCountPair a, const NameCountPair b)
{
    return a.second > b.second;
}

void Profile::printDataSampleStyle() const
{
    typedef Vector<NameCountPair> NameCountPairVector;

    FunctionCallHashCount countedFunctions;
    printf("Call graph:\n");
    m_callTree->printDataSampleStyle(0, countedFunctions);

    printf("\nTotal number in stack:\n");
    NameCountPairVector sortedFunctions(countedFunctions.size());
    copyToVector(countedFunctions, sortedFunctions);

    std::sort(sortedFunctions.begin(), sortedFunctions.end(), functionNameCountPairComparator);
    for (NameCountPairVector::iterator it = sortedFunctions.begin(); it != sortedFunctions.end(); ++it)
        printf("        %-12d%s\n", (*it).second, UString((*it).first).UTF8String().c_str());

    printf("\nSort by top of stack, same collapsed (when >= 5):\n");
}

}   // namespace KJS
