/*
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#if ENABLE(BLACKBERRY_CREDENTIAL_PERSIST)
#include "CredentialBackingStore.h"

#include "CredentialStorage.h"
#include "KURL.h"
#include "NotImplemented.h"
#include "ProtectionSpaceHash.h"
#include "SQLiteStatement.h"
#include <wtf/UnusedParam.h>

#define HANDLE_SQL_EXEC_FAILURE(statement, returnValue, ...) \
    if (statement) { \
        LOG_ERROR(__VAR_ARGS__); \
        return returnValue; \
    }

namespace WebCore {

CredentialBackingStore* CredentialBackingStore::instance()
{
    static CredentialBackingStore* backingStore = 0;
    if (!backingStore)
        backingStore = new CredentialBackingStore;
    return backingStore;
}

CredentialBackingStore::CredentialBackingStore()
    : m_addStatement(0)
    , m_updateStatement(0)
    , m_hasStatement(0)
    , m_getStatement(0)
    , m_removeStatement(0)
{
}

CredentialBackingStore::~CredentialBackingStore()
{
    if (m_database.isOpen())
        m_database.close();
}

bool CredentialBackingStore::open(const String& dbPath)
{
    ASSERT(!m_database.isOpen());

    HANDLE_SQL_EXEC_FAILURE(!m_database.open(dbPath), false,
        "Failed to open database file %s for login database", dbPath.utf8().data());

    if (!m_database.tableExists("logins")) {
        HANDLE_SQL_EXEC_FAILURE(!m_database.executeCommand("CREATE TABLE logins (origin_url VARCHAR NOT NULL, host VARCHAR NOT NULL, port INTEGER, service_type INTEGER NOT NULL, realm VARCHAR, auth_scheme INTEGER NOT NULL, username VARCHAR, password BLOB) "),
            false, "Failed to create table logins for login database");

        // Create index for table logins.
        HANDLE_SQL_EXEC_FAILURE(!m_database.executeCommand("CREATE INDEX logins_signon ON logins (host)"),
            false, "Failed to create index for table logins");
    } else { // Initiate CredentialStorage.
        SQLiteStatement query(m_database, "SELECT origin_url, host, port, service_type, realm, auth_scheme, username, password FROM logins");
        HANDLE_SQL_EXEC_FAILURE(query.prepare() != SQLResultOk,
            false, "Failed to prepare query statement to initiate CredentialStorage");

        int result = query.step();
        while (result == SQLResultRow) {
            String strUrl = query.getColumnText(1);
            String strHost = query.getColumnText(2);
            int intPort = query.getColumnInt(3);
            ProtectionSpaceServerType serviceType = static_cast<ProtectionSpaceServerType>(query.getColumnInt(4));
            String strRealm = query.getColumnText(5);
            ProtectionSpaceAuthenticationScheme authScheme = static_cast<ProtectionSpaceAuthenticationScheme>(query.getColumnInt(6));
            String strUserName = query.getColumnText(7);
            String strPassword = decryptedString(query.getColumnText(8));

            KURL url(ParsedURLString, strUrl);
            ProtectionSpace protectionSpace(strHost, intPort, serviceType, strRealm, authScheme);
            Credential credential(strUserName, strPassword, CredentialPersistencePermanent);
            CredentialStorage::set(credential, protectionSpace, url);

            result = query.step();
        }
    }

    // Prepare the statements.
    m_addStatement = new SQLiteStatement(m_database, "INSERT OR REPLACE INTO logins (origin_url, host, port, service_type, realm, auth_scheme, username, password) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    HANDLE_SQL_EXEC_FAILURE(m_addStatement->prepare() != SQLResultOk,
        false, "Failed to prepare addLogin statement");

    m_updateStatement = new SQLiteStatement(m_database, "UPDATE logins SET username = ?, password = ? WHERE origin_url = ? AND host = ? AND port = ? AND service_type = ? AND realm = ? AND auth_scheme = ?");
    HANDLE_SQL_EXEC_FAILURE(m_updateStatement->prepare() != SQLResultOk,
        false, "Failed to prepare updateLogin statement");

    m_hasStatement = new SQLiteStatement(m_database, "SELECT COUNT(*) FROM logins WHERE host = ? AND port = ? AND service_type = ? AND realm = ? AND auth_scheme = ?");
    HANDLE_SQL_EXEC_FAILURE(m_hasStatement->prepare() != SQLResultOk,
        false, "Failed to prepare hasLogin statement");

    m_getStatement = new SQLiteStatement(m_database, "SELECT username, password FROM logins WHERE host = ? AND port = ? AND service_type = ? AND realm = ? AND auth_scheme = ?");
    HANDLE_SQL_EXEC_FAILURE(m_getStatement->prepare() != SQLResultOk,
        false, "Failed to prepare getLogin statement");

    m_removeStatement = new SQLiteStatement(m_database, "DELETE FROM logins WHERE host = ? AND port = ? AND service_type = ? AND realm = ? AND auth_scheme = ?");
    HANDLE_SQL_EXEC_FAILURE(m_removeStatement->prepare() != SQLResultOk,
        false, "Failed to prepare removeLogin statement");

    return true;
}

void CredentialBackingStore::close()
{
    delete m_addStatement;
    m_addStatement = 0;
    delete m_updateStatement;
    m_updateStatement = 0;
    delete m_hasStatement;
    m_hasStatement = 0;
    delete m_getStatement;
    m_getStatement = 0;
    delete m_removeStatement;
    m_removeStatement = 0;

    if (m_database.isOpen())
        m_database.close();
}

bool CredentialBackingStore::addLogin(const KURL& url, const ProtectionSpace& protectionSpace, const Credential& credential)
{
    ASSERT(m_database.isOpen());
    ASSERT(m_database.tableExists("logins"));

    if (!m_addStatement)
        return false;

    m_addStatement->bindText(1, url.string());
    m_addStatement->bindText(2, protectionSpace.host());
    m_addStatement->bindInt(3, protectionSpace.port());
    m_addStatement->bindInt(4, static_cast<int>(protectionSpace.serverType()));
    m_addStatement->bindText(5, protectionSpace.realm());
    m_addStatement->bindInt(6, static_cast<int>(protectionSpace.authenticationScheme()));
    m_addStatement->bindText(7, credential.user());
    m_addStatement->bindBlob(8, encryptedString(credential.password()));

    int result = m_addStatement->step();
    HANDLE_SQL_EXEC_FAILURE(result != SQLResultDone, false,
        "Failed to add login info into table logins - %i", result);

    return true;
}

bool CredentialBackingStore::updateLogin(const KURL& url, const ProtectionSpace& protectionSpace, const Credential& credential)
{
    ASSERT(m_database.isOpen());
    ASSERT(m_database.tableExists("logins"));

    if (!m_updateStatement)
        return false;

    m_updateStatement->bindText(1, url.string());
    m_updateStatement->bindText(2, credential.user());
    m_updateStatement->bindBlob(3, encryptedString(credential.password()));
    m_updateStatement->bindText(4, protectionSpace.host());
    m_updateStatement->bindInt(5, protectionSpace.port());
    m_updateStatement->bindInt(6, static_cast<int>(protectionSpace.serverType()));
    m_updateStatement->bindText(7, protectionSpace.realm());
    m_updateStatement->bindInt(8, static_cast<int>(protectionSpace.authenticationScheme()));

    int result = m_updateStatement->step();
    HANDLE_SQL_EXEC_FAILURE(result != SQLResultDone, false,
        "Failed to update login info in table logins - %i", result);

    return true;
}

bool CredentialBackingStore::hasLogin(const ProtectionSpace& protectionSpace)
{
    ASSERT(m_database.isOpen());
    ASSERT(m_database.tableExists("logins"));

    if (!m_hasStatement)
        return false;

    m_hasStatement->bindText(1, protectionSpace.host());
    m_hasStatement->bindInt(2, protectionSpace.port());
    m_hasStatement->bindInt(3, static_cast<int>(protectionSpace.serverType()));
    m_hasStatement->bindText(4, protectionSpace.realm());
    m_hasStatement->bindInt(5, static_cast<int>(protectionSpace.authenticationScheme()));

    int result = m_hasStatement->step();
    HANDLE_SQL_EXEC_FAILURE(result != SQLResultRow, false,
        "Failed to execute select login info from table logins in hasLogin - %i", result);

    if (m_hasStatement->getColumnInt(0))
        return true;
    return false;
}

Credential CredentialBackingStore::getLogin(const ProtectionSpace& protectionSpace)
{
    ASSERT(m_database.isOpen());
    ASSERT(m_database.tableExists("logins"));

    if (!m_getStatement)
        return Credential();

    m_getStatement->bindText(1, protectionSpace.host());
    m_getStatement->bindInt(2, protectionSpace.port());
    m_getStatement->bindInt(3, static_cast<int>(protectionSpace.serverType()));
    m_getStatement->bindText(4, protectionSpace.realm());
    m_getStatement->bindInt(5, static_cast<int>(protectionSpace.authenticationScheme()));

    int result = m_getStatement->step();
    HANDLE_SQL_EXEC_FAILURE(result != SQLResultRow, Credential(),
        "Failed to execute select login info from table logins in getLogin - %i", result);

    return Credential(m_getStatement->getColumnText(0), decryptedString(m_getStatement->getColumnText(1)), CredentialPersistencePermanent);
}

bool CredentialBackingStore::removeLogin(const ProtectionSpace& protectionSpace)
{
    ASSERT(m_database.isOpen());
    ASSERT(m_database.tableExists("logins"));

    if (!m_removeStatement)
        return false;

    m_removeStatement->bindText(1, protectionSpace.host());
    m_removeStatement->bindInt(2, protectionSpace.port());
    m_removeStatement->bindInt(3, static_cast<int>(protectionSpace.serverType()));
    m_removeStatement->bindText(4, protectionSpace.realm());
    m_removeStatement->bindInt(5, static_cast<int>(protectionSpace.authenticationScheme()));

    int result = m_removeStatement->step();
    HANDLE_SQL_EXEC_FAILURE(result != SQLResultDone, false,
        "Failed to remove login info from table logins - %i", result);

    return true;
}

bool CredentialBackingStore::clear()
{
    ASSERT(m_database.isOpen());
    ASSERT(m_database.tableExists("logins"));

    HANDLE_SQL_EXEC_FAILURE(!m_database.executeCommand("DELETE * FROM logins"),
        false, "Failed to clear table logins");

    return true;
}

String CredentialBackingStore::encryptedString(const String& plainText) const
{
    // FIXME: Need encrypt plainText here
    notImplemented();
    return plainText;
}

String CredentialBackingStore::decryptedString(const String& cipherText) const
{
    // FIXME: Need decrypt cipherText here
    notImplemented();
    return cipherText;
}

} // namespace WebCore

#endif // ENABLE(BLACKBERRY_CREDENTIAL_PERSIST)
