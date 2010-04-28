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
#include "DatabaseSync.h"

#if ENABLE(DATABASE)
#include "DatabaseCallback.h"
#include "ExceptionCode.h"
#include "SQLTransactionSyncCallback.h"
#include "ScriptExecutionContext.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

const String& DatabaseSync::databaseInfoTableName()
{
    DEFINE_STATIC_LOCAL(String, name, ("__WebKitDatabaseInfoTable__"));
    return name;
}

static bool isSyncDatabaseAvailable = true;

void DatabaseSync::setIsAvailable(bool available)
{
    isSyncDatabaseAvailable = available;
}

bool DatabaseSync::isAvailable()
{
    return isSyncDatabaseAvailable;
}

PassRefPtr<DatabaseSync> DatabaseSync::openDatabaseSync(ScriptExecutionContext*, const String&, const String&, const String&,
                                                        unsigned long, PassRefPtr<DatabaseCallback>, ExceptionCode& ec)
{
    // FIXME: uncomment the assert once we use the ScriptExecutionContext* parameter
    //ASSERT(context->isContextThread());

    ec = SECURITY_ERR;
    return 0;
}

DatabaseSync::DatabaseSync(ScriptExecutionContext* context, const String& name, const String& expectedVersion,
                           const String& displayName, unsigned long estimatedSize, PassRefPtr<DatabaseCallback> creationCallback)
    : m_scriptExecutionContext(context)
    , m_name(name.crossThreadString())
    , m_expectedVersion(expectedVersion.crossThreadString())
    , m_displayName(displayName.crossThreadString())
    , m_estimatedSize(estimatedSize)
    , m_creationCallback(creationCallback)
{
    ASSERT(context->isContextThread());
}

DatabaseSync::~DatabaseSync()
{
    ASSERT(m_scriptExecutionContext->isContextThread());
}

String DatabaseSync::version() const
{
    ASSERT(m_scriptExecutionContext->isContextThread());
    return String();
}

void DatabaseSync::changeVersion(const String&, const String&, PassRefPtr<SQLTransactionSyncCallback>)
{
    ASSERT(m_scriptExecutionContext->isContextThread());
}

void DatabaseSync::transaction(PassRefPtr<SQLTransactionSyncCallback>, bool)
{
    ASSERT(m_scriptExecutionContext->isContextThread());
}

ScriptExecutionContext* DatabaseSync::scriptExecutionContext() const
{
    ASSERT(m_scriptExecutionContext->isContextThread());
    return m_scriptExecutionContext.get();
}

#endif // ENABLE(DATABASE)

} // namespace WebCore
