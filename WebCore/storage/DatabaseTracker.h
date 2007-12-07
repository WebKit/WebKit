/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
#ifndef DatabaseTracker_h
#define DatabaseTracker_h

#include "DatabaseDetails.h"
#include "PlatformString.h"
#include "SQLiteDatabase.h"
#include "StringHash.h"
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

class DatabaseTrackerClient;
class Document;
class SecurityOriginData;

struct SecurityOriginDataHash;
struct SecurityOriginDataTraits;

class DatabaseTracker {
public:
    void setDatabasePath(const String&);
    const String& databasePath();

    bool canEstablishDatabase(Document* document, const String& name, const String& displayName, unsigned long estimatedSize);
    void setDatabaseDetails(const SecurityOriginData& origin, const String& name, const String& displayName, unsigned long estimatedSize);
    String fullPathForDatabase(const SecurityOriginData& origin, const String& name, bool createIfNotExists = true);

    void origins(Vector<SecurityOriginData>& result);
    bool databaseNamesForOrigin(const SecurityOriginData& origin, Vector<String>& result);

    DatabaseDetails detailsForNameAndOrigin(const String&, const SecurityOriginData&);
    
    unsigned long long usageForDatabase(const String&, const SecurityOriginData&);
    unsigned long long usageForOrigin(const SecurityOriginData&);
    unsigned long long quotaForOrigin(const SecurityOriginData&);
    void setQuota(const SecurityOriginData&, unsigned long long);
    
    void deleteAllDatabases();
    void deleteDatabasesWithOrigin(const SecurityOriginData& origin);
    void deleteDatabase(const SecurityOriginData& origin, const String& name);

    void setClient(DatabaseTrackerClient*);
    
    void setDefaultOriginQuota(unsigned long long);
    unsigned long long defaultOriginQuota() const;
    
    // From a secondary thread, must be thread safe with its data
    void scheduleNotifyDatabaseChanged(const SecurityOriginData&, const String& name);
    
    static DatabaseTracker& tracker();
private:
    DatabaseTracker();

    void openTrackerDatabase();
    
    bool hasEntryForOrigin(const SecurityOriginData&);
    bool hasEntryForDatabase(const SecurityOriginData&, const String& databaseIdentifier);
    void establishEntryForOrigin(const SecurityOriginData&);
    
    bool addDatabase(const SecurityOriginData& origin, const String& name, const String& path);
    void populateOrigins();
    
    bool deleteDatabaseFile(const SecurityOriginData& origin, const String& name);

    SQLiteDatabase m_database;
    mutable OwnPtr<HashMap<SecurityOriginData, unsigned long long, SecurityOriginDataHash, SecurityOriginDataTraits> > m_originQuotaMap;

    String m_databasePath;
    
    unsigned long long m_defaultQuota;
    DatabaseTrackerClient* m_client;

    static void scheduleForNotification();
    static void notifyDatabasesChanged();
};

} // namespace WebCore

#endif // DatabaseTracker_h
