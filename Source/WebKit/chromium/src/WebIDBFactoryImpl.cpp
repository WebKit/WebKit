/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebIDBFactoryImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMStringList.h"
#include "IDBCallbacksProxy.h"
#include "IDBFactoryBackendImpl.h"
#include "SecurityOrigin.h"
#include "WebIDBDatabaseError.h"
#include <wtf/OwnPtr.h>

using namespace WebCore;

namespace WebKit {

static WebIDBFactory::BackingStoreType overriddenBackingStoreType = WebIDBFactory::DefaultBackingStore;
static WebString tempDatabaseFolder;

WebIDBFactory* WebIDBFactory::create()
{
    return new WebIDBFactoryImpl();
}

void WebIDBFactory::setOverrideBackingStoreType(BackingStoreType type)
{
    overriddenBackingStoreType = type;
}

void WebIDBFactory::setTemporaryDatabaseFolder(const WebString& path)
{
    tempDatabaseFolder = path;
}

WebIDBFactoryImpl::WebIDBFactoryImpl()
    : m_idbFactoryBackend(IDBFactoryBackendImpl::create())
{
    m_idbFactoryBackend->setEnableMigration(true);
}

WebIDBFactoryImpl::~WebIDBFactoryImpl()
{
}

void WebIDBFactoryImpl::open(const WebString& name, WebIDBCallbacks* callbacks, const WebSecurityOrigin& origin, WebFrame*, const WebString& dataDir, unsigned long long maximumSize, BackingStoreType backingStoreType)
{
    WebString path = dataDir;
    if (overriddenBackingStoreType != DefaultBackingStore) {
        backingStoreType = overriddenBackingStoreType;

        // The dataDir is empty for two reasons: LevelDB in icognito mode or
        // LevelDB from DumpRenderTree. The first case is taken care of inside
        // IDBFactoryBackendImpl.cpp by forcing SQLITE backend for incognito.
        // For the DumpRenderTree case we need to keep track of the location
        // so we can wipe it out when we're done with the test.
        if (dataDir.isEmpty() && backingStoreType == LevelDBBackingStore)
            path = tempDatabaseFolder;
    }
    m_idbFactoryBackend->open(name, IDBCallbacksProxy::create(adoptPtr(callbacks)), origin, 0, path, maximumSize, static_cast<IDBFactoryBackendInterface::BackingStoreType>(backingStoreType));
}

void WebIDBFactoryImpl::setEnableMigration(bool enable)
{
    m_idbFactoryBackend->setEnableMigration(enable);
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
