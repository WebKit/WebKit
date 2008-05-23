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
static const char* GlobalCodeExecution = "(program)";
static const char* AnonymousFunction = "(anonymous function)";

static CallIdentifier createCallIdentifier(JSObject*);
static CallIdentifier createCallIdentifier(const UString& sourceURL, int startingLineNumber);
static CallIdentifier createCallIdentifierFromFunctionImp(FunctionImp*);

Profiler* Profiler::profiler()
{
    if (!sharedProfiler)
        sharedProfiler = new Profiler;
    return sharedProfiler;
}

Profile* Profiler::findProfile(ExecState* exec, const UString& title) const
{
    ExecState* globalExec = exec->lexicalGlobalObject()->globalExec();
    for (size_t i = 0; i < m_currentProfiles.size(); ++i)
        if (m_currentProfiles[i]->originatingGlobalExec() == globalExec && (title.isNull() || m_currentProfiles[i]->title() == title))
            return m_currentProfiles[i].get();
    return 0;
}

void Profiler::startProfiling(ExecState* exec, const UString& title)
{
    ASSERT_ARG(exec, exec);

    // Check if we currently have a Profile for this global ExecState and title.
    // If so return early and don't create a new Profile.
    ExecState* globalExec = exec->lexicalGlobalObject()->globalExec();
    for (size_t i = 0; i < m_currentProfiles.size(); ++i)
        if (m_currentProfiles[i]->originatingGlobalExec() == globalExec && m_currentProfiles[i]->title() == title)
            return;

    RefPtr<Profile> profile = Profile::create(title, globalExec, exec->lexicalGlobalObject()->pageGroupIdentifier());
    m_currentProfiles.append(profile);
}

PassRefPtr<Profile> Profiler::stopProfiling(ExecState* exec, const UString& title)
{
    ExecState* globalExec = exec->lexicalGlobalObject()->globalExec();
    for (ptrdiff_t i = m_currentProfiles.size() - 1; i >= 0; --i) {
        if (m_currentProfiles[i]->originatingGlobalExec() == globalExec && (title.isNull() || m_currentProfiles[i]->title() == title)) {
            m_currentProfiles[i]->stopProfiling();

            PassRefPtr<Profile> prpProfile = m_currentProfiles[i].release();
            m_currentProfiles.remove(i);
            return prpProfile;
        }
    }

    return 0;
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

static inline void dispatchFunctionToProfiles(const Vector<RefPtr<Profile> >& profiles, Profile::ProfileFunction function, const CallIdentifier& callIdentifier, unsigned currentPageGroupIdentifier)
{
    for (size_t i = 0; i < profiles.size(); ++i)
        if (profiles[i]->pageGroupIdentifier() == currentPageGroupIdentifier)
            (profiles[i].get()->*function)(callIdentifier);
}

void Profiler::willExecute(ExecState* exec, JSObject* calledFunction)
{
    if (m_currentProfiles.isEmpty())
        return;

    if (shouldExcludeFunction(exec, calledFunction))
        return;

    dispatchFunctionToProfiles(m_currentProfiles, &Profile::willExecute, createCallIdentifier(calledFunction), exec->lexicalGlobalObject()->pageGroupIdentifier());
}

void Profiler::willExecute(ExecState* exec, const UString& sourceURL, int startingLineNumber)
{
    if (m_currentProfiles.isEmpty())
        return;

    CallIdentifier callIdentifier = createCallIdentifier(sourceURL, startingLineNumber);

    dispatchFunctionToProfiles(m_currentProfiles, &Profile::willExecute, callIdentifier, exec->lexicalGlobalObject()->pageGroupIdentifier());
}

void Profiler::didExecute(ExecState* exec, JSObject* calledFunction)
{
    if (m_currentProfiles.isEmpty())
        return;

    if (shouldExcludeFunction(exec, calledFunction))
        return;

    dispatchFunctionToProfiles(m_currentProfiles, &Profile::didExecute, createCallIdentifier(calledFunction), exec->lexicalGlobalObject()->pageGroupIdentifier());
}

void Profiler::didExecute(ExecState* exec, const UString& sourceURL, int startingLineNumber)
{
    if (m_currentProfiles.isEmpty())
        return;

    dispatchFunctionToProfiles(m_currentProfiles, &Profile::didExecute, createCallIdentifier(sourceURL, startingLineNumber), exec->lexicalGlobalObject()->pageGroupIdentifier());
}

CallIdentifier createCallIdentifier(JSObject* calledFunction)
{
    if (calledFunction->inherits(&FunctionImp::info))
        return createCallIdentifierFromFunctionImp(static_cast<FunctionImp*>(calledFunction));
    if (calledFunction->inherits(&InternalFunctionImp::info))
        return CallIdentifier(static_cast<InternalFunctionImp*>(calledFunction)->functionName().ustring(), "", 0);

    UString name = "(";
    name += calledFunction->classInfo()->className;
    name += " object)";
    return CallIdentifier(name, 0, 0);
}

CallIdentifier createCallIdentifier(const UString& sourceURL, int startingLineNumber)
{
    return CallIdentifier(GlobalCodeExecution, sourceURL, (startingLineNumber + 1));
}

CallIdentifier createCallIdentifierFromFunctionImp(FunctionImp* functionImp)
{
    UString name = functionImp->functionName().ustring();
    if (name.isEmpty())
        name = AnonymousFunction;

    return CallIdentifier(name, functionImp->body->sourceURL(), functionImp->body->lineNo());
}

}   // namespace KJS
