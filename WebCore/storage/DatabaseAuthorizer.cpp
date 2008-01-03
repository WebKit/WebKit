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
#include "DatabaseAuthorizer.h"

#include "Database.h"
#include "PlatformString.h"

namespace WebCore {

DatabaseAuthorizer::DatabaseAuthorizer()
    : m_securityEnabled(false)
{
    reset();
}

void DatabaseAuthorizer::reset()
{
    m_lastActionWasInsert = false;
    m_lastActionChangedDatabase = false;
}

int DatabaseAuthorizer::createTable(const String& tableName)
{
    m_lastActionChangedDatabase = true;
    return denyBasedOnTableName(tableName);
}

int DatabaseAuthorizer::createTempTable(const String& tableName)
{
    return denyBasedOnTableName(tableName);
}

int DatabaseAuthorizer::dropTable(const String& tableName)
{
    return denyBasedOnTableName(tableName);
}

int DatabaseAuthorizer::dropTempTable(const String& tableName)
{
    return denyBasedOnTableName(tableName);
}

int DatabaseAuthorizer::allowAlterTable(const String& databaseName, const String& tableName)
{
    m_lastActionChangedDatabase = true;
    return denyBasedOnTableName(tableName);
}

int DatabaseAuthorizer::createIndex(const String& indexName, const String& tableName)
{
    m_lastActionChangedDatabase = true;
    return denyBasedOnTableName(tableName);
}

int DatabaseAuthorizer::createTempIndex(const String& indexName, const String& tableName)
{
    return denyBasedOnTableName(tableName);
}

int DatabaseAuthorizer::dropIndex(const String& indexName, const String& tableName)
{
    return denyBasedOnTableName(tableName);
}

int DatabaseAuthorizer::dropTempIndex(const String& indexName, const String& tableName)
{
    return denyBasedOnTableName(tableName);
}

int DatabaseAuthorizer::createTrigger(const String& triggerName, const String& tableName)
{
    m_lastActionChangedDatabase = true;
    return denyBasedOnTableName(tableName);
}

int DatabaseAuthorizer::createTempTrigger(const String& triggerName, const String& tableName)
{
    return denyBasedOnTableName(tableName);
}

int DatabaseAuthorizer::dropTrigger(const String& triggerName, const String& tableName)
{
    return denyBasedOnTableName(tableName);
}

int DatabaseAuthorizer::dropTempTrigger(const String& triggerName, const String& tableName)
{
    return denyBasedOnTableName(tableName);
}

int DatabaseAuthorizer::createVTable(const String& tableName, const String& moduleName)
{
    m_lastActionChangedDatabase = true;
    return m_securityEnabled ? SQLAuthDeny : SQLAuthAllow;
}

int DatabaseAuthorizer::dropVTable(const String& tableName, const String& moduleName)
{
    return m_securityEnabled ? SQLAuthDeny : SQLAuthAllow;
}

int DatabaseAuthorizer::allowDelete(const String& tableName)
{
    return denyBasedOnTableName(tableName);
}

int DatabaseAuthorizer::allowInsert(const String& tableName)
{
    m_lastActionChangedDatabase = true;
    m_lastActionWasInsert = true;
    return denyBasedOnTableName(tableName);
}

int DatabaseAuthorizer::allowUpdate(const String& tableName, const String& columnName)
{
    m_lastActionChangedDatabase = true;
    return denyBasedOnTableName(tableName);
}

int DatabaseAuthorizer::allowTransaction()
{
    return m_securityEnabled ? SQLAuthDeny : SQLAuthAllow;
}

int DatabaseAuthorizer::allowRead(const String& tableName, const String& columnName)
{
    return denyBasedOnTableName(tableName);
}

int DatabaseAuthorizer::allowAnalyze(const String& tableName)
{
    return denyBasedOnTableName(tableName);
}

int DatabaseAuthorizer::allowPragma(const String& pragmaName, const String& firstArgument)
{
    return m_securityEnabled ? SQLAuthDeny : SQLAuthAllow;
}

int DatabaseAuthorizer::allowAttach(const String& filename)
{
    return m_securityEnabled ? SQLAuthDeny : SQLAuthAllow;
}

int DatabaseAuthorizer::allowDetach(const String& databaseName)
{
    return m_securityEnabled ? SQLAuthDeny : SQLAuthAllow;
}

int DatabaseAuthorizer::allowFunction(const String& functionName)
{
    // FIXME: Are there any of these we need to prevent?  One might guess current_date, current_time, current_timestamp because
    // they would violate the "sandbox environment" part of 4.11.3, but scripts can generate the local client side information via
    // javascript directly, anyways.  Are there any other built-ins we need to be worried about?
    return SQLAuthAllow;
}

void DatabaseAuthorizer::disable()
{
    m_securityEnabled = false;
}

void DatabaseAuthorizer::enable()
{
    m_securityEnabled = true;
}

int DatabaseAuthorizer::denyBasedOnTableName(const String& tableName)
{
    if (!m_securityEnabled)
        return SQLAuthAllow;

    // Sadly, normal creates and drops end up affecting sqlite_master in an authorizer callback, so
    // it will be tough to enforce all of the following policies
    //if (equalIgnoringCase(tableName, "sqlite_master") || equalIgnoringCase(tableName, "sqlite_temp_master") ||
    //    equalIgnoringCase(tableName, "sqlite_sequence") || equalIgnoringCase(tableName, Database::databaseInfoTableName()))
    //        return SQLAuthDeny;

    if (equalIgnoringCase(tableName, Database::databaseInfoTableName()))
        return SQLAuthDeny;

    return SQLAuthAllow;
}

} // namespace WebCore
