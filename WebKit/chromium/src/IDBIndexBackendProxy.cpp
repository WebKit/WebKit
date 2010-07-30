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
#include "IDBIndexBackendProxy.h"

#include "WebIDBDatabaseError.h"
#include "WebIDBIndex.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

PassRefPtr<IDBIndexBackendInterface> IDBIndexBackendProxy::create(PassOwnPtr<WebKit::WebIDBIndex> index)
{
    return adoptRef(new IDBIndexBackendProxy(index));
}

IDBIndexBackendProxy::IDBIndexBackendProxy(PassOwnPtr<WebKit::WebIDBIndex> index)
    : m_webIDBIndex(index)
{
}

IDBIndexBackendProxy::~IDBIndexBackendProxy()
{
}

String IDBIndexBackendProxy::name()
{
    return m_webIDBIndex->name();
}

String IDBIndexBackendProxy::keyPath()
{
    return m_webIDBIndex->keyPath();
}

bool IDBIndexBackendProxy::unique()
{
    return m_webIDBIndex->unique();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
