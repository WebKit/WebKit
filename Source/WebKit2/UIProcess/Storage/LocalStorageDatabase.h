/*
 * Copyright (C) 2008, 2009, 2010, 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LocalStorageDatabase_h
#define LocalStorageDatabase_h

#include <WebCore/SQLiteDatabase.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

class WorkQueue;

namespace WebCore {
class StorageMap;
}

namespace WebKit {

class LocalStorageDatabase : public ThreadSafeRefCounted<LocalStorageDatabase> {
public:
    static PassRefPtr<LocalStorageDatabase> create(const String& databaseFilename, PassRefPtr<WorkQueue>);
    ~LocalStorageDatabase();

    // Will block until the import is complete.
    void importItems(WebCore::StorageMap&);

private:
    LocalStorageDatabase(const String& databaseFilename, PassRefPtr<WorkQueue>);

    enum DatabaseOpeningStrategy {
        CreateIfNonExistent,
        SkipIfNonExistent
    };
    bool tryToOpenDatabase(DatabaseOpeningStrategy);
    void openDatabase(DatabaseOpeningStrategy);

    bool migrateItemTableIfNeeded();

    void performImport();

    String m_databaseFilename;
    RefPtr<WorkQueue> m_queue;

    WebCore::SQLiteDatabase m_database;
    bool m_failedToOpenDatabase;
};


} // namespace WebKit

#endif // LocalStorageDatabase_h
