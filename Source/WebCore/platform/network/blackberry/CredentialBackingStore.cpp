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
}

bool CredentialBackingStore::open(const String& dbPath)
{
    UNUSED_PARAM(dbPath);

    notImplemented();
    return false;
}

void CredentialBackingStore::close()
{
    notImplemented();
}

bool CredentialBackingStore::addLogin(const KURL& url, const ProtectionSpace& protectionSpace, const Credential& credential)
{
    UNUSED_PARAM(url);
    UNUSED_PARAM(protectionSpace);
    UNUSED_PARAM(credential);

    notImplemented();
    return false;
}

bool CredentialBackingStore::updateLogin(const KURL& url, const ProtectionSpace& protectionSpace, const Credential& credential)
{
    UNUSED_PARAM(url);
    UNUSED_PARAM(protectionSpace);
    UNUSED_PARAM(credential);

    notImplemented();
    return false;
}

bool CredentialBackingStore::hasLogin(const ProtectionSpace& protectionSpace)
{
    UNUSED_PARAM(protectionSpace);

    notImplemented();
    return false;
}

Credential CredentialBackingStore::getLogin(const ProtectionSpace& protectionSpace)
{
    UNUSED_PARAM(protectionSpace);

    notImplemented();
    return Credential();
}

bool CredentialBackingStore::removeLogin(const ProtectionSpace& protectionSpace)
{
    UNUSED_PARAM(protectionSpace);

    notImplemented();
    return false;
}

bool CredentialBackingStore::clear()
{
    notImplemented();
    return false;
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
