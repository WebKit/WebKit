/*
 * Copyright (C) 2011, 2013, 2019 Apple Inc. All rights reserved.
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
#include <wtf/Markable.h>
#include <wtf/RefCounted.h>
#include <wtf/WallTime.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class LocalStorageDatabaseTracker : public RefCounted<LocalStorageDatabaseTracker> {
public:
    static Ref<LocalStorageDatabaseTracker> create(String&& localStorageDirectory);
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
        Optional<WallTime> creationTime;
        Optional<WallTime> modificationTime;

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static Optional<OriginDetails> decode(Decoder&);

        OriginDetails isolatedCopy() const { return { originIdentifier.isolatedCopy(), creationTime, modificationTime }; }
    };
    Vector<OriginDetails> originDetailsCrossThreadCopy();

private:
    LocalStorageDatabaseTracker(String&& localStorageDirectory);

    String databasePath(const String& filename) const;
    String localStorageDirectory() const;

    enum DatabaseOpeningStrategy {
        CreateIfNonExistent,
        SkipIfNonExistent
    };
    
    // It is not safe to use this member from a background thread, call localStorageDirectory() instead.
    const String m_localStorageDirectory;

#if PLATFORM(IOS_FAMILY)
    void platformMaybeExcludeFromBackup() const;

    mutable bool m_hasExcludedFromBackup { false };
#endif
};

template<class Encoder>
void LocalStorageDatabaseTracker::OriginDetails::encode(Encoder& encoder) const
{
    encoder << originIdentifier << creationTime << modificationTime;
}

template<class Decoder>
Optional<LocalStorageDatabaseTracker::OriginDetails> LocalStorageDatabaseTracker::OriginDetails::decode(Decoder& decoder)
{
    LocalStorageDatabaseTracker::OriginDetails result;
    if (!decoder.decode(result.originIdentifier))
        return WTF::nullopt;
    
    if (!decoder.decode(result.creationTime))
        return WTF::nullopt;
    
    if (!decoder.decode(result.modificationTime))
        return WTF::nullopt;
    
    return result;
}

} // namespace WebKit
