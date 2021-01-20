/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CredentialBase.h"

#include "Credential.h"

namespace WebCore {

// Need to enforce empty, non-null strings due to the pickiness of the String == String operator
// combined with the semantics of the String(NSString*) constructor
CredentialBase::CredentialBase()
    : m_user(emptyString())
    , m_password(emptyString())
    , m_persistence(CredentialPersistenceNone)
{
}
   
// Need to enforce empty, non-null strings due to the pickiness of the String == String operator
// combined with the semantics of the String(NSString*) constructor
CredentialBase::CredentialBase(const String& user, const String& password, CredentialPersistence persistence)
    : m_user(user.length() ? user : emptyString())
    , m_password(password.length() ? password : emptyString())
    , m_persistence(persistence)
{
}
    
CredentialBase::CredentialBase(const Credential& original, CredentialPersistence persistence)
    : m_user(original.user())
    , m_password(original.password())
    , m_persistence(persistence)
{
}

bool CredentialBase::isEmpty() const
{
    return m_user.isEmpty() && m_password.isEmpty();
}
    
const String& CredentialBase::user() const
{ 
    return m_user; 
}

const String& CredentialBase::password() const
{ 
    return m_password; 
}

bool CredentialBase::hasPassword() const
{ 
    return !m_password.isEmpty(); 
}

CredentialPersistence CredentialBase::persistence() const
{ 
    return m_persistence; 
}

bool CredentialBase::compare(const Credential& a, const Credential& b)
{
    // Check persistence first since all credential types
    // have the persistence property.
    if (a.persistence() != b.persistence())
        return false;
    if (a.user() != b.user())
        return false;
    if (a.password() != b.password())
        return false;
        
    return Credential::platformCompare(a, b);
}

}
