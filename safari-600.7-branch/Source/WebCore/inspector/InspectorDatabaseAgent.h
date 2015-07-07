/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#ifndef InspectorDatabaseAgent_h
#define InspectorDatabaseAgent_h

#if ENABLE(INSPECTOR) && ENABLE(SQL_DATABASE)

#include "InspectorWebAgentBase.h"
#include "InspectorWebBackendDispatchers.h"
#include "InspectorWebFrontendDispatchers.h"
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

namespace Inspector {
class InspectorArray;
}

namespace WebCore {

class Database;
class InspectorDatabaseResource;
class InstrumentingAgents;

typedef String ErrorString;

class InspectorDatabaseAgent : public InspectorAgentBase, public Inspector::InspectorDatabaseBackendDispatcherHandler {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit InspectorDatabaseAgent(InstrumentingAgents*);
    ~InspectorDatabaseAgent();

    virtual void didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel*, Inspector::InspectorBackendDispatcher*) override;
    virtual void willDestroyFrontendAndBackend(Inspector::InspectorDisconnectReason) override;

    void clearResources();

    // Called from the front-end.
    virtual void enable(ErrorString*) override;
    virtual void disable(ErrorString*) override;
    virtual void getDatabaseTableNames(ErrorString*, const String& databaseId, RefPtr<Inspector::TypeBuilder::Array<String>>& names) override;
    virtual void executeSQL(ErrorString*, const String& databaseId, const String& query, PassRefPtr<ExecuteSQLCallback>) override;

    // Called from the injected script.
    String databaseId(Database*);

    void didOpenDatabase(PassRefPtr<Database>, const String& domain, const String& name, const String& version);
private:
    Database* databaseForId(const String& databaseId);
    InspectorDatabaseResource* findByFileName(const String& fileName);

    std::unique_ptr<Inspector::InspectorDatabaseFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::InspectorDatabaseBackendDispatcher> m_backendDispatcher;
    typedef HashMap<String, RefPtr<InspectorDatabaseResource>> DatabaseResourcesMap;
    DatabaseResourcesMap m_resources;
    bool m_enabled;
};

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(SQL_DATABASE)

#endif // !defined(InspectorDatabaseAgent_h)
