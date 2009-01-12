/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#ifndef LocalStorage_h
#define LocalStorage_h

#include "LocalStorageArea.h"
#include "LocalStorageTask.h"
#include "LocalStorageThread.h"
#include "SecurityOriginHash.h"

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Threading.h>

namespace WebCore {

    class PageGroup;
    class StorageArea;

    class LocalStorage : public ThreadSafeShared<LocalStorage> {
    public:
        ~LocalStorage();

        static PassRefPtr<LocalStorage> localStorage(const String& path);

        PassRefPtr<StorageArea> storageArea(SecurityOrigin*);

        bool scheduleImport(PassRefPtr<LocalStorageArea>);
        void scheduleSync(PassRefPtr<LocalStorageArea>);

        void close();

    private:
        LocalStorage(const String& path);

        typedef HashMap<RefPtr<SecurityOrigin>, RefPtr<LocalStorageArea>, SecurityOriginHash> LocalStorageAreaMap;
        LocalStorageAreaMap m_storageAreaMap;

        RefPtr<LocalStorageThread> m_thread;

    // The following members are subject to thread synchronization issues
    public:
        // To be called from the background thread:
        String fullDatabaseFilename(SecurityOrigin*);

        void performImport();
        void performSync();

    private:
        String m_path;

        typedef HashMap<RefPtr<SecurityOrigin>, unsigned long long, SecurityOriginHash> SecurityOriginQuoteMap;
        SecurityOriginQuoteMap m_securityOriginQuoteMap;
    };

} // namespace WebCore

#endif // LocalStorage_h
