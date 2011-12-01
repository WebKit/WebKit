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

#ifndef CredentialBackingStore_h
#define CredentialBackingStore_h

#if ENABLE(BLACKBERRY_CREDENTIAL_PERSIST)

#include "Credential.h"
#include "SQLiteDatabase.h"

namespace WebCore {

class KURL;
class ProtectionSpace;

class CredentialBackingStore {
public:
    static CredentialBackingStore* instance();
    ~CredentialBackingStore();
    bool open(const String& dbPath);
    void close();
    bool addLogin(const KURL&, const ProtectionSpace&, const Credential&);
    bool updateLogin(const KURL&, const ProtectionSpace&, const Credential&);
    bool hasLogin(const ProtectionSpace&);
    Credential getLogin(const ProtectionSpace&);
    bool removeLogin(const ProtectionSpace&);
    bool clear();

private:
    CredentialBackingStore();
    String encryptedString(const String& plainText) const;
    String decryptedString(const String& cipherText) const;

    SQLiteDatabase m_database;
    SQLiteStatement* m_addStatement;
    SQLiteStatement* m_updateStatement;
    SQLiteStatement* m_hasStatement;
    SQLiteStatement* m_getStatement;
    SQLiteStatement* m_removeStatement;
};

} // namespace WebCore

#endif // ENABLE(BLACKBERRY_CREDENTIAL_PERSIST)

#endif // CredentialBackingStore_h
