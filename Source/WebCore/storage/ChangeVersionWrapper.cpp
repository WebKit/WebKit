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
#include "config.h"
#include "ChangeVersionWrapper.h"

#if ENABLE(SQL_DATABASE)

#include "Database.h"
#include "SQLError.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

ChangeVersionWrapper::ChangeVersionWrapper(const String& oldVersion, const String& newVersion)
    : m_oldVersion(oldVersion.isolatedCopy())
    , m_newVersion(newVersion.isolatedCopy())
{
}

bool ChangeVersionWrapper::performPreflight(SQLTransaction* transaction)
{
    ASSERT(transaction && transaction->database());

    Database* database = transaction->database();

    String actualVersion;
    if (!database->getVersionFromDatabase(actualVersion)) {
        int sqliteError = database->sqliteDatabase().lastError();
        database->reportChangeVersionResult(1, SQLError::UNKNOWN_ERR, sqliteError);
        m_sqlError = SQLError::create(SQLError::UNKNOWN_ERR, "unable to read the current version",
                                      sqliteError, database->sqliteDatabase().lastErrorMsg());
        return false;
    }

    if (actualVersion != m_oldVersion) {
        database->reportChangeVersionResult(2, SQLError::VERSION_ERR, 0);
        m_sqlError = SQLError::create(SQLError::VERSION_ERR, "current version of the database and `oldVersion` argument do not match");
        return false;
    }

    return true;
}

bool ChangeVersionWrapper::performPostflight(SQLTransaction* transaction)
{
    ASSERT(transaction && transaction->database());

    Database* database = transaction->database();

    if (!database->setVersionInDatabase(m_newVersion)) {
        int sqliteError = database->sqliteDatabase().lastError();
        database->reportChangeVersionResult(3, SQLError::UNKNOWN_ERR, sqliteError);
        m_sqlError = SQLError::create(SQLError::UNKNOWN_ERR, "unable to set new version in database",
                                      sqliteError, database->sqliteDatabase().lastErrorMsg());
        return false;
    }

    database->setExpectedVersion(m_newVersion);

    database->reportChangeVersionResult(0, -1, 0); // OK
    return true;
}

void ChangeVersionWrapper::handleCommitFailedAfterPostflight(SQLTransaction* transaction)
{
    transaction->database()->setCachedVersion(m_oldVersion);
}

} // namespace WebCore

#endif // ENABLE(SQL_DATABASE)
