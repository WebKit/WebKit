/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#ifndef InspectorBackend_h
#define InspectorBackend_h

#include "Console.h"
#include "InspectorController.h"
#include "PlatformString.h"

#include <wtf/RefCounted.h>

namespace WebCore {

class InspectorApplicationCacheAgent;
class InspectorDOMAgent;
class InspectorFrontend;

class InspectorBackend : public RefCounted<InspectorBackend>
{
public:
    static PassRefPtr<InspectorBackend> create(InspectorController* inspectorController)
    {
        return adoptRef(new InspectorBackend(inspectorController));
    }

    ~InspectorBackend();

    InspectorController* inspectorController() { return m_inspectorController; }
    InspectorDOMAgent* inspectorDOMAgent() { return m_inspectorController->domAgent(); }
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    InspectorApplicationCacheAgent* inspectorApplicationCacheAgent() { return m_inspectorController->applicationCacheAgent(); }
#endif
    void disconnectController() { m_inspectorController = 0; }

#if ENABLE(JAVASCRIPT_DEBUGGER)
    void enableDebugger(bool always);
#endif

    void setInjectedScriptSource(const String& source);
    void dispatchOnInjectedScript(long injectedScriptId, const String& methodName, const String& arguments, RefPtr<InspectorValue>* result, bool* hadException);

    // Generic code called from custom implementations.
    void releaseWrapperObjectGroup(long injectedScriptId, const String& objectGroup);

#if ENABLE(DATABASE)
    void getDatabaseTableNames(long databaseId, RefPtr<InspectorArray>* names);
    void executeSQL(long databaseId, const String& query, bool* success, long* transactionId);
#endif

private:
    InspectorBackend(InspectorController* inspectorController);
    InspectorFrontend* frontend();

    InspectorController* m_inspectorController;
};

} // namespace WebCore

#endif // !defined(InspectorBackend_h)
