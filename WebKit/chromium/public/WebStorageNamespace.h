/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebStorageNamespace_h
#define WebStorageNamespace_h

#include "WebCommon.h"

namespace WebKit {

class WebStorageArea;
class WebString;

// WebStorageNamespace represents a collection of StorageAreas. Typically, you'll have
// multiple StorageNamespaces to represent the SessionStorage for each tab and a single
// StorageNamespace to represent LocalStorage for the entire browser.
class WebStorageNamespace {
public:
    // Create a new WebStorageNamespace. LocalStorageNamespaces require a path to specify
    // where the SQLite databases that make LocalStorage data persistent are located.
    // If path is empty, data will not persist. You should call delete on the returned
    // object when you're finished.
    WEBKIT_API static WebStorageNamespace* createLocalStorageNamespace(const WebString& backingDirectoryPath, unsigned quota);
    WEBKIT_API static WebStorageNamespace* createSessionStorageNamespace(unsigned quota);

    // The quota for each storage area.  Suggested by the spec.
    static const unsigned m_localStorageQuota = 5 * 1024 * 1024;

    // Since SessionStorage memory is allocated in the browser process, we place a
    // per-origin quota on it.  Like LocalStorage there are known attacks against
    // this, so it's more of a sanity check than a real security measure.
    static const unsigned m_sessionStorageQuota = 5 * 1024 * 1024;

    static const unsigned noQuota = UINT_MAX;

    virtual ~WebStorageNamespace() { }

    // Create a new WebStorageArea object. Two subsequent calls with the same origin
    // will return two different WebStorageArea objects that share the same backing store.
    // You should call delete on the returned object when you're finished.
    virtual WebStorageArea* createStorageArea(const WebString& origin) = 0;

    // Copy a StorageNamespace. This only makes sense in the case of SessionStorage.
    virtual WebStorageNamespace* copy() = 0;

    // Shutdown the StorageNamespace. Write all StorageArea's to disk and disallow new
    // write activity.
    virtual void close() = 0;
};

} // namespace WebKit

#endif // WebStorageNamespace_h
