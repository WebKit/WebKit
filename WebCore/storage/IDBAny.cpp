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
#include "IDBAny.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBDatabaseRequest.h"
#include "IDBFactory.h"
#include "IDBIndex.h"
#include "IDBObjectStoreRequest.h"
#include "SerializedScriptValue.h"

namespace WebCore {

PassRefPtr<IDBAny> IDBAny::create()
{
    return adoptRef(new IDBAny());
}

IDBAny::IDBAny()
    : m_type(UndefinedType)
{
}

IDBAny::~IDBAny()
{
}

PassRefPtr<IDBDatabaseRequest> IDBAny::idbDatabaseRequest()
{
    ASSERT(m_type == IDBDatabaseRequestType);
    return m_idbDatabaseRequest;
}

PassRefPtr<IDBIndex> IDBAny::idbIndex()
{
    ASSERT(m_type == IDBIndexType);
    return m_idbIndex;
}

PassRefPtr<IDBKey> IDBAny::idbKey()
{
    ASSERT(m_type == IDBKeyType);
    return m_idbKey;
}

PassRefPtr<IDBObjectStoreRequest> IDBAny::idbObjectStoreRequest()
{
    ASSERT(m_type == IDBObjectStoreRequestType);
    return m_idbObjectStoreRequest;
}

PassRefPtr<IDBFactory> IDBAny::idbFactory()
{
    ASSERT(m_type == IDBFactoryType);
    return m_idbFactory;
}

PassRefPtr<SerializedScriptValue> IDBAny::serializedScriptValue()
{
    ASSERT(m_type == SerializedScriptValueType);
    return m_serializedScriptValue;
}

void IDBAny::set()
{
    ASSERT(m_type == UndefinedType);
    m_type = NullType;
}

void IDBAny::set(PassRefPtr<IDBDatabaseRequest> value)
{
    ASSERT(m_type == UndefinedType);
    m_type = IDBDatabaseRequestType;
    m_idbDatabaseRequest = value;
}

void IDBAny::set(PassRefPtr<IDBIndex> value)
{
    ASSERT(m_type == UndefinedType);
    m_type = IDBDatabaseRequestType;
    m_idbIndex = value;
}

void IDBAny::set(PassRefPtr<IDBKey> value)
{
    ASSERT(m_type == UndefinedType);
    m_type = IDBKeyType;
    m_idbKey = value;
}

void IDBAny::set(PassRefPtr<IDBObjectStoreRequest> value)
{
    ASSERT(m_type == UndefinedType);
    m_type = IDBObjectStoreRequestType;
    m_idbObjectStoreRequest = value;
}

void IDBAny::set(PassRefPtr<IDBFactory> value)
{
    ASSERT(m_type == UndefinedType);
    m_type = IDBFactoryType;
    m_idbFactory = value;
}

void IDBAny::set(PassRefPtr<SerializedScriptValue> value)
{
    ASSERT(m_type == UndefinedType);
    m_type = SerializedScriptValueType;
    m_serializedScriptValue = value;
}

} // namespace WebCore

#endif
