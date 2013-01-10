/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef WebIDBMetadata_h
#define WebIDBMetadata_h

#include "WebIDBKeyPath.h"
#include "platform/WebCommon.h"
#include "platform/WebString.h"
#include "platform/WebVector.h"

namespace WebCore {
struct IDBDatabaseMetadata;
}

namespace WebKit {

struct WebIDBMetadata {
    enum {
        NoIntVersion = -1
    };
    struct Index;
    struct ObjectStore;

    WebString name;
    // FIXME: Both version members need to be present while we support both the
    // old setVersion and new upgradeneeded API. Once we no longer support
    // setVersion, WebString version can be removed.
    WebString version;
    long long intVersion;
    long long id;
    long long maxObjectStoreId;
    WebVector<ObjectStore> objectStores;
    WebIDBMetadata()
        : intVersion(NoIntVersion) { }

    struct ObjectStore {
        WebString name;
        WebIDBKeyPath keyPath;
        bool autoIncrement;
        long long id;
        long long maxIndexId;
        WebVector<Index> indexes;
        ObjectStore()
            : keyPath(WebIDBKeyPath::createNull())
            , autoIncrement(false) { }
    };

    struct Index {
        WebString name;
        WebIDBKeyPath keyPath;
        bool unique;
        bool multiEntry;
        long long id;
        Index()
            : keyPath(WebIDBKeyPath::createNull())
            , unique(false)
            , multiEntry(false) { }
    };

#if WEBKIT_IMPLEMENTATION
    WebIDBMetadata(const WebCore::IDBDatabaseMetadata&);
    operator WebCore::IDBDatabaseMetadata() const;
#endif
};


} // namespace WebKit

#endif // WebIDBMetadata_h
