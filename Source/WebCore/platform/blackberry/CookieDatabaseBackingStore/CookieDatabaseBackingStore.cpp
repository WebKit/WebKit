/*
 * Copyright (C) 2009 Julien Chaffraix <jchaffraix@pleyo.com>
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#define ENABLE_COOKIE_DEBUG 0

#include "config.h"
#include "CookieDatabaseBackingStore.h"

#include "CookieManager.h"
#include "Logging.h"
#include "ParsedCookie.h"
#include "SQLiteStatement.h"
#include "SQLiteTransaction.h"
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

#if ENABLE_COOKIE_DEBUG
#include <BlackBerryPlatformLog.h>
#define CookieLog(format, ...) BlackBerry::Platform::logAlways(BlackBerry::Platform::LogLevelInfo, format, ## __VA_ARGS__)
#else
#define CookieLog(format, ...)
#endif

#include <BlackBerryPlatformExecutableMessage.h>

using BlackBerry::Platform::MessageClient;
using BlackBerry::Platform::TypedReplyBuffer;
using BlackBerry::Platform::createMethodCallMessage;

static const double s_databaseTimerInterval = 2;

namespace WebCore {

CookieDatabaseBackingStore::CookieDatabaseBackingStore()
    : MessageClient(MessageClient::ReplyFeature | MessageClient::SyncFeature)
    , m_tableName("cookies") // This is chosen to match Mozilla's table name.
    , m_dbTimer(this, &CookieDatabaseBackingStore::sendChangesToDatabaseTimerFired)
    , m_insertStatement(0)
    , m_updateStatement(0)
    , m_deleteStatement(0)
{
    m_dbTimerClient = new BlackBerry::Platform::GenericTimerClient(this);
    m_dbTimer.setClient(m_dbTimerClient);

    pthread_attr_t threadAttrs;
    pthread_attr_init(&threadAttrs);
    createThread("cookie_database", threadAttrs);
}

CookieDatabaseBackingStore::~CookieDatabaseBackingStore()
{
    deleteGuardedObject(m_dbTimerClient);
    m_dbTimerClient = 0;
    // FIXME: This object will never be deleted due to the set up of CookieManager (it's a singleton)
    CookieLog("CookieBackingStore - Destructing");
#ifndef NDEBUG
    {
        MutexLocker lock(m_mutex);
        ASSERT(m_changedCookies.isEmpty());
    }
#endif
}

void CookieDatabaseBackingStore::upgradeTableIfNeeded(const String& databaseFields, const String& primaryKeyFields)
{
    ASSERT(isCurrentThread());

    bool creationTimeExists = false;
    bool protocolExists = false;

    if (!m_db.tableExists(m_tableName))
        return;

    // Check if the existing table has the required database fields
    {
        String query = "PRAGMA table_info(" + m_tableName + ");";

        SQLiteStatement statement(m_db, query);
        if (statement.prepare()) {
            LOG_ERROR("Cannot prepare statement to query cookie table info. sql:%s", query.utf8().data());
            LOG_ERROR("SQLite Error Message: %s", m_db.lastErrorMsg());
            return;
        }

        while (statement.step() == SQLResultRow) {
            DEFINE_STATIC_LOCAL(String, creationTime, ("creationTime"));
            DEFINE_STATIC_LOCAL(String, protocol, ("protocol"));
            String name = statement.getColumnText(1);
            if (name == creationTime)
                creationTimeExists = true;
            if (name == protocol)
                protocolExists = true;
            if (creationTimeExists && protocolExists)
                return;
        }
        LOG(Network, "Need to update cookie table schema.");
    }

    // Drop and recreate the cookie table to update to the latest database fields.
    // We do not use alter table - add column because that method cannot add primary keys.
    Vector<String> commands;

    // Backup existing table
    String renameQuery = "ALTER TABLE " + m_tableName + " RENAME TO Backup_" + m_tableName + ";";
    commands.append(renameQuery);

    // Recreate the cookie table using the new database and primary key fields
    String createTableQuery("CREATE TABLE ");
    createTableQuery += m_tableName;
    createTableQuery += " (" + databaseFields + ", " + primaryKeyFields + ");";
    commands.append(createTableQuery);

    // Copy the old data into the new table. If a column does not exists,
    // we have to put a '' in the select statement to make the number of columns
    // equal in the insert statement.
    String migrationQuery("INSERT OR REPLACE INTO ");
    migrationQuery += m_tableName;
    migrationQuery += " SELECT *";
    if (!creationTimeExists)
        migrationQuery += ",''";
    if (!protocolExists)
        migrationQuery += ",''";
    migrationQuery += " FROM Backup_" + m_tableName;
    commands.append(migrationQuery);

    // The new columns will be blank, set the new values.
    if (!creationTimeExists) {
        String setCreationTimeQuery = "UPDATE " + m_tableName + " SET creationTime = lastAccessed;";
        commands.append(setCreationTimeQuery);
    }

    if (!protocolExists) {
        String setProtocolQuery = "UPDATE " + m_tableName + " SET protocol = 'http' WHERE isSecure = '0';";
        String setProtocolQuery2 = "UPDATE " + m_tableName + " SET protocol = 'https' WHERE isSecure = '1';";
        commands.append(setProtocolQuery);
        commands.append(setProtocolQuery2);
    }

    // Drop the backup table
    String dropBackupQuery = "DROP TABLE IF EXISTS Backup_" + m_tableName + ";";
    commands.append(dropBackupQuery);

    SQLiteTransaction transaction(m_db, false);
    transaction.begin();
    size_t commandSize = commands.size();
    for (size_t i = 0; i < commandSize; ++i) {
        if (!m_db.executeCommand(commands[i])) {
            LOG_ERROR("Failed to alter cookie table when executing sql:%s", commands[i].utf8().data());
            LOG_ERROR("SQLite Error Message: %s", m_db.lastErrorMsg());
            transaction.rollback();

            // We should never get here, but if we do, rename the current cookie table for future restoration. This has the side effect of
            // clearing the current cookie table, but that's better than continually hitting this case and hence never being able to use the
            // cookie table.
            ASSERT_NOT_REACHED();
            String renameQuery = "ALTER TABLE " + m_tableName + " RENAME TO Backup2_" + m_tableName + ";";
            if (!m_db.executeCommand(renameQuery)) {
                LOG_ERROR("Failed to backup existing cookie table.");
                LOG_ERROR("SQLite Error Message: %s", m_db.lastErrorMsg());
            }
            return;
        }
    }
    transaction.commit();
    LOG(Network, "Successfully updated cookie table schema.");
}

void CookieDatabaseBackingStore::open(const String& cookieJar)
{
    dispatchMessage(createMethodCallMessage(&CookieDatabaseBackingStore::invokeOpen, this, cookieJar));
}

void CookieDatabaseBackingStore::invokeOpen(const String& cookieJar)
{
    ASSERT(isCurrentThread());
    if (m_db.isOpen())
        close();

    if (!m_db.open(cookieJar)) {
        LOG_ERROR("Could not open the cookie database. No cookie will be stored!");
        LOG_ERROR("SQLite Error Message: %s", m_db.lastErrorMsg());
        return;
    }

    m_db.executeCommand("PRAGMA locking_mode=EXCLUSIVE;");
    m_db.executeCommand("PRAGMA journal_mode=TRUNCATE;");

    const String primaryKeyFields("PRIMARY KEY (protocol, host, path, name)");
    const String databaseFields("name TEXT, value TEXT, host TEXT, path TEXT, expiry DOUBLE, lastAccessed DOUBLE, isSecure INTEGER, isHttpOnly INTEGER, creationTime DOUBLE, protocol TEXT");
    // Update table to add the new column creationTime and protocol for backwards compatability.
    upgradeTableIfNeeded(databaseFields, primaryKeyFields);

    // Create table if not exsist in case that the upgradeTableIfNeeded() failed accidentally.
    String createTableQuery("CREATE TABLE IF NOT EXISTS ");
    createTableQuery += m_tableName;
    // This table schema is compliant with Mozilla's.
    createTableQuery += " (" + databaseFields + ", " + primaryKeyFields+");";

    if (!m_db.executeCommand(createTableQuery)) {
        LOG_ERROR("Could not create the table to store the cookies into. No cookie will be stored!");
        LOG_ERROR("SQLite Error Message: %s", m_db.lastErrorMsg());
        close();
        return;
    }

    String insertQuery("INSERT OR REPLACE INTO ");
    insertQuery += m_tableName;
    insertQuery += " (name, value, host, path, expiry, lastAccessed, isSecure, isHttpOnly, creationTime, protocol) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10);";

    m_insertStatement = new SQLiteStatement(m_db, insertQuery);
    if (m_insertStatement->prepare()) {
        LOG_ERROR("Cannot save cookies");
        LOG_ERROR("SQLite Error Message: %s", m_db.lastErrorMsg());
    }

    String updateQuery("UPDATE ");
    updateQuery += m_tableName;
    // The where statement is chosen to match CookieMap key.
    updateQuery += " SET name = ?1, value = ?2, host = ?3, path = ?4, expiry = ?5, lastAccessed = ?6, isSecure = ?7, isHttpOnly = ?8, creationTime = ?9, protocol = ?10 where name = ?1 and host = ?3 and path = ?4;";
    m_updateStatement = new SQLiteStatement(m_db, updateQuery);

    if (m_updateStatement->prepare()) {
        LOG_ERROR("Cannot update cookies");
        LOG_ERROR("SQLite Error Message: %s", m_db.lastErrorMsg());
    }

    String deleteQuery("DELETE FROM ");
    deleteQuery += m_tableName;
    // The where statement is chosen to match CookieMap key.
    deleteQuery += " WHERE name=?1 and host=?2 and path=?3 and protocol=?4;";
    m_deleteStatement = new SQLiteStatement(m_db, deleteQuery);

    if (m_deleteStatement->prepare()) {
        LOG_ERROR("Cannot delete cookies");
        LOG_ERROR("SQLite Error Message: %s", m_db.lastErrorMsg());
    }

}

void CookieDatabaseBackingStore::close()
{
    ASSERT(isCurrentThread());
    CookieLog("CookieBackingStore - Closing");

    size_t changedCookiesSize;
    {
        MutexLocker lock(m_mutex);
        if (m_dbTimer.started())
            m_dbTimer.stop();
        changedCookiesSize = m_changedCookies.size();
    }

    if (changedCookiesSize > 0)
        invokeSendChangesToDatabase();

    delete m_insertStatement;
    m_insertStatement = 0;
    delete m_updateStatement;
    m_updateStatement = 0;
    delete m_deleteStatement;
    m_deleteStatement = 0;

    if (m_db.isOpen())
        m_db.close();
}

void CookieDatabaseBackingStore::insert(const ParsedCookie* cookie)
{
    CookieLog("CookieBackingStore - adding inserting cookie %s to queue.", cookie->toString().utf8().data());
    addToChangeQueue(cookie, Insert);
}

void CookieDatabaseBackingStore::update(const ParsedCookie* cookie)
{
    CookieLog("CookieBackingStore - adding updating cookie %s to queue.", cookie->toString().utf8().data());
    addToChangeQueue(cookie, Update);
}

void CookieDatabaseBackingStore::remove(const ParsedCookie* cookie)
{
    CookieLog("CookieBackingStore - adding deleting cookie %s to queue.", cookie->toString().utf8().data());
    addToChangeQueue(cookie, Delete);
}

void CookieDatabaseBackingStore::removeAll()
{
    dispatchMessage(createMethodCallMessage(&CookieDatabaseBackingStore::invokeRemoveAll, this));
}

void CookieDatabaseBackingStore::invokeRemoveAll()
{
    ASSERT(isCurrentThread());
    if (!m_db.isOpen())
        return;

    CookieLog("CookieBackingStore - remove All cookies from backingstore");

    {
        MutexLocker lock(m_mutex);
        m_changedCookies.clear();
    }

    String deleteQuery("DELETE FROM ");
    deleteQuery += m_tableName;
    deleteQuery += ";";

    SQLiteStatement deleteStatement(m_db, deleteQuery);
    if (deleteStatement.prepare()) {
        LOG_ERROR("Could not prepare DELETE * statement");
        LOG_ERROR("SQLite Error Message: %s", m_db.lastErrorMsg());
        return;
    }

    if (!deleteStatement.executeCommand()) {
        LOG_ERROR("Cannot delete cookie from database");
        LOG_ERROR("SQLite Error Message: %s", m_db.lastErrorMsg());
        return;
    }
}

void CookieDatabaseBackingStore::getCookiesFromDatabase(Vector<ParsedCookie*>& stackOfCookies, unsigned int limit)
{
    // It is not a huge performance hit to wait on the reply here because this is only done once during setup and when turning off private mode.
    TypedReplyBuffer< Vector<ParsedCookie*>* > replyBuffer(0);
    dispatchMessage(createMethodCallMessageWithReturn(&CookieDatabaseBackingStore::invokeGetCookiesWithLimit, &replyBuffer, this, limit));
    Vector<ParsedCookie*>* cookies = replyBuffer.pointer();
    stackOfCookies.swap(*cookies);
    delete cookies;
}

Vector<ParsedCookie*>* CookieDatabaseBackingStore::invokeGetCookiesWithLimit(unsigned int limit)
{
    ASSERT(isCurrentThread());

    // Check that the table exists to avoid doing an unnecessary request.
    if (!m_db.isOpen())
        return 0;

    StringBuilder selectQuery;
    selectQuery.append("SELECT name, value, host, path, expiry, lastAccessed, isSecure, isHttpOnly, creationTime, protocol FROM ");
    selectQuery.append(m_tableName);
    if (limit > 0) {
        selectQuery.append(" ORDER BY lastAccessed ASC");
        selectQuery.append(" LIMIT " + String::number(limit));
    }
    selectQuery.append(";");

    CookieLog("CookieBackingStore - invokeGetAllCookies with select query %s", selectQuery.toString().utf8().data());

    SQLiteStatement selectStatement(m_db, selectQuery.toString());

    if (selectStatement.prepare()) {
        LOG_ERROR("Cannot retrieved cookies from the database");
        LOG_ERROR("SQLite Error Message: %s", m_db.lastErrorMsg());
        return 0;
    }

    Vector<ParsedCookie*>* cookies = new Vector<ParsedCookie*>;
    while (selectStatement.step() == SQLResultRow) {
        // There is a row to fetch

        String name = selectStatement.getColumnText(0);
        String value = selectStatement.getColumnText(1);
        String domain = selectStatement.getColumnText(2);
        String path = selectStatement.getColumnText(3);
        double expiry = selectStatement.getColumnDouble(4);
        double lastAccessed = selectStatement.getColumnDouble(5);
        bool isSecure = selectStatement.getColumnInt(6);
        bool isHttpOnly = selectStatement.getColumnInt(7);
        double creationTime = selectStatement.getColumnDouble(8);
        String protocol = selectStatement.getColumnText(9);

        cookies->append(new ParsedCookie(name, value, domain, protocol, path, expiry, lastAccessed, creationTime, isSecure, isHttpOnly));
    }

    return cookies;
}

void CookieDatabaseBackingStore::sendChangesToDatabaseSynchronously()
{
    CookieLog("CookieBackingStore - sending to database immediately");
    {
        MutexLocker lock(m_mutex);
        if (m_dbTimer.started())
            m_dbTimer.stop();
    }
    dispatchSyncMessage(createMethodCallMessage(&CookieDatabaseBackingStore::invokeSendChangesToDatabase, this));
}

void CookieDatabaseBackingStore::sendChangesToDatabase(int nextInterval)
{
    MutexLocker lock(m_mutex);
    if (!m_dbTimer.started()) {
        CookieLog("CookieBackingStore - Starting one shot send to database");
        m_dbTimer.start(nextInterval);
    } else {
#ifndef NDEBUG
        CookieLog("CookieBackingStore - Timer already running, skipping this request");
#endif
    }
}

void CookieDatabaseBackingStore::sendChangesToDatabaseTimerFired()
{
    dispatchMessage(createMethodCallMessage(&CookieDatabaseBackingStore::invokeSendChangesToDatabase, this));
}

void CookieDatabaseBackingStore::invokeSendChangesToDatabase()
{
    ASSERT(isCurrentThread());

    if (!m_db.isOpen()) {
        LOG_ERROR("Timer Fired, but database is closed.");
        return;
    }

    Vector<CookieAction> changedCookies;
    {
        MutexLocker lock(m_mutex);
        changedCookies.swap(m_changedCookies);
        ASSERT(m_changedCookies.isEmpty());
    }

    if (changedCookies.isEmpty()) {
        CookieLog("CookieBackingStore - Timer fired, but no cookies in changelist");
        return;
    }
    CookieLog("CookieBackingStore - Timer fired, sending changes to database. We have %d changes", changedCookies.size());
    SQLiteTransaction transaction(m_db, false);
    transaction.begin();

    // Iterate through every element in the change list to make calls
    // If error occurs, ignore it and continue to the next statement
    size_t sizeOfChange = changedCookies.size();
    for (size_t i = 0; i < sizeOfChange; i++) {
        SQLiteStatement* m_statement;
        const ParsedCookie cookie = changedCookies[i].first;
        UpdateParameter action = changedCookies[i].second;

        if (action == Delete) {
            m_statement = m_deleteStatement;
            CookieLog("CookieBackingStore - deleting cookie %s.", cookie.toString().utf8().data());

            // Binds all the values
            if (m_statement->bindText(1, cookie.name()) || m_statement->bindText(2, cookie.domain())
                || m_statement->bindText(3, cookie.path()) || m_statement->bindText(4, cookie.protocol())) {
                LOG_ERROR("Cannot bind cookie data to delete");
                LOG_ERROR("SQLite Error Message: %s", m_db.lastErrorMsg());
                ASSERT_NOT_REACHED();
                continue;
            }
        } else {
            if (action == Update) {
                CookieLog("CookieBackingStore - updating cookie %s.", cookie.toString().utf8().data());
                m_statement = m_updateStatement;
            } else {
                CookieLog("CookieBackingStore - inserting cookie %s.", cookie.toString().utf8().data());
                m_statement = m_insertStatement;
            }

            // Binds all the values
            if (m_statement->bindText(1, cookie.name()) || m_statement->bindText(2, cookie.value())
                || m_statement->bindText(3, cookie.domain()) || m_statement->bindText(4, cookie.path())
                || m_statement->bindDouble(5, cookie.expiry()) || m_statement->bindDouble(6, cookie.lastAccessed())
                || m_statement->bindInt64(7, cookie.isSecure()) || m_statement->bindInt64(8, cookie.isHttpOnly())
                || m_statement->bindDouble(9, cookie.creationTime()) || m_statement->bindText(10, cookie.protocol())) {
                LOG_ERROR("Cannot bind cookie data to save");
                LOG_ERROR("SQLite Error Message: %s", m_db.lastErrorMsg());
                ASSERT_NOT_REACHED();
                continue;
            }
        }

        int rc = m_statement->step();
        m_statement->reset();
        if (rc != SQLResultOk && rc != SQLResultDone) {
            LOG_ERROR("Cannot make call to the database");
            LOG_ERROR("SQLite Error Message: %s", m_db.lastErrorMsg());
            ASSERT_NOT_REACHED();
            continue;
        }
    }
    transaction.commit();
    CookieLog("CookieBackingStore - transaction complete");
}

void CookieDatabaseBackingStore::addToChangeQueue(const ParsedCookie* changedCookie, UpdateParameter actionParam)
{
    ASSERT(!changedCookie->isSession());
    ParsedCookie cookieCopy(changedCookie);
    CookieAction action(cookieCopy, actionParam);
    {
        MutexLocker lock(m_mutex);
        m_changedCookies.append(action);
        CookieLog("CookieBackingStore - m_changedcookies has %d.", m_changedCookies.size());
    }
    sendChangesToDatabase(s_databaseTimerInterval);
}

} // namespace WebCore
