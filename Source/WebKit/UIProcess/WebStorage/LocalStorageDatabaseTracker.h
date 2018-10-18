/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/SecurityOriginData.h>
#include <wtf/HashSet.h>
#include <wtf/Optional.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WallTime.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

struct LocalStorageDetails;

class LocalStorageDatabaseTracker : public ThreadSafeRefCounted<LocalStorageDatabaseTracker> {
public:
    static Ref<LocalStorageDatabaseTracker> create(Ref<WorkQueue>&&, const String& localStorageDirectory);
    ~LocalStorageDatabaseTracker();

    String databasePath(const WebCore::SecurityOriginData&) const;

    void didOpenDatabaseWithOrigin(const WebCore::SecurityOriginData&);
    void deleteDatabaseWithOrigin(const WebCore::SecurityOriginData&);
    void deleteAllDatabases();

    // Returns a vector of the origins whose databases should be deleted.
    Vector<WebCore::SecurityOriginData> databasesModifiedSince(WallTime);

    Vector<WebCore::SecurityOriginData> origins() const;

    struct OriginDetails {
        String originIdentifier;
        WallTime creationTime;
        WallTime modificationTime;
    };
    Vector<OriginDetails> originDetails();

private:
    LocalStorageDatabaseTracker(Ref<WorkQueue>&&, const String& localStorageDirectory);

    String databasePath(const String& filename) const;

    enum DatabaseOpeningStrategy {
        CreateIfNonExistent,
        SkipIfNonExistent
    };

    RefPtr<WorkQueue> m_queue;
    String m_localStorageDirectory;

#if PLATFORM(IOS_FAMILY)
    void platformMaybeExcludeFromBackup() const;

    mutable bool m_hasExcludedFromBackup { false };
#endif
};

} // namespace WebKit
