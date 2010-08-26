/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
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
#include "InspectorBackend.h"

#if ENABLE(INSPECTOR)

#if ENABLE(DATABASE)
#include "Database.h"
#endif

#include "InjectedScript.h"
#include "InjectedScriptHost.h"
#include "InspectorController.h"
#include "InspectorDOMAgent.h"
#include "InspectorFrontend.h"
#include "InspectorStorageAgent.h"
#include "ScriptBreakpoint.h"
#include "ScriptProfiler.h"
#include "SerializedScriptValue.h"

using namespace std;

namespace WebCore {

InspectorBackend::InspectorBackend(InspectorController* inspectorController)
    : m_inspectorController(inspectorController)
{
}

InspectorBackend::~InspectorBackend()
{
}


#if ENABLE(JAVASCRIPT_DEBUGGER)

void InspectorBackend::enableDebugger(bool always)
{
    if (m_inspectorController)
        m_inspectorController->enableDebuggerFromFrontend(always);
}

#endif

void InspectorBackend::setInjectedScriptSource(const String& source)
{
     m_inspectorController->injectedScriptHost()->setInjectedScriptSource(source);
}

void InspectorBackend::dispatchOnInjectedScript(long injectedScriptId, const String& methodName, const String& arguments, RefPtr<InspectorValue>* result, bool* hadException)
{
    if (!frontend())
        return;

    // FIXME: explicitly pass injectedScriptId along with node id to the frontend.
    bool injectedScriptIdIsNodeId = injectedScriptId <= 0;

    InjectedScript injectedScript;
    if (injectedScriptIdIsNodeId)
        injectedScript = m_inspectorController->injectedScriptForNodeId(-injectedScriptId);
    else
        injectedScript = m_inspectorController->injectedScriptHost()->injectedScriptForId(injectedScriptId);

    if (injectedScript.hasNoValue())
        return;

    injectedScript.dispatch(methodName, arguments, result, hadException);
}

void InspectorBackend::releaseWrapperObjectGroup(long injectedScriptId, const String& objectGroup)
{
    m_inspectorController->injectedScriptHost()->releaseWrapperObjectGroup(injectedScriptId, objectGroup);
}

#if ENABLE(DATABASE)
void InspectorBackend::getDatabaseTableNames(long databaseId, RefPtr<InspectorArray>* names)
{
    Database* database = m_inspectorController->databaseForId(databaseId);
    if (database) {
        Vector<String> tableNames = database->tableNames();
        unsigned length = tableNames.size();
        for (unsigned i = 0; i < length; ++i)
            (*names)->pushString(tableNames[i]);
    }
}

void InspectorBackend::executeSQL(long databaseId, const String& query, bool* success, long* transactionId)
{
    Database* database = m_inspectorController->databaseForId(databaseId);
    if (!m_inspectorController->m_storageAgent || !database) {
        *success = false;
        return;
    }

    *transactionId = m_inspectorController->m_storageAgent->executeSQL(database, query);
    *success = true;
}

#endif

InspectorFrontend* InspectorBackend::frontend()
{
    return m_inspectorController->m_frontend.get();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
