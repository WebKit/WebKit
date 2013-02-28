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
#include "FileSystem.h"
#include "KURL.h"
#include "NotImplemented.h"
#include "ProtectionSpaceHash.h"
#include "SQLiteStatement.h"
#include <BlackBerryPlatformEncryptor.h>
#include <BlackBerryPlatformSettings.h>
#include <CertMgrWrapper.h>

#define HANDLE_SQL_EXEC_FAILURE(statement, returnValue, ...) \
    if (statement) { \
        LOG_ERROR(__VA_ARGS__); \
        return returnValue; \
    }

namespace WebCore {

static unsigned hashCredentialInfo(const ProtectionSpace& space, const String& username)
{
    String hashString = String::format("%s@%s@%d@%d@%s@%d",
        username.utf8().data(),
        space.host().utf8().data(), space.port(),
        static_cast<int>(space.serverType()),
        space.realm().utf8().data(),
        static_cast<int>(space.authenticationScheme()));
    return StringHasher::computeHashAndMaskTop8Bits(hashString.characters(), hashString.length());
}

CredentialBackingStore& credentialBackingStore()
{
    DEFINE_STATIC_LOCAL(CredentialBackingStore, backingStore, ());
    if (!backingStore.m_database.isOpen())
        backingStore.open(pathByAppendingComponent(BlackBerry::Platform::Settings::instance()->applicationDataDirectory().c_str(), "/credentials.db"));
    return backingStore;
}

CredentialBackingStore::CredentialBackingStore()
    : m_addLoginStatement(0)
    , m_updateLoginStatement(0)
    , m_hasLoginStatement(0)
    , m_getLoginStatement(0)
    , m_removeLoginStatement(0)
    , m_addNeverRememberStatement(0)
    , m_hasNeverRememberStatement(0)
    , m_removeNeverRememberStatement(0)
    , m_certMgrWrapper(0)
{
}

CredentialBackingStore::~CredentialBackingStore()
{
    delete m_certMgrWrapper;
    m_certMgrWrapper = 0;
    delete m_addLoginStatement;
    m_addLoginStatement = 0;
    delete m_updateLoginStatement;
    m_updateLoginStatement = 0;
    delete m_hasLoginStatement;
    m_hasLoginStatement = 0;
    delete m_getLoginStatement;
    m_getLoginStatement = 0;
    delete m_removeLoginStatement;
    m_removeLoginStatement = 0;
    delete m_addNeverRememberStatement;
    m_addNeverRememberStatement = 0;
    delete m_hasNeverRememberStatement;
    m_hasNeverRememberStatement = 0;
    delete m_removeNeverRememberStatement;
    m_removeNeverRememberStatement = 0;

    if (m_database.isOpen())
        m_database.close();
}

bool CredentialBackingStore::open(const String& dbPath)
{
    ASSERT(!m_database.isOpen());

    HANDLE_SQL_EXEC_FAILURE(!m_database.open(dbPath), false,
        "Failed to open database file %s for login database", dbPath.utf8().data());

    if (!m_database.tableExists("logins")) {
        HANDLE_SQL_EXEC_FAILURE(!m_database.executeCommand("CREATE TABLE logins (host VARCHAR NOT NULL, port INTEGER, service_type INTEGER NOT NULL, realm VARCHAR, auth_scheme INTEGER NOT NULL, username VARCHAR, password BLOB) "),
            false, "Failed to create table logins for login database");

        // Create index for table logins.
        HANDLE_SQL_EXEC_FAILURE(!m_database.executeCommand("CREATE INDEX logins_index ON logins (host)"),
            false, "Failed to create index for table logins");
    }

    if (!m_database.tableExists("never_remember")) {
        HANDLE_SQL_EXEC_FAILURE(!m_database.executeCommand("CREATE TABLE never_remember (host VARCHAR NOT NULL, port INTEGER, service_type INTEGER NOT NULL, realm VARCHAR, auth_scheme INTEGER NOT NULL) "),
            false, "Failed to create table never_remember for login database");

        // Create index for table never_remember.
        HANDLE_SQL_EXEC_FAILURE(!m_database.executeCommand("CREATE INDEX never_remember_index ON never_remember (host)"),
            false, "Failed to create index for table never_remember");
    }

    // Prepare the statements.
    m_addLoginStatement = new SQLiteStatement(m_database, "INSERT OR REPLACE INTO logins (host, port, service_type, realm, auth_scheme, username, password) VALUES (?, ?, ?, ?, ?, ?, ?)");
    HANDLE_SQL_EXEC_FAILURE(m_addLoginStatement->prepare() != SQLResultOk,
        false, "Failed to prepare addLogin statement");

    m_updateLoginStatement = new SQLiteStatement(m_database, "UPDATE logins SET username = ?, password = ? WHERE host = ? AND port = ? AND service_type = ? AND realm = ? AND auth_scheme = ?");
    HANDLE_SQL_EXEC_FAILURE(m_updateLoginStatement->prepare() != SQLResultOk,
        false, "Failed to prepare updateLogin statement");

    m_hasLoginStatement = new SQLiteStatement(m_database, "SELECT COUNT(*) FROM logins WHERE host = ? AND port = ? AND service_type = ? AND realm = ? AND auth_scheme = ?");
    HANDLE_SQL_EXEC_FAILURE(m_hasLoginStatement->prepare() != SQLResultOk,
        false, "Failed to prepare hasLogin statement");

    m_getLoginStatement = new SQLiteStatement(m_database, "SELECT username, password FROM logins WHERE host = ? AND port = ? AND service_type = ? AND realm = ? AND auth_scheme = ?");
    HANDLE_SQL_EXEC_FAILURE(m_getLoginStatement->prepare() != SQLResultOk,
        false, "Failed to prepare getLogin statement");

    m_removeLoginStatement = new SQLiteStatement(m_database, "DELETE FROM logins WHERE host = ? AND port = ? AND service_type = ? AND realm = ? AND auth_scheme = ?");
    HANDLE_SQL_EXEC_FAILURE(m_removeLoginStatement->prepare() != SQLResultOk,
        false, "Failed to prepare removeLogin statement");

    m_addNeverRememberStatement = new SQLiteStatement(m_database, "INSERT OR REPLACE INTO never_remember (host, port, service_type, realm, auth_scheme) VALUES (?, ?, ?, ?, ?)");
    HANDLE_SQL_EXEC_FAILURE(m_addNeverRememberStatement->prepare() != SQLResultOk,
        false, "Failed to prepare addNeverRemember statement");

    m_hasNeverRememberStatement = new SQLiteStatement(m_database, "SELECT COUNT(*) FROM never_remember WHERE host = ? AND port = ? AND service_type = ? AND realm = ? AND auth_scheme = ?");
    HANDLE_SQL_EXEC_FAILURE(m_hasNeverRememberStatement->prepare() != SQLResultOk,
        false, "Failed to prepare hasNeverRemember statement");

    m_removeNeverRememberStatement = new SQLiteStatement(m_database, "DELETE FROM never_remember WHERE host = ? AND port = ? AND service_type = ? AND realm = ? AND auth_scheme = ?");
    HANDLE_SQL_EXEC_FAILURE(m_removeNeverRememberStatement->prepare() != SQLResultOk,
        false, "Failed to prepare removeNeverRemember statement");

    return true;
}

bool CredentialBackingStore::addLogin(const ProtectionSpace& protectionSpace, const Credential& credential)
{
    ASSERT(m_database.isOpen());
    ASSERT(m_database.tableExists("logins"));

    if (!m_addLoginStatement)
        return false;

    m_addLoginStatement->bindText(1, protectionSpace.host());
    m_addLoginStatement->bindInt(2, protectionSpace.port());
    m_addLoginStatement->bindInt(3, static_cast<int>(protectionSpace.serverType()));
    m_addLoginStatement->bindText(4, protectionSpace.realm());
    m_addLoginStatement->bindInt(5, static_cast<int>(protectionSpace.authenticationScheme()));
    m_addLoginStatement->bindText(6, credential.user());
    if (certMgrWrapper()->isReady())
        m_addLoginStatement->bindBlob(7, "");
    else {
        String ciphertext = encryptedString(credential.password());
        ASSERT(ciphertext.is8Bit());
        m_addLoginStatement->bindBlob(7, ciphertext.characters8(), ciphertext.length());
    }

    int result = m_addLoginStatement->step();
    m_addLoginStatement->reset();
    HANDLE_SQL_EXEC_FAILURE(result != SQLResultDone, false,
        "Failed to add login info into table logins - %i", result);

    if (!certMgrWrapper()->isReady())
        return true;

    String ciphertext = encryptedString(credential.password());
    ASSERT(ciphertext.is8Bit());
    unsigned hash = hashCredentialInfo(protectionSpace, credential.user());
    return certMgrWrapper()->savePassword(hash, std::string(reinterpret_cast<const char*>(ciphertext.characters8()), ciphertext.length()));
}

bool CredentialBackingStore::updateLogin(const ProtectionSpace& protectionSpace, const Credential& credential)
{
    ASSERT(m_database.isOpen());
    ASSERT(m_database.tableExists("logins"));

    if (!m_updateLoginStatement)
        return false;

    m_updateLoginStatement->bindText(1, credential.user());
    if (certMgrWrapper()->isReady())
        m_updateLoginStatement->bindBlob(2, "");
    else {
        String ciphertext = encryptedString(credential.password());
        ASSERT(ciphertext.is8Bit());
        m_updateLoginStatement->bindBlob(2, ciphertext.characters8(), ciphertext.length());
    }
    m_updateLoginStatement->bindText(3, protectionSpace.host());
    m_updateLoginStatement->bindInt(4, protectionSpace.port());
    m_updateLoginStatement->bindInt(5, static_cast<int>(protectionSpace.serverType()));
    m_updateLoginStatement->bindText(6, protectionSpace.realm());
    m_updateLoginStatement->bindInt(7, static_cast<int>(protectionSpace.authenticationScheme()));

    int result = m_updateLoginStatement->step();
    m_updateLoginStatement->reset();
    HANDLE_SQL_EXEC_FAILURE(result != SQLResultDone, false,
        "Failed to update login info in table logins - %i", result);

    if (!certMgrWrapper()->isReady())
        return true;

    String ciphertext = encryptedString(credential.password());
    ASSERT(ciphertext.is8Bit());
    unsigned hash = hashCredentialInfo(protectionSpace, credential.user());
    return certMgrWrapper()->savePassword(hash, std::string(reinterpret_cast<const char*>(ciphertext.characters8()), ciphertext.length()));
}

bool CredentialBackingStore::hasLogin(const ProtectionSpace& protectionSpace)
{
    ASSERT(m_database.isOpen());
    ASSERT(m_database.tableExists("logins"));

    if (!m_hasLoginStatement)
        return false;

    m_hasLoginStatement->bindText(1, protectionSpace.host());
    m_hasLoginStatement->bindInt(2, protectionSpace.port());
    m_hasLoginStatement->bindInt(3, static_cast<int>(protectionSpace.serverType()));
    m_hasLoginStatement->bindText(4, protectionSpace.realm());
    m_hasLoginStatement->bindInt(5, static_cast<int>(protectionSpace.authenticationScheme()));

    int result = m_hasLoginStatement->step();
    int numOfRow = m_hasLoginStatement->getColumnInt(0);
    m_hasLoginStatement->reset();
    HANDLE_SQL_EXEC_FAILURE(result != SQLResultRow, false,
        "Failed to execute select login info from table logins in hasLogin - %i", result);

    if (numOfRow)
        return true;
    return false;
}

Credential CredentialBackingStore::getLogin(const ProtectionSpace& protectionSpace)
{
    ASSERT(m_database.isOpen());
    ASSERT(m_database.tableExists("logins"));

    if (!m_getLoginStatement)
        return Credential();

    m_getLoginStatement->bindText(1, protectionSpace.host());
    m_getLoginStatement->bindInt(2, protectionSpace.port());
    m_getLoginStatement->bindInt(3, static_cast<int>(protectionSpace.serverType()));
    m_getLoginStatement->bindText(4, protectionSpace.realm());
    m_getLoginStatement->bindInt(5, static_cast<int>(protectionSpace.authenticationScheme()));

    int result = m_getLoginStatement->step();
    String username = m_getLoginStatement->getColumnText(0);
    String password = m_getLoginStatement->getColumnBlobAsString(1);
    m_getLoginStatement->reset();
    HANDLE_SQL_EXEC_FAILURE(result != SQLResultRow, Credential(),
        "Failed to execute select login info from table logins in getLogin - %i", result);

    if (!certMgrWrapper()->isReady())
        return Credential(username, decryptedString(password), CredentialPersistencePermanent);

    unsigned hash = hashCredentialInfo(protectionSpace, username);

    std::string passwordBlob;
    if (!certMgrWrapper()->getPassword(hash, passwordBlob))
        return Credential();

    return Credential(username, decryptedString(String(passwordBlob.data(), passwordBlob.length())), CredentialPersistencePermanent);
}

bool CredentialBackingStore::removeLogin(const ProtectionSpace& protectionSpace, const String& username)
{
    ASSERT(m_database.isOpen());
    ASSERT(m_database.tableExists("logins"));

    if (!m_removeLoginStatement)
        return false;

    m_removeLoginStatement->bindText(1, protectionSpace.host());
    m_removeLoginStatement->bindInt(2, protectionSpace.port());
    m_removeLoginStatement->bindInt(3, static_cast<int>(protectionSpace.serverType()));
    m_removeLoginStatement->bindText(4, protectionSpace.realm());
    m_removeLoginStatement->bindInt(5, static_cast<int>(protectionSpace.authenticationScheme()));

    int result = m_removeLoginStatement->step();
    m_removeLoginStatement->reset();
    HANDLE_SQL_EXEC_FAILURE(result != SQLResultDone, false,
        "Failed to remove login info from table logins - %i", result);

    if (!certMgrWrapper()->isReady())
        return true;

    unsigned hash = hashCredentialInfo(protectionSpace, username);
    if (!certMgrWrapper()->removePassword(hash))
        return false;

    return true;
}

bool CredentialBackingStore::addNeverRemember(const ProtectionSpace& protectionSpace)
{
    ASSERT(m_database.isOpen());
    ASSERT(m_database.tableExists("never_remember"));

    if (!m_addNeverRememberStatement)
        return false;

    m_addNeverRememberStatement->bindText(1, protectionSpace.host());
    m_addNeverRememberStatement->bindInt(2, protectionSpace.port());
    m_addNeverRememberStatement->bindInt(3, static_cast<int>(protectionSpace.serverType()));
    m_addNeverRememberStatement->bindText(4, protectionSpace.realm());
    m_addNeverRememberStatement->bindInt(5, static_cast<int>(protectionSpace.authenticationScheme()));

    int result = m_addNeverRememberStatement->step();
    m_addNeverRememberStatement->reset();
    HANDLE_SQL_EXEC_FAILURE(result != SQLResultDone, false,
        "Failed to add naver saved item info into table never_remember - %i", result);

    return true;
}

bool CredentialBackingStore::hasNeverRemember(const ProtectionSpace& protectionSpace)
{
    ASSERT(m_database.isOpen());
    ASSERT(m_database.tableExists("never_remember"));

    if (!m_hasNeverRememberStatement)
        return false;

    m_hasNeverRememberStatement->bindText(1, protectionSpace.host());
    m_hasNeverRememberStatement->bindInt(2, protectionSpace.port());
    m_hasNeverRememberStatement->bindInt(3, static_cast<int>(protectionSpace.serverType()));
    m_hasNeverRememberStatement->bindText(4, protectionSpace.realm());
    m_hasNeverRememberStatement->bindInt(5, static_cast<int>(protectionSpace.authenticationScheme()));

    int result = m_hasNeverRememberStatement->step();
    int numOfRow = m_hasNeverRememberStatement->getColumnInt(0);
    m_hasNeverRememberStatement->reset();
    HANDLE_SQL_EXEC_FAILURE(result != SQLResultRow, false,
        "Failed to execute select to find naver saved site from table never_remember - %i", result);

    if (numOfRow)
        return true;
    return false;
}

bool CredentialBackingStore::removeNeverRemember(const ProtectionSpace& protectionSpace)
{
    ASSERT(m_database.isOpen());
    ASSERT(m_database.tableExists("never_remember"));

    if (!m_removeNeverRememberStatement)
        return false;

    m_removeNeverRememberStatement->bindText(1, protectionSpace.host());
    m_removeNeverRememberStatement->bindInt(2, protectionSpace.port());
    m_removeNeverRememberStatement->bindInt(3, static_cast<int>(protectionSpace.serverType()));
    m_removeNeverRememberStatement->bindText(4, protectionSpace.realm());
    m_removeNeverRememberStatement->bindInt(5, static_cast<int>(protectionSpace.authenticationScheme()));

    int result = m_removeNeverRememberStatement->step();
    m_removeNeverRememberStatement->reset();
    HANDLE_SQL_EXEC_FAILURE(result != SQLResultDone, false,
        "Failed to remove never saved site from table never_remember - %i", result);

    return true;
}

bool CredentialBackingStore::clearLogins()
{
    ASSERT(m_database.isOpen());
    ASSERT(m_database.tableExists("logins"));

    HANDLE_SQL_EXEC_FAILURE(!m_database.executeCommand("DELETE FROM logins"),
        false, "Failed to clear table logins");

    return true;
}

bool CredentialBackingStore::clearNeverRemember()
{
    ASSERT(m_database.isOpen());
    ASSERT(m_database.tableExists("never_remember"));

    HANDLE_SQL_EXEC_FAILURE(!m_database.executeCommand("DELETE FROM never_remember"),
        false, "Failed to clear table never_remember");

    return true;
}

String CredentialBackingStore::encryptedString(const String& plainText) const
{
    WTF::CString utf8 = plainText.utf8(true);
    std::string cipherText;
    BlackBerry::Platform::Encryptor::encryptString(std::string(utf8.data(), utf8.length()), &cipherText);
    return String(cipherText.data(), cipherText.length());
}

String CredentialBackingStore::decryptedString(const String& cipherText) const
{
    std::string text = cipherText.is8Bit() ?
        std::string(reinterpret_cast<const char*>(cipherText.characters8()), cipherText.length() * sizeof(LChar)) :
        std::string(reinterpret_cast<const char*>(cipherText.characters16()), cipherText.length() * sizeof(UChar));

    std::string plainText;
    BlackBerry::Platform::Encryptor::decryptString(text, &plainText);

    return String::fromUTF8(plainText.data(), plainText.length());
}

BlackBerry::Platform::CertMgrWrapper* CredentialBackingStore::certMgrWrapper()
{
    if (!m_certMgrWrapper)
        m_certMgrWrapper = new BlackBerry::Platform::CertMgrWrapper();

    return m_certMgrWrapper;
}


} // namespace WebCore

#endif // ENABLE(BLACKBERRY_CREDENTIAL_PERSIST)
