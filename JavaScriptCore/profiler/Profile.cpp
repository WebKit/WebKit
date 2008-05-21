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

#include "ProfileNode.h"
#include "JSGlobalObject.h"
#include "ExecState.h"
#include "function.h"

#include <stdio.h>

namespace KJS {

Profile::Profile(const UString& title, ExecState* originatingGlobalExec, unsigned pageGroupIdentifier)
    : m_title(title)
    , m_originatingGlobalExec(originatingGlobalExec)
    , m_pageGroupIdentifier(pageGroupIdentifier)
{
    // FIXME: When multi-threading is supported this will be a vector and calls
    // into the profiler will need to know which thread it is executing on.
    m_callTree = ProfileNode::create(CallIdentifier("Thread_1", 0, 0));
}

void Profile::stopProfiling()
{
    m_originatingGlobalExec = 0;
    m_callTree->stopProfiling(0, true);
}

// The callIdentifiers are in order of bottom of the stack to top of the stack so we iterate it backwards.
void Profile::willExecute(const Vector<CallIdentifier>& callIdentifiers)
{
    RefPtr<ProfileNode> callTreeInsertionPoint;
    RefPtr<ProfileNode> foundNameInTree = m_callTree;

    int i = callIdentifiers.size();
    while (foundNameInTree && i) {
        callTreeInsertionPoint = foundNameInTree;
        foundNameInTree = callTreeInsertionPoint->findChild(callIdentifiers[--i]);
    }

    if (!foundNameInTree) {   // Insert remains of the stack into the call tree.
        for (RefPtr<ProfileNode> next; i >= 0; callTreeInsertionPoint = next) {
            next = ProfileNode::create(callIdentifiers[i--]);
            callTreeInsertionPoint->addChild(next);
        }
    } else    // We are calling a function that is already in the call tree.
        foundNameInTree->willExecute();
}

void Profile::didExecute(const Vector<CallIdentifier>& callIdentifiers)
{
    if (!callIdentifiers.size())
        return;

    m_callTree->didExecute(callIdentifiers, callIdentifiers.size() - 1);
}

#ifndef NDEBUG
void Profile::debugPrintData() const
{
    printf("Call graph:\n");
    m_callTree->debugPrintData(0);
}

typedef pair<UString::Rep*, unsigned> NameCountPair;

static inline bool functionNameCountPairComparator(const NameCountPair& a, const NameCountPair& b)
{
    return a.second > b.second;
}

void Profile::debugPrintDataSampleStyle() const
{
    typedef Vector<NameCountPair> NameCountPairVector;

    FunctionCallHashCount countedFunctions;
    printf("Call graph:\n");
    m_callTree->debugPrintDataSampleStyle(0, countedFunctions);

    printf("\nTotal number in stack:\n");
    NameCountPairVector sortedFunctions(countedFunctions.size());
    copyToVector(countedFunctions, sortedFunctions);

    std::sort(sortedFunctions.begin(), sortedFunctions.end(), functionNameCountPairComparator);
    for (NameCountPairVector::iterator it = sortedFunctions.begin(); it != sortedFunctions.end(); ++it)
        printf("        %-12d%s\n", (*it).second, UString((*it).first).UTF8String().c_str());

    printf("\nSort by top of stack, same collapsed (when >= 5):\n");
}
#endif

}   // namespace KJS
