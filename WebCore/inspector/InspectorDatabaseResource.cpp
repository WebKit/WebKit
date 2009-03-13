/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#if ENABLE(DATABASE)
#include "InspectorDatabaseResource.h"

#include "Database.h"
#include "Document.h"
#include "Frame.h"
#include "ScriptFunctionCall.h"
#include "ScriptObjectQuarantine.h"
#include "ScriptValue.h"

namespace WebCore {

InspectorDatabaseResource::InspectorDatabaseResource(Database* database, const String& domain, const String& name, const String& version)
    : m_database(database)
    , m_domain(domain)
    , m_name(name)
    , m_version(version)
{
}

void InspectorDatabaseResource::bind(ScriptState* scriptState, const ScriptObject& webInspector)
{
    if (!m_scriptObject.hasNoValue())
        return;

    ASSERT(scriptState);
    ASSERT(!webInspector.hasNoValue());
    if (!scriptState || webInspector.hasNoValue())
        return;

    ScriptFunctionCall resourceConstructor(scriptState, webInspector, "Database");
    ScriptObject database;
    if (!getQuarantinedScriptObject(m_database.get(), database))
        return;

    resourceConstructor.appendArgument(database);
    resourceConstructor.appendArgument(m_domain);
    resourceConstructor.appendArgument(m_name);
    resourceConstructor.appendArgument(m_version);

    bool hadException = false;
    m_scriptObject = resourceConstructor.construct(hadException);
    if (hadException)
        return;

    ScriptFunctionCall addDatabase(scriptState, webInspector, "addDatabase");
    addDatabase.appendArgument(m_scriptObject);
    addDatabase.call(hadException);
}

void InspectorDatabaseResource::unbind()
{
    m_scriptObject = ScriptObject();
}

} // namespace WebCore

#endif // ENABLE(DATABASE)
